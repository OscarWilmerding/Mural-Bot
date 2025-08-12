import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial, serial.tools.list_ports
import threading, queue, time

BAUD = 115200
NL   = b'\n'
HANDSHAKE_MS = 3000


class SprayGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Spray-Chassis Serial Console")

        self.ser      = None
        self.rx_q     = queue.Queue()
        self.rx_thr   = None
        self.running  = False
        self.handshake_pending = False

        self.port_var = tk.StringVar()

        self.make_topbar()
        self.make_controls()
        self.make_console()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(100, self.check_rx_queue)

    # ---------- UI layout ----------
    def make_topbar(self):
        top = ttk.Frame(self.root, padding=6)
        top.grid(sticky="ew")
        self.root.columnconfigure(0, weight=1)

        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_var.set(ports[0] if ports else "")
        self.port_cbx = ttk.Combobox(
            top, textvariable=self.port_var,
            values=ports, width=15, state="readonly")
        self.port_cbx.grid(row=0, column=0, padx=(0, 6))

        ttk.Button(top, text="Update Ports", width=12,
                   command=self.refresh_ports).grid(row=0, column=1, padx=3)
        ttk.Button(top, text="Connect", width=10,
                   command=self.open_port).grid(row=0, column=2, padx=3)
        ttk.Button(top, text="Reset Port", width=10,
                   command=self.reset_port).grid(row=0, column=3, padx=3)

        self.status_lbl = ttk.Label(top, text="closed")
        self.status_lbl.grid(row=0, column=4, padx=6)
        top.columnconfigure(5, weight=1)

    def refresh_ports(self):
        current = self.port_var.get()
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_cbx["values"] = ports
        if current in ports:
            self.port_var.set(current)
        elif ports:
            self.port_var.set(ports[0])
        else:
            self.port_var.set("")

    def make_controls(self):
        cmd = ttk.Frame(self.root, padding=(6, 0, 6, 6))
        cmd.grid(sticky="ew")
        for i in range(7):
            cmd.columnconfigure(i, weight=1)

        r = c = 0

        ttk.Label(cmd, text="Command").grid(row=r, column=0, sticky="e", padx=2)
        self.free_entry = ttk.Entry(cmd)
        self.free_entry.grid(row=r, column=1, columnspan=4, sticky="ew", padx=(4, 4))
        ttk.Button(cmd, text="Send", width=10,
                   command=self.send_from_entry).grid(row=r, column=5, sticky="w")
        self.free_entry.bind("<Return>", lambda e: self.send_from_entry())
        r += 1; c = 0

        def btn(label, cmdstr):
            nonlocal r, c
            ttk.Button(cmd, text=label, width=10,
                       command=lambda s=cmdstr: self.send(s)).grid(row=r, column=c, pady=2)
            c = (c + 1) % 6
            if c == 0:
                r += 1

        for lbl in ("clean", "forever", "rand", "trig", "?"):
            btn(lbl, lbl)

        ttk.Label(cmd, text="pulse-width ms").grid(row=r, column=c, sticky="e", padx=2)
        self.pw_entry = ttk.Entry(cmd, width=6)
        self.pw_entry.grid(row=r, column=c + 1, sticky="w")
        ttk.Button(cmd, text="Set",
                   command=lambda: self.send(self.pw_entry.get())).grid(row=r, column=c + 2)
        r += 1; c = 0

        ttk.Label(cmd, text="delay ms").grid(row=r, column=0, sticky="e", padx=2)
        self.delay_entry = ttk.Entry(cmd, width=6); self.delay_entry.grid(row=r, column=1, sticky="w")
        ttk.Button(cmd, text="Set",
                   command=lambda: self.send(f"delay {self.delay_entry.get()}")).grid(row=r, column=2)

        ttk.Label(cmd, text="trig").grid(row=r, column=3, sticky="e", padx=2)
        self.trig_s = ttk.Entry(cmd, width=3); self.trig_c = ttk.Entry(cmd, width=3)
        self.trig_s.grid(row=r, column=4); self.trig_c.grid(row=r, column=5)
        ttk.Button(cmd, text="Send",
                   command=lambda: self.send(f"trig {self.trig_s.get()},{self.trig_c.get()}")).grid(row=r, column=6)
        r += 1

        ttk.Label(cmd, text="calibration").grid(row=r, column=0, sticky="e", padx=2)
        self.cal_sol  = ttk.Entry(cmd, width=3)
        self.cal_low  = ttk.Entry(cmd, width=4)
        self.cal_high = ttk.Entry(cmd, width=4)
        self.cal_step = ttk.Entry(cmd, width=4)
        self.cal_sol.grid(row=r, column=1);  self.cal_low.grid(row=r, column=2)
        self.cal_high.grid(row=r, column=3); self.cal_step.grid(row=r, column=4)
        ttk.Button(cmd, text="Run",
                   command=lambda: self.send(
                       f"calibration {self.cal_sol.get()},{self.cal_low.get()},"
                       f"{self.cal_high.get()},{self.cal_step.get()}")).grid(row=r, column=5)

    def make_console(self):
        self.console = scrolledtext.ScrolledText(
            self.root, height=15, wrap=tk.WORD, state="disabled")
        self.console.grid(sticky="nsew", padx=6, pady=(0, 6))
        self.root.rowconfigure(2, weight=1)

    # ---------- Serial handling ----------
    def open_port(self):
        port = self.port_var.get()
        if not port:
            messagebox.showerror("No port", "Select a COM port first"); return
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.1)
        except serial.SerialException as e:
            messagebox.showerror("Serial error", str(e)); return

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

    def reset_port(self):
        if not self.ser: return
        port = self.ser.port
        self.close_port()
        time.sleep(0.5)
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.1)
        except serial.SerialException as e:
            messagebox.showerror("Serial error", str(e)); return

        self.status_lbl.config(text="waiting...")
        self.running = True
        self.handshake_pending = True
        self.rx_thr = threading.Thread(target=self.read_thread, daemon=True)
        self.rx_thr.start()
        self.send("?")
        self.root.after(HANDSHAKE_MS, self.check_handshake_timeout)

    def close_port(self):
        self.running = False
        if self.ser and self.ser.is_open:
            try: self.ser.close()
            except serial.SerialException: pass
        self.status_lbl.config(text="closed")

    def on_close(self):
        self.close_port()
        self.root.destroy()

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

    def send(self, text):
        if not text: return
        if not self.ser or not self.ser.is_open:
            self.console.configure(state="normal")
            self.console.insert(tk.END, "[not connected] " + text + "\n")
            self.console.configure(state="disabled")
            self.console.see(tk.END)
            return
        try:
            self.ser.write(text.encode() + NL)
            self.console.configure(state="normal")
            self.console.insert(tk.END, "> " + text + "\n")
            self.console.configure(state="disabled")
            self.console.see(tk.END)
        except serial.SerialException as e:
            messagebox.showerror("Write error", str(e))

    def send_from_entry(self):
        txt = self.free_entry.get().strip()
        if not txt:
            return
        self.send(txt)
        self.free_entry.delete(0, tk.END)


if __name__ == "__main__":
    root = tk.Tk()
    SprayGUI(root)
    root.mainloop()
