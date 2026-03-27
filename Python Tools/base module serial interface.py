import tkinter as tk
import tkinter.font as tkfont
from tkinter import ttk, scrolledtext, messagebox, filedialog
import serial, serial.tools.list_ports
import threading, queue, time, datetime
import os, json

BAUD = 115200
NL   = b'\n'
HANDSHAKE_MS = 3000
SPRAY_PIXEL_LIMIT = 25000      # alert threshold (pixels per color)


class HubGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Stepper-Hub Serial Console")
        # make UI text a bit larger for readability
        try:
            tkfont.nametofont("TkDefaultFont").configure(size=11)
            tkfont.nametofont("TkTextFont").configure(size=11)
        except Exception:
            pass

        # debounce state
        self._last_click       = {}

        # spray pixel counting
        self.pixel_counts         = {}   # {color_idx_str: int}
        self.pixel_alerted        = {}   # {color_idx_str: bool}
        self.pixel_indicators     = {}   # {color_idx_str: tk.Button}
        self.color_hex_map        = {}   # {color_idx_str: "#rrggbb"}
        self._stripe_json_pending = False
        self._stripe_json_buffer  = ""
        self._pixel_bar_col       = 1

        # command index tracking
        self._cmd_display_idx = None   # last known index (1-indexed, as Arduino prints)
        self._cmd_canceled    = False  # True when EMERGENCY STOP fired mid-command

        # serial state
        self.ser               = None
        self.rx_q              = queue.Queue()
        self.rx_thr            = None
        self.running           = False
        self.handshake_pending = False

        # tk vars
        self.port_var    = tk.StringVar()
        self.move_a_mode = tk.StringVar(value="to")
        self.move_b_mode = tk.StringVar(value="to")

        # UI
        self.make_topbar()
        self.make_tabs()
        self.make_console()
        self._load_color_map()
        self.make_pixel_toolbar()
        # start global mouse listener (maps side buttons to 'trig')
        self._start_mouse_listener()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(100, self.check_rx_queue)

    # ───────── top bar ─────────
    def make_topbar(self):
        bar = ttk.Frame(self.root, padding=6)
        bar.grid(sticky="ew")
        self.root.columnconfigure(0, weight=1)
        # get ports, always omit COM3 and COM4
        ports = self._list_ports_filtered()
        if ports:
            self.port_var.set(ports[0])
        self.port_cbx = ttk.Combobox(bar, textvariable=self.port_var,
                                     values=ports, width=16, state="readonly")
        self.port_cbx.grid(row=0, column=0, padx=(0, 6))

        ttk.Button(bar, text="Update Ports", width=14,
                   command=self.refresh_ports).grid(row=0, column=1, padx=3)
        ttk.Button(bar, text="Connect", width=12,
                   command=lambda: self._debounced("connect", self.open_port)).grid(row=0, column=2, padx=3)
        ttk.Button(bar, text="Reset Port", width=12,
                   command=lambda: self._debounced("reset_port", self.reset_port)).grid(row=0, column=3, padx=3)
        ttk.Button(bar, text="Save Log", width=12,
                   command=self.save_log).grid(row=0, column=4, padx=3)

        self.status_lbl = ttk.Label(bar, text="closed")
        self.status_lbl.grid(row=0, column=5, padx=8)
        bar.columnconfigure(6, weight=1)

    def refresh_ports(self):
        current = self.port_var.get()
        ports = self._list_ports_filtered()
        self.port_cbx["values"] = ports
        if current in ports:
            self.port_var.set(current)
        elif ports:
            self.port_var.set(ports[0])
        else:
            self.port_var.set("")

    # ───────── tabs ─────────
    def make_tabs(self):
        nb = ttk.Notebook(self.root)
        nb.grid(sticky="ew", padx=6)

        self.tab_hub = ttk.Frame(nb, padding=(6, 6, 6, 6))
        self.tab_relay = ttk.Frame(nb, padding=(6, 6, 6, 6))
        nb.add(self.tab_hub, text="Hub Commands")
        nb.add(self.tab_relay, text="Relayed Commands")

        self.build_hub_tab(self.tab_hub)
        self.build_relay_tab(self.tab_relay)

    # ───────── hub tab (base module) ─────────
    def build_hub_tab(self, p):
        for i in range(6):
            p.columnconfigure(i, weight=1)

        row = 0
        ttk.Label(p, text="Command").grid(row=row, column=0, sticky="e")
        self.cmd_entry = ttk.Entry(p)
        self.cmd_entry.grid(row=row, column=1, columnspan=3, sticky="ew", padx=(4, 4))
        ttk.Button(p, text="Send", width=14,
                   command=self.send_from_entry).grid(row=row, column=4, sticky="w")
        self.cmd_entry.bind("<Return>", lambda e: self.send_from_entry())
        row += 1

        def simple_btn(text):
            nonlocal row
            ttk.Button(p, text=text, width=14,
                       command=lambda t=text: self._debounced(t, lambda: self.send(t))).grid(row=row, column=0, sticky="w", pady=2)
            row += 1

        simple_btn("go")
        simple_btn("run")

        ttk.Label(p, text="move A").grid(row=row, column=0, sticky="e")
        ttk.Radiobutton(p, text="to", variable=self.move_a_mode, value="to").grid(row=row, column=1, sticky="w")
        ttk.Radiobutton(p, text="by", variable=self.move_a_mode, value="by").grid(row=row, column=2, sticky="w")
        self.move_a_entry = ttk.Entry(p, width=8); self.move_a_entry.grid(row=row, column=3, sticky="w")
        ttk.Button(p, text="Send", command=lambda: self._debounced("move_a", self.send_move_a)).grid(row=row, column=4, sticky="w", padx=4); row += 1

        ttk.Label(p, text="move B").grid(row=row, column=0, sticky="e")
        ttk.Radiobutton(p, text="to", variable=self.move_b_mode, value="to").grid(row=row, column=1, sticky="w")
        ttk.Radiobutton(p, text="by", variable=self.move_b_mode, value="by").grid(row=row, column=2, sticky="w")
        self.move_b_entry = ttk.Entry(p, width=8); self.move_b_entry.grid(row=row, column=3, sticky="w")
        ttk.Button(p, text="Send", command=lambda: self._debounced("move_b", self.send_move_b)).grid(row=row, column=4, sticky="w", padx=4); row += 1

        ttk.Label(p, text="set A to").grid(row=row, column=0, sticky="e")
        self.set_a_entry = ttk.Entry(p, width=8); self.set_a_entry.grid(row=row, column=1, sticky="w")
        ttk.Button(p, text="Set",
                   command=lambda: self.send(f"set a to {self.set_a_entry.get()}")).grid(row=row, column=2, sticky="w")
        ttk.Label(p, text="set B to").grid(row=row, column=3, sticky="e")
        self.set_b_entry = ttk.Entry(p, width=8); self.set_b_entry.grid(row=row, column=4, sticky="w")
        ttk.Button(p, text="Set",
                   command=lambda: self.send(f"set b to {self.set_b_entry.get()}")).grid(row=row, column=5, sticky="w"); row += 1

        ttk.Button(p, text="zero A", width=12,
                   command=lambda: self.send("zero a")).grid(row=row, column=0, sticky="w", pady=(2, 6))
        ttk.Button(p, text="zero B", width=12,
                   command=lambda: self.send("zero b")).grid(row=row, column=1, sticky="w", pady=(2, 6))
        row += 1

        def num_cmd(label, cmd_template):
            nonlocal row
            ttk.Label(p, text=label).grid(row=row, column=0, sticky="e")
            e = ttk.Entry(p, width=8); e.grid(row=row, column=1, sticky="w")
            ttk.Button(p, text="Set",
                       command=lambda t=cmd_template, ref=e: self.send(t.format(ref.get()))
                       ).grid(row=row, column=2, sticky="w"); row += 1
            return e

        self.accel_entry  = num_cmd("accel mult", "acceleration multiplier {}")
        self.vel_entry    = num_cmd("vel mult",   "velocity multiplier {}")
        # stripe velocity multiplier replaces the old direct set command
        self.stripe_mult  = num_cmd("stripe vel mult", "stripe velocity multiplier {}")
        self.spr_entry    = num_cmd("SPR",        "spr {}")
        self.vel_calc     = num_cmd("vel calc ms","set velocity calc delay {}")
        self.conf_timeout = num_cmd("confirm ms", "set confirmation timeout {}")
        self.pre_poke     = num_cmd("pre poke ms","pre poke pause {}")
        self.wait_time    = num_cmd("chassis wait","chasseyWaitTime {}")
        self.cmd_index    = num_cmd("cmd index", "set command index {}")

        ttk.Button(p, text="skip color", width=14,
                   command=lambda: self.send("skip color")).grid(row=row, column=0, sticky="w", pady=2)
        ttk.Button(p, text="reset run", width=12,
                   command=lambda: self.send("reset run")).grid(row=row, column=1, sticky="w", pady=2)
        ttk.Button(p, text="test", width=14,
                   command=lambda: self.send("test")).grid(row=row, column=2, sticky="w", pady=2)
        ttk.Button(p, text="4 corners", width=14,
                   command=lambda: self.send("4 corners")).grid(row=row, column=3, sticky="w", pady=2)
        row += 1
        tk.Button(p, text="restart (ESP32)", fg="white", bg="#c0392b", width=17,
                  command=lambda: self.send("restart")).grid(row=row, column=0, sticky="w", pady=2)
        ttk.Button(p, text="?", width=6,
                   command=lambda: self.send("?")).grid(row=row, column=1, sticky="w", pady=2)
        # test and 4 corners are not here; moved to relayed tab

    # ───────── relayed tab (SprayGUI set + test/4 corners) ─────────
    def build_relay_tab(self, f):
        for i in range(7):
            f.columnconfigure(i, weight=1)

        r = 0

        # quick buttons: clean, forever, ?
        c = 0
        def rbtn(label, cmd):
            nonlocal r, c
            ttk.Button(f, text=label, width=14, command=cmd).grid(row=r, column=c, pady=2, sticky="w")
            c = (c + 1) % 6
            if c == 0:
                r += 1
        rbtn("clean", lambda: self._debounced("clean", self.do_clean))
        for lbl in ("forever", "?"):
            rbtn(lbl, lambda s=lbl: self.send_relay(s))
        if c != 0:
            r += 1
            c = 0

        # pulse-width ms (just the number)
        ttk.Label(f, text="pulse-width ms").grid(row=r, column=0, sticky="e", padx=2)
        self.pw_entry = ttk.Entry(f, width=8); self.pw_entry.grid(row=r, column=1, sticky="w")
        ttk.Button(f, text="Set", width=14,
                   command=lambda: self.send_relay(self.pw_entry.get().strip())).grid(row=r, column=2, sticky="w")
        r += 1

        # delay ms
        ttk.Label(f, text="delay ms").grid(row=r, column=0, sticky="e", padx=2)
        self.delay_entry = ttk.Entry(f, width=8); self.delay_entry.grid(row=r, column=1, sticky="w")
        ttk.Button(f, text="Set", width=14,
                   command=lambda: self.send_relay(f"delay {self.delay_entry.get().strip()}")).grid(row=r, column=2, sticky="w")
        r += 1

        # trig single - label + entry + send
        ttk.Label(f, text="trig").grid(row=r, column=0, sticky="e", padx=2)
        self.trig_single = ttk.Entry(f, width=8); self.trig_single.grid(row=r, column=1, sticky="w")
        ttk.Button(f, text="Send", width=14,
               command=lambda: self._debounced("trig_single", lambda: self.send_relay(f"trig {self.trig_single.get().strip()}"))).grid(row=r, column=2, sticky="w")
        r += 1

        # trig s,c,d - labels
        ttk.Label(f, text="trig").grid(row=r, column=0, sticky="e", padx=2)
        ttk.Label(f, text="S", foreground="grey", font=("TkDefaultFont", 9)).grid(row=r, column=1, sticky="w")
        ttk.Label(f, text="C", foreground="grey", font=("TkDefaultFont", 9)).grid(row=r, column=2, sticky="w")
        ttk.Label(f, text="D", foreground="grey", font=("TkDefaultFont", 9)).grid(row=r, column=3, sticky="w")
        r += 1
        self.trig_s = ttk.Entry(f, width=4); self.trig_c = ttk.Entry(f, width=4); self.trig_d = ttk.Entry(f, width=4)
        self.trig_s.grid(row=r, column=1); self.trig_c.grid(row=r, column=2); self.trig_d.grid(row=r, column=3)
        ttk.Button(f, text="Send", width=14,
                   command=lambda: self._debounced("trig_scd", lambda: self.send_relay(f"trig {self.trig_s.get().strip()},{self.trig_c.get().strip()},{self.trig_d.get().strip()}"))).grid(row=r, column=4, sticky="w")
        r += 1

        # calibration sol,low,high,step - labels
        ttk.Label(f, text="calibration").grid(row=r, column=0, sticky="e", padx=2)
        ttk.Label(f, text="sol", foreground="grey", font=("TkDefaultFont", 9)).grid(row=r, column=1, sticky="w")
        ttk.Label(f, text="low", foreground="grey", font=("TkDefaultFont", 9)).grid(row=r, column=2, sticky="w")
        ttk.Label(f, text="high", foreground="grey", font=("TkDefaultFont", 9)).grid(row=r, column=3, sticky="w")
        ttk.Label(f, text="step", foreground="grey", font=("TkDefaultFont", 9)).grid(row=r, column=4, sticky="w")
        r += 1
        self.cal_sol  = ttk.Entry(f, width=4)
        self.cal_low  = ttk.Entry(f, width=6)
        self.cal_high = ttk.Entry(f, width=6)
        self.cal_step = ttk.Entry(f, width=6)
        self.cal_sol.grid( row=r, column=1)
        self.cal_low.grid( row=r, column=2)
        self.cal_high.grid(row=r, column=3)
        self.cal_step.grid(row=r, column=4)
        ttk.Button(f, text="Run", width=14,
                   command=lambda: self.send_relay(
                       f"calibration {self.cal_sol.get().strip()},{self.cal_low.get().strip()},"
                       f"{self.cal_high.get().strip()},{self.cal_step.get().strip()}")).grid(row=r, column=5, sticky="w")
        r += 1

        # heater control
        ttk.Label(f, text="heater").grid(row=r, column=0, sticky="e", padx=2)
        self.heater_select = ttk.Combobox(f, values=["1", "2", "both"], width=6, state="readonly")
        self.heater_select.set("both")
        self.heater_select.grid(row=r, column=1, sticky="w")
        ttk.Label(f, text="%").grid(row=r, column=2, sticky="e", padx=(4, 2))
        self.heater_entry = ttk.Entry(f, width=4); self.heater_entry.grid(row=r, column=3, sticky="w")
        ttk.Button(f, text="Set", width=14,
                   command=lambda: self.send_relay(f"heater {self.heater_select.get()} {self.heater_entry.get().strip()}%")).grid(row=r, column=4, sticky="w")
        r += 1

        # solenoid direct control (compact, generalized)
        ttk.Label(f, text="solenoid").grid(row=r, column=0, sticky="e", padx=2)
        self.sol_num = ttk.Entry(f, width=6)
        self.sol_num.grid(row=r, column=1, sticky="w")
        self.sol_val = ttk.Entry(f, width=8)
        self.sol_val.grid(row=r, column=2, sticky="w")
        ttk.Button(f, text="Send", width=14,
                   command=self.send_solenoid_generic).grid(row=r, column=3, sticky="w")
        # allow Enter to send from either field
        self.sol_num.bind("<Return>", lambda e: self.send_solenoid_generic())
        self.sol_val.bind("<Return>", lambda e: self.send_solenoid_generic())
        r += 1

        # moved hub buttons
        ttk.Separator(f, orient="horizontal").grid(row=r, column=0, columnspan=7, sticky="ew", pady=(6, 6))
        r += 1

        # optional free-form relay box for ad-hoc chassis commands
        r += 1
        ttk.Label(f, text="free-form relay").grid(row=r, column=0, sticky="e")
        self.relay_entry = ttk.Entry(f)
        self.relay_entry.grid(row=r, column=1, columnspan=4, sticky="ew", padx=(4, 4))
        ttk.Button(f, text="Send", width=14,
                   command=self.send_relay_from_entry).grid(row=r, column=5, sticky="w")
        self.relay_entry.bind("<Return>", lambda e: self.send_relay_from_entry())

    # ───────── console ─────────
    def make_console(self):
        self.console = scrolledtext.ScrolledText(self.root, height=14, wrap=tk.WORD, state="disabled")
        self.console.grid(sticky="nsew", padx=6, pady=(0, 6))
        self.root.rowconfigure(3, weight=1)

    def _list_ports_filtered(self):
        """Return list of available serial port device names excluding COM3 and COM4."""
        try:
            ports = [p.device for p in serial.tools.list_ports.comports()]
        except Exception:
            return []
        return [p for p in ports if p.upper() not in ("COM3", "COM4")]

    # ───────── serial helpers ─────────
    def open_port(self):
        port = self.port_var.get()
        if not port:
            messagebox.showerror("No port", "Select a COM port first")
            return
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.1)
        except serial.SerialException as e:
            messagebox.showerror("Serial error", str(e))
            return

        self.status_lbl.config(text="waiting...")
        self.running = True
        self.handshake_pending = True
        self.rx_thr = threading.Thread(target=self.read_thread, daemon=True)
        self.rx_thr.start()
        self.send("?")
        self.root.after(HANDSHAKE_MS, self.check_handshake_timeout)

    def reset_port(self):
        if not self.ser:
            return
        port = self.ser.port
        self.close_port()
        time.sleep(0.5)
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.1)
        except serial.SerialException as e:
            messagebox.showerror("Serial error", str(e))
            return
        self.status_lbl.config(text="waiting...")
        self.running = True
        self.handshake_pending = True
        self.rx_thr = threading.Thread(target=self.read_thread, daemon=True)
        self.rx_thr.start()
        self.send("?")
        self.root.after(HANDSHAKE_MS, self.check_handshake_timeout)

    def check_handshake_timeout(self):
        if self.handshake_pending:
            self.status_lbl.config(text="port not opened")
            self.close_port()

    def close_port(self):
        self.running = False
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except serial.SerialException:
                pass
        self.status_lbl.config(text="closed")

    def read_thread(self):
        while self.running and self.ser and self.ser.is_open:
            try:
                line = self.ser.readline()
                if line:
                    if self.handshake_pending:
                        self.handshake_pending = False
                        self.rx_q.put("__CONNECTED__\n")
                    self.rx_q.put(line.decode(errors="replace"))
            except serial.SerialException:
                self.rx_q.put("[Serial error]\n")
                self.running = False
        self.rx_q.put("[Port closed]\n")

    def check_rx_queue(self):
        try:
            while True:
                line = self.rx_q.get_nowait()
                if line == "__CONNECTED__\n":
                    self.status_lbl.config(text="connected")
                    continue
                stripped = line.strip()
                # stripe pixel counting
                if stripped == "Generated JSON Data for STRIPE:":
                    self._stripe_json_pending = True
                    self._stripe_json_buffer  = ""
                elif self._stripe_json_pending:
                    self._stripe_json_buffer += stripped
                    if self._stripe_json_buffer.endswith("}"):
                        self._stripe_json_pending = False
                        self._process_stripe_json(self._stripe_json_buffer)
                        self._stripe_json_buffer = ""
                # command index tracking
                self._parse_cmd_index_line(stripped)
                self.console.configure(state="normal")
                self.console.insert(tk.END, line)
                self.console.see(tk.END)
                self.console.configure(state="disabled")
        except queue.Empty:
            pass
        self.root.after(100, self.check_rx_queue)

    # ───────── helpers ─────────
    def _debounced(self, key, fn, delay=1.0):
        now = time.time()
        if now - self._last_click.get(key, 0) < delay:
            return
        self._last_click[key] = now
        fn()

    def send(self, text):
        text = text.strip()
        if not text:
            return
        if not self.ser or not self.ser.is_open:
            self.log(f"[not connected] {text}\n")
            return
        try:
            self.ser.write(text.encode() + NL)
            self.log("> " + text + "\n")
        except serial.SerialException as e:
            messagebox.showerror("Write error", str(e))

    def send_from_entry(self):
        txt = self.cmd_entry.get().strip()
        if not txt:
            return
        self.send(txt)
        self.cmd_entry.delete(0, tk.END)

    def send_relay(self, payload):
        payload = payload.strip()
        if not payload:
            return
        self.send(f"relayed command: {payload}")

    def send_relay_from_entry(self):
        txt = self.relay_entry.get().strip()
        if not txt:
            return
        self.send_relay(txt)
        self.relay_entry.delete(0, tk.END)

    def log(self, msg):
        self.console.configure(state="normal")
        self.console.insert(tk.END, msg)
        self.console.see(tk.END)
        self.console.configure(state="disabled")

    def save_log(self):
        log_content = self.console.get("1.0", tk.END)
        if not log_content.strip():
            messagebox.showinfo("Save log", "Console is empty")
            return
        default_name = f"stepper_log_{datetime.datetime.now():%Y%m%d_%H%M%S}.txt"
        path = filedialog.asksaveasfilename(defaultextension=".txt",
                                            initialfile=default_name,
                                            filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(log_content)
            messagebox.showinfo("Save log", f"Log saved to {path}")

    def _confirm_close_to_pulley(self, val_str):
        """Return True if safe to proceed (either value > 1.5 or user confirmed)."""
        try:
            if float(val_str) > 1.5:
                return True
        except ValueError:
            return True  # non-numeric, let the device handle it
        win = tk.Toplevel(self.root)
        win.title("Warning")
        win.grab_set()
        win.resizable(False, False)
        confirmed = [False]
        tk.Label(win, text="Are you sure you want to move here?\nThis is very close to the pulley\nand risks overloading cable tension.",
                 font=("TkDefaultFont", 11), pady=12, padx=16, justify="center").pack()
        btn_frame = tk.Frame(win); btn_frame.pack(pady=(0, 12))
        tk.Button(btn_frame, text="Cancel", width=12,
                  command=win.destroy).pack(side="left", padx=8)
        def _continue():
            confirmed[0] = True
            win.destroy()
        tk.Button(btn_frame, text="Continue", width=12, bg="#c0392b", fg="white",
                  activebackground="#922b21", activeforeground="white",
                  command=_continue).pack(side="left", padx=8)
        win.wait_window()
        return confirmed[0]

    def send_move_a(self):
        val = self.move_a_entry.get().strip()
        if not val:
            return
        if self.move_a_mode.get() == "to":
            if not self._confirm_close_to_pulley(val):
                return
            cmd = f"move a to {val}"
        else:
            cmd = f"move a {val}"
        self.send(cmd)

    def send_move_b(self):
        val = self.move_b_entry.get().strip()
        if not val:
            return
        if self.move_b_mode.get() == "to":
            if not self._confirm_close_to_pulley(val):
                return
            cmd = f"move b to {val}"
        else:
            cmd = f"move b {val}"
        self.send(cmd)

    def send_solenoid(self, sol, entry):
        """Send a solenoid relay command for solenoid number `sol` using value from `entry`."""
        val = entry.get().strip()
        if not val:
            return
        self.send_relay(f"solenoid {sol} {val}")

    def send_solenoid_generic(self):
        """Send a solenoid relay command using the compact solenoid number/value fields."""
        num = self.sol_num.get().strip()
        val = self.sol_val.get().strip()
        if not num or not val:
            return
        self.send_relay(f"solenoid {num} {val}")

    def do_clean(self):
        """Set pulse width to 5ms, fire trig 10 times at 2s intervals, then restore."""
        saved_pw = self.pw_entry.get().strip()
        self.send_relay("5")
        self.pw_entry.delete(0, tk.END)
        self.pw_entry.insert(0, "5")

        TOTAL = 10
        INTERVAL_MS = 2000

        def fire(remaining):
            self.send_relay("trig")
            if remaining > 1:
                self.root.after(INTERVAL_MS, lambda: fire(remaining - 1))
            else:
                # restore original pulse width
                if saved_pw:
                    self.send_relay(saved_pw)
                    self.pw_entry.delete(0, tk.END)
                    self.pw_entry.insert(0, saved_pw)

        self.root.after(INTERVAL_MS, lambda: fire(TOTAL))

    def on_close(self):
        # stop mouse listener if running
        try:
            if hasattr(self, "_mouse_listener") and self._mouse_listener:
                try:
                    self._mouse_listener.stop()
                except Exception:
                    pass
        except Exception:
            pass
        self.close_port()
        self.root.destroy()

    def _start_mouse_listener(self):
        """Start a background pynput mouse listener mapping side buttons to 'trig'."""
        try:
            from pynput.mouse import Listener, Button
        except Exception:
            # pynput not installed; skip silently (user can install to enable)
            return

        def _on_click(x, y, button, pressed):
            if pressed and button in (Button.x1, Button.x2):
                try:
                    self._debounced("mouse_trig", lambda: self.send_relay("trig"))
                except Exception:
                    pass

        self._mouse_listener = Listener(on_click=_on_click)
        self._mouse_listener.daemon = True
        self._mouse_listener.start()


    # ───────── spray pixel counting ─────────
    def _load_color_map(self):
        """Parse color index→hex from gcode.txt (../mural/gcode.txt relative to this script)."""
        try:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            gcode_path = os.path.normpath(os.path.join(script_dir, "..", "mural", "gcode.txt"))
            with open(gcode_path, "r", encoding="utf-8") as f:
                in_section = False
                for raw in f:
                    ln = raw.strip()
                    if ln == "-- MULTI-COLOR INDEX MAPPING --":
                        in_section = True
                        continue
                    if ln == "-- END OF COLOR MAPPING --":
                        break
                    if in_section and ln.startswith("Index ") and "=>" in ln:
                        parts = ln.split("=>")
                        idx     = parts[0].replace("Index", "").strip()
                        hex_val = parts[1].strip()
                        self.color_hex_map[idx] = hex_val
        except Exception:
            pass

    def make_pixel_toolbar(self):
        """Persistent bottom toolbar: command index indicator + per-color spray pixel counts."""
        self.root.rowconfigure(2, weight=1)   # console row expands → toolbar always pinned
        self.root.rowconfigure(3, weight=0)   # toolbar row fixed height
        self._pixel_bar = ttk.Frame(self.root, padding=(6, 4))
        self._pixel_bar.grid(row=3, sticky="ew", padx=6, pady=(2, 6))

        # ── command index section ──
        idx_frame = ttk.Frame(self._pixel_bar)
        idx_frame.grid(row=0, column=0, padx=(0, 2), sticky="ns")

        ttk.Label(idx_frame, text="cmd idx").grid(row=0, column=0, columnspan=2, sticky="w")

        self._cmd_idx_label = tk.Label(idx_frame, text="?",
                                       font=tkfont.Font(size=20, weight="bold"),
                                       width=4, anchor="center")
        self._cmd_idx_label.grid(row=1, column=0, sticky="w")
        self._cmd_idx_label.bind("<Button-1>", lambda _: self._clear_cmd_canceled())

        self._cmd_idx_plus_btn = ttk.Button(idx_frame, text="+1", width=3,
                                            command=self._cmd_progress_by_1,
                                            state="disabled")
        self._cmd_idx_plus_btn.grid(row=1, column=1, padx=(4, 0), sticky="s")

        self._cmd_canceled_label = tk.Label(idx_frame, text="",
                                            fg="#e74c3c",
                                            font=tkfont.Font(size=9, weight="bold"))
        self._cmd_canceled_label.grid(row=2, column=0, columnspan=2, sticky="w")

        # separator between cmd idx and spray px sections
        ttk.Separator(self._pixel_bar, orient="vertical").grid(
            row=0, column=1, sticky="ns", padx=(6, 8))

        # ── spray pixel section ──
        ttk.Label(self._pixel_bar, text="Spray px:").grid(row=0, column=2, padx=(0, 8))
        self._pixel_bar_col = 3

    def _get_or_create_indicator(self, color_idx):
        """Return indicator button for color_idx, creating it on first call."""
        if color_idx in self.pixel_indicators:
            return self.pixel_indicators[color_idx]
        hex_str  = self.color_hex_map.get(color_idx, "")
        top_line = f"{color_idx}  {hex_str}" if hex_str else f"Color {color_idx}"
        btn = tk.Button(self._pixel_bar,
                        text=f"{top_line}\n0",
                        relief="raised", padx=8, pady=4,
                        justify="center",
                        command=lambda c=color_idx: self._reset_pixel_count(c))
        btn.grid(row=0, column=self._pixel_bar_col, padx=4)
        btn._default_bg  = btn.cget("bg")
        btn._default_fg  = btn.cget("fg")
        btn._default_abg = btn.cget("activebackground")
        btn._default_afg = btn.cget("activeforeground")
        self._pixel_bar_col += 1
        self.pixel_indicators[color_idx] = btn
        self.pixel_counts[color_idx]     = 0
        self.pixel_alerted[color_idx]    = False
        return btn

    def _process_stripe_json(self, json_str):
        """Count per-color pixels in a stripe JSON and update indicators."""
        try:
            data    = json.loads(json_str)
            pattern = data.get("pattern", [])
            tally   = {}
            for row in pattern:
                for ch in row:
                    if ch.isdigit() and ch != "0":
                        tally[ch] = tally.get(ch, 0) + 1
            for color_idx, count in tally.items():
                self._get_or_create_indicator(color_idx)
                self.pixel_counts[color_idx] += count
                self._update_indicator(color_idx)
        except Exception:
            pass

    def _update_indicator(self, color_idx):
        """Refresh button label and color; play chime if threshold just crossed."""
        btn      = self.pixel_indicators[color_idx]
        count    = self.pixel_counts[color_idx]
        hex_str  = self.color_hex_map.get(color_idx, "")
        top_line = f"{color_idx}  {hex_str}" if hex_str else f"Color {color_idx}"
        btn.config(text=f"{top_line}\n{count:,}")
        was_alerted = self.pixel_alerted[color_idx]
        now_alerted = count >= SPRAY_PIXEL_LIMIT
        if now_alerted:
            btn.config(bg="#e74c3c", fg="white",
                       activebackground="#c0392b", activeforeground="white")
            if not was_alerted:
                self.pixel_alerted[color_idx] = True
                self._play_chime()
        else:
            btn.config(bg=btn._default_bg,  fg=btn._default_fg,
                       activebackground=btn._default_abg, activeforeground=btn._default_afg)

    def _reset_pixel_count(self, color_idx):
        """Reset count and alert state for one color (called when user clicks indicator)."""
        self.pixel_counts[color_idx]  = 0
        self.pixel_alerted[color_idx] = False
        self._update_indicator(color_idx)

    def _parse_cmd_index_line(self, stripped):
        """Update command index state from a single stripped serial line."""
        if stripped.startswith("Executing STRIPE command #") or \
           stripped.startswith("Starting MOVE command #"):
            try:
                n = int(stripped.rsplit("#", 1)[1])
                self._cmd_display_idx = n
                self._cmd_canceled    = False
                self._update_cmd_display()
            except (IndexError, ValueError):
                pass
        elif stripped == "EMERGENCY STOP":
            if self._cmd_display_idx is not None:
                self._cmd_canceled = True
                self._update_cmd_display()
        elif stripped.startswith("Command index set to: "):
            try:
                n = int(stripped.split(": ", 1)[1])
                self._cmd_display_idx = n
                self._cmd_canceled    = False
                self._update_cmd_display()
            except (IndexError, ValueError):
                pass
        elif stripped.startswith("Run index reset"):
            self._cmd_display_idx = 1
            self._cmd_canceled    = False
            self._update_cmd_display()

    def _update_cmd_display(self):
        """Refresh the command index label, canceled text, and +1 button state."""
        if self._cmd_display_idx is None:
            self._cmd_idx_label.config(text="?", fg="black")
            self._cmd_canceled_label.config(text="")
            self._cmd_idx_plus_btn.config(state="disabled")
        else:
            self._cmd_idx_label.config(text=str(self._cmd_display_idx),
                                       fg="#e74c3c" if self._cmd_canceled else "black")
            self._cmd_canceled_label.config(
                text="canceled" if self._cmd_canceled else "")
            self._cmd_idx_plus_btn.config(state="normal")

    def _cmd_progress_by_1(self):
        """Send 'set command index N+1' where N is the currently displayed index."""
        if self._cmd_display_idx is None:
            return
        self.send(f"set command index {self._cmd_display_idx + 1}")

    def _clear_cmd_canceled(self):
        """Clear the canceled flag when user clicks the index label."""
        self._cmd_canceled = False
        self._update_cmd_display()

    def _play_chime(self):
        """Play a 3-second alert beep in a background thread (non-blocking)."""
        def _beep():
            try:
                import winsound
                winsound.Beep(1000, 3000)
            except Exception:
                pass
        threading.Thread(target=_beep, daemon=True).start()


if __name__ == "__main__":
    tk_root = tk.Tk()
    HubGUI(tk_root)
    tk_root.mainloop()
