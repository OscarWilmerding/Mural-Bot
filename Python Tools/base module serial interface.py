import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
import serial, serial.tools.list_ports
import threading, queue, time, datetime

BAUD = 115200
NL   = b'\n'               # newline sent after every command
HANDSHAKE_MS = 3000        # give the port 3s to respond, then abort


class HubGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Stepper-Hub Serial Console")

        # serial state
        self.ser                = None
        self.rx_q               = queue.Queue()
        self.rx_thr             = None
        self.running            = False
        self.handshake_pending  = False

        # tk vars
        self.port_var  = tk.StringVar()
        self.move_a_mode = tk.StringVar(value="to")
        self.move_b_mode = tk.StringVar(value="to")

        # build UI
        self.make_topbar()
        self.make_controls()
        self.make_console()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(100, self.check_rx_queue)

    # ───────────── top bar ─────────────
    def make_topbar(self):
        bar = ttk.Frame(self.root, padding=6)
        bar.grid(sticky="ew")
        self.root.columnconfigure(0, weight=1)

        ports = [p.device for p in serial.tools.list_ports.comports()]
        if ports:
            self.port_var.set(ports[0])
        self.port_cbx = ttk.Combobox(
            bar, textvariable=self.port_var,
            values=ports, width=16, state="readonly")
        self.port_cbx.grid(row=0, column=0, padx=(0, 6))

        ttk.Button(bar, text="Update Ports", width=12,
                   command=self.refresh_ports).grid(row=0, column=1, padx=3)

        ttk.Button(bar, text="Connect", width=10,
                   command=self.open_port).grid(row=0, column=2, padx=3)
        ttk.Button(bar, text="Reset Port", width=10,
                   command=self.reset_port).grid(row=0, column=3, padx=3)
        ttk.Button(bar, text="Save Log", width=10,
                   command=self.save_log).grid(row=0, column=4, padx=3)

        self.status_lbl = ttk.Label(bar, text="closed")
        self.status_lbl.grid(row=0, column=5, padx=8)
        bar.columnconfigure(6, weight=1)

    def refresh_ports(self):
        """Rescan and repopulate the COM port list while preserving selection when possible."""
        current = self.port_var.get()
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_cbx["values"] = ports
        if current in ports:
            self.port_var.set(current)
        elif ports:
            self.port_var.set(ports[0])
        else:
            self.port_var.set("")

    # ───────────── command panel ─────────────
    def make_controls(self):
        p = ttk.Frame(self.root, padding=(6, 0, 6, 6))
        p.grid(sticky="ew")
        for i in range(6):
            p.columnconfigure(i, weight=1)

        row = 0

        # free form command
        ttk.Label(p, text="Command").grid(row=row, column=0, sticky="e")
        self.cmd_entry = ttk.Entry(p)
        self.cmd_entry.grid(row=row, column=1, columnspan=3, sticky="ew", padx=(4, 4))
        ttk.Button(p, text="Send", width=10,
                   command=self.send_from_entry).grid(row=row, column=4, sticky="w")
        self.cmd_entry.bind("<Return>", lambda e: self.send_from_entry())
        row += 1

        # row helper
        def simple_btn(text):
            nonlocal row
            ttk.Button(p, text=text, width=12,
                       command=lambda t=text: self.send(t)).grid(row=row, column=0, sticky="w", pady=2)
            row += 1

        # basic run control
        simple_btn("go")
        simple_btn("run")

        # movement – motor A
        ttk.Label(p, text="move A").grid(row=row, column=0, sticky="e")
        ttk.Radiobutton(p, text="to",   variable=self.move_a_mode, value="to").grid(row=row, column=1, sticky="w")
        ttk.Radiobutton(p, text="by",   variable=self.move_a_mode, value="by").grid(row=row, column=2, sticky="w")
        self.move_a_entry = ttk.Entry(p, width=8); self.move_a_entry.grid(row=row, column=3, sticky="w")
        ttk.Button(p, text="Send",
                   command=self.send_move_a).grid(row=row, column=4, sticky="w", padx=4); row += 1

        # movement – motor B
        ttk.Label(p, text="move B").grid(row=row, column=0, sticky="e")
        ttk.Radiobutton(p, text="to", variable=self.move_b_mode, value="to").grid(row=row, column=1, sticky="w")
        ttk.Radiobutton(p, text="by", variable=self.move_b_mode, value="by").grid(row=row, column=2, sticky="w")
        self.move_b_entry = ttk.Entry(p, width=8); self.move_b_entry.grid(row=row, column=3, sticky="w")
        ttk.Button(p, text="Send",
                   command=self.send_move_b).grid(row=row, column=4, sticky="w", padx=4); row += 1

        # set positions
        ttk.Label(p, text="set A to").grid(row=row, column=0, sticky="e")
        self.set_a_entry = ttk.Entry(p, width=8); self.set_a_entry.grid(row=row, column=1, sticky="w")
        ttk.Button(p, text="Set",
                   command=lambda: self.send(f"set a to {self.set_a_entry.get()}")).grid(row=row, column=2, sticky="w")
        ttk.Label(p, text="set B to").grid(row=row, column=3, sticky="e")
        self.set_b_entry = ttk.Entry(p, width=8); self.set_b_entry.grid(row=row, column=4, sticky="w")
        ttk.Button(p, text="Set",
                   command=lambda: self.send(f"set b to {self.set_b_entry.get()}")).grid(row=row, column=5, sticky="w"); row += 1

        # zero buttons
        ttk.Button(p, text="zero A", width=10,
                   command=lambda: self.send("zero a")).grid(row=row, column=0, sticky="w", pady=(2, 6))
        ttk.Button(p, text="zero B", width=10,
                   command=lambda: self.send("zero b")).grid(row=row, column=1, sticky="w", pady=(2, 6))
        row += 1

        # numeric-value helpers (label, entry, button)
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
        self.spr_entry    = num_cmd("SPR",        "spr {}")
        self.stripe_v     = num_cmd("stripe vel", "set stripe velocity {}")
        self.vel_calc     = num_cmd("vel calc ms","set velocity calc delay {}")
        self.conf_timeout = num_cmd("confirm ms", "set confirmation timeout {}")
        self.pre_poke     = num_cmd("pre poke ms","pre poke pause {}")
        self.wait_time    = num_cmd("chassis wait","chasseyWaitTime {}")
        self.cmd_index    = num_cmd("cmd index", "set command index {}")

        # misc buttons
        ttk.Button(p, text="skip color", width=12,
                   command=lambda: self.send("skip color")).grid(row=row, column=0, sticky="w", pady=2)
        ttk.Button(p, text="test", width=8,
                   command=lambda: self.send("test")).grid(row=row, column=1, sticky="w", pady=2)
        ttk.Button(p, text="reset run", width=10,
                   command=lambda: self.send("reset run")).grid(row=row, column=2, sticky="w", pady=2)
        tk.Button(p, text="restart (ESP32)", fg="white", bg="#c0392b", width=15,
                  command=lambda: self.send("restart")).grid(row=row, column=3, sticky="w", pady=2)
        ttk.Button(p, text="?", width=4,
                   command=lambda: self.send("?")).grid(row=row, column=4, sticky="w", pady=2)

    # ───────────── console ─────────────
    def make_console(self):
        self.console = scrolledtext.ScrolledText(
            self.root, height=14, wrap=tk.WORD, state="disabled")
        self.console.grid(sticky="nsew", padx=6, pady=(0, 6))
        self.root.rowconfigure(2, weight=1)

    # ───────────── serial helpers ─────────────
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
        self.send("?")  # initial poke
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
                self.console.configure(state="normal")
                self.console.insert(tk.END, line)
                self.console.see(tk.END)
                self.console.configure(state="disabled")
        except queue.Empty:
            pass
        self.root.after(100, self.check_rx_queue)

    # ───────────── helpers ─────────────
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

    # move helpers
    def send_move_a(self):
        val = self.move_a_entry.get().strip()
        if not val:
            return
        cmd = f"move a to {val}" if self.move_a_mode.get() == "to" else f"move a {val}"
        self.send(cmd)

    def send_move_b(self):
        val = self.move_b_entry.get().strip()
        if not val:
            return
        cmd = f"move b to {val}" if self.move_b_mode.get() == "to" else f"move b {val}"
        self.send(cmd)

    def on_close(self):
        self.close_port()
        self.root.destroy()


if __name__ == "__main__":
    tk_root = tk.Tk()
    HubGUI(tk_root)
    tk_root.mainloop()
