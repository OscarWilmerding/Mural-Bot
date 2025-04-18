"""
Spray‑paint colour matcher — large‑print, no‑crash version
"""

import tkinter as tk
from tkinter import ttk, messagebox
import tkinter.font as tkfont
import pandas as pd
import math, re

# ─── 1 Load + clean Excel ───────────────────────────────────────
EXCEL_PATH = r"C:\Users\oewil\OneDrive\Desktop\Mural-Bot\compiled spray colors.xlsm"   # <‑‑ change me
SHEET      = 0                                        # or sheet name

raw = pd.read_excel(EXCEL_PATH, sheet_name=SHEET,
                    usecols=["Brand", "Code", "Color Name", "Hex"])

raw["Hex"] = (raw["Hex"].astype(str).str.strip()
                          .str.lstrip("#").str.lower())
df = raw[raw["Hex"].str.match(r"^[0-9a-f]{6}$", na=False)].copy()

# ─── 2 Colour maths helpers ─────────────────────────────────────
def hex_to_lab(h):
    r, g, b = [int(h[i:i+2], 16)/255 for i in (0,2,4)]
    f = lambda c: c/12.92 if c<=0.04045 else ((c+0.055)/1.055)**2.4
    r, g, b = map(f, (r, g, b))
    X = (0.4124564*r + 0.3575761*g + 0.1804375*b)*100
    Y = (0.2126729*r + 0.7151522*g + 0.0721750*b)*100
    Z = (0.0193339*r + 0.1191920*g + 0.9503041*b)*100
    Xn, Yn, Zn = 95.047, 100, 108.883
    g = lambda t: t**(1/3) if t>0.008856 else 7.787*t + 16/116
    fx, fy, fz = g(X/Xn), g(Y/Yn), g(Z/Zn)
    return 116*fy-16, 500*(fx-fy), 200*(fy-fz)

def dE(l1, l2):
    return math.sqrt(sum((a-b)**2 for a,b in zip(l1,l2)))

df["Lab"] = df["Hex"].apply(hex_to_lab)

# ─── 3 GUI ──────────────────────────────────────────────────────
root = tk.Tk()
root.title("Spray‑Paint Colour Finder")
root.geometry("800x600")

# Global 12‑pt font (fallback‑safe)
try:
    default_family = "Segoe UI"
    tkfont.nametofont("TkDefaultFont").configure(family=default_family, size=12)
except tk.TclError:           # font not installed, just bump size
    tkfont.nametofont("TkDefaultFont").configure(size=12)

style = ttk.Style(root)
style.theme_use("clam")
style.configure(".", font=tkfont.nametofont("TkDefaultFont"))
style.configure("Header.TFrame", background="#ffc0cb")
style.configure("Header.TLabel", background="#ffc0cb",
                font=(tkfont.nametofont("TkDefaultFont").cget("family"), 14, "bold"))
style.configure("Result.TFrame", background="#ffffff")
style.configure("Odd.Result.TFrame", background="#f5f7ff")
style.configure("TButton", padding=6)

# ── Header (input) ─────────────────────────────────────────────
hdr = ttk.Frame(root, style="Header.TFrame", padding=16)
hdr.pack(fill="x")

ttk.Label(hdr, text="Hex code (# optional):", style="Header.TLabel").pack(side="left")
hex_var = tk.StringVar()
ttk.Entry(hdr, textvariable=hex_var, width=12).pack(side="left", padx=8)

def search():
    code = re.sub(r"[^0-9a-fA-F]", "", hex_var.get())
    if not re.fullmatch(r"[0-9a-fA-F]{6}", code):
        messagebox.showerror("Invalid", "Enter a 6‑digit hex value.")
        return
    target = hex_to_lab(code.lower())
    df["ΔE"] = df["Lab"].apply(lambda L: dE(L, target))
    top = df.nsmallest(10, "ΔE")

    for w in results.winfo_children():
        w.destroy()

    for i, row in top.reset_index(drop=True).iterrows():
        s = "Odd.Result.TFrame" if i % 2 else "Result.TFrame"
        line = ttk.Frame(results, style=s, padding=6)
        line.pack(fill="x")

        sw = tk.Canvas(line, width=60, height=26, bd=1, relief="solid",
                       highlightthickness=0)
        sw.create_rectangle(0,0,60,26, fill="#" + row.Hex, outline="")
        sw.pack(side="left", padx=(0,10))

        txt = f'{row.Brand} | {row.Code} | {row["Color Name"]} | #{row.Hex}'
        ttk.Label(line, text=txt,
                  background=style.lookup(s, "background")).pack(side="left")

ttk.Button(hdr, text="Search", command=search).pack(side="left", padx=8)

# ── Results pane ───────────────────────────────────────────────
results = ttk.Frame(root, padding=10, style="Result.TFrame")
results.pack(fill="both", expand=True)

root.mainloop()
