"""
Spray‑paint colour matcher — extended search + brand filter
"""

import tkinter as tk
from tkinter import ttk, messagebox
import tkinter.font as tkfont
import pandas as pd
import math, re

# ─── 1 Load + clean Excel ───────────────────────────────────────
EXCEL_PATH = r"C:\Users\oewil\OneDrive\Desktop\Mural-Bot\Python Tools\compiled spray colors.xlsm"
SHEET      = 0

raw = pd.read_excel(EXCEL_PATH, sheet_name=SHEET,
                    usecols=["Brand", "Code", "Color Name", "Hex"])

raw["Hex"] = (raw["Hex"].astype(str).str.strip()
                          .str.lstrip("#").str.lower())
df = raw[raw["Hex"].str.match(r"^[0-9a-f]{6}$", na=False)].copy()

ALL_BRANDS = sorted(df["Brand"].dropna().unique().tolist())

# ─── 2 Colour maths helpers ─────────────────────────────────────
def hex_to_lab(h):
    r, g, b = [int(h[i:i+2], 16)/255 for i in (0,2,4)]
    f = lambda c: c/12.92 if c<=0.04045 else ((c+0.055)/1.055)**2.4
    r, g, b = map(f, (r, g, b))
    X = (0.4124564*r + 0.3575761*g + 0.1804375*b)*100
    Y = (0.2126729*r + 0.7151522*g + 0.0721750*b)*100
    Z = (0.0193339*r + 0.1191920*g + 0.9503041*b)*100
    Xn, Yn, Zn = 95.047, 100, 108.883
    g2 = lambda t: t**(1/3) if t>0.008856 else 7.787*t + 16/116
    fx, fy, fz = g2(X/Xn), g2(Y/Yn), g2(Z/Zn)
    return 116*fy-16, 500*(fx-fy), 200*(fy-fz)

def dE(l1, l2):
    return math.sqrt(sum((a-b)**2 for a,b in zip(l1,l2)))

def de_label(val):
    if val < 1:   return "Imperceptible"
    if val < 2:   return "Very close"
    if val < 5:   return "Close match"
    if val < 10:  return "Similar"
    if val < 20:  return "Loosely related"
    return "Distant"

# badge: (bg, fg)
def de_badge_colors(val):
    if val < 1:   return ("#c6efce", "#1e5c1e")
    if val < 2:   return ("#d0edcf", "#2a6a2a")
    if val < 5:   return ("#ffeb9c", "#5a4a00")
    if val < 10:  return ("#fce4c0", "#8a4000")
    if val < 20:  return ("#ffc7c0", "#8a2000")
    return ("#ffb8b0", "#6a0000")

df["Lab"] = df["Hex"].apply(hex_to_lab)

# ─── 3 Colours / theme ──────────────────────────────────────────
C_HDR       = "#1e1e2e"    # header bar bg
C_HDR_TXT   = "#cdd6f4"    # header bar text
C_GOLD      = "#f9c74f"    # title accent gold
C_SIDEBAR   = "#f0f0f7"    # sidebar bg
C_SB_HDR    = "#3a3a5c"    # sidebar section header bg
C_SB_HDR_FG = "#ffffff"
C_COL_HDR   = "#2e2e48"    # column header row bg
C_COL_FG    = "#e0e0ff"    # column header text
C_EVEN      = "#ffffff"
C_ODD       = "#f3f4ff"
C_BORDER    = "#d0d0e8"
C_STATUS    = "#e4e4f0"
C_STATUS_FG = "#333355"
C_BTN_S     = "#4a7ef5"    # Search button
C_BTN_S_FG  = "#ffffff"
C_BTN_C     = "#d94f4f"    # Clear button
C_BTN_C_FG  = "#ffffff"
C_BTN_SM    = "#5a5a7a"    # small buttons (All/None)
C_BTN_SM_FG = "#ffffff"

# ─── 4 GUI root ─────────────────────────────────────────────────
root = tk.Tk()
root.title("Spray‑Paint Colour Finder")
root.geometry("1060x800")
root.minsize(860, 620)
root.configure(bg=C_HDR)

try:
    tkfont.nametofont("TkDefaultFont").configure(family="Segoe UI", size=11)
except tk.TclError:
    tkfont.nametofont("TkDefaultFont").configure(size=11)

FONT  = tkfont.nametofont("TkDefaultFont").cget("family")
F11   = (FONT, 11)
F11B  = (FONT, 11, "bold")
F11I  = (FONT, 11, "italic")
F10   = (FONT, 10)
F10B  = (FONT, 10, "bold")
F9    = (FONT, 9)
F14B  = (FONT, 14, "bold")

style = ttk.Style(root)
style.theme_use("clam")
style.configure(".", font=F11)
style.configure("TEntry",    fieldbackground="#2c2c44", foreground=C_HDR_TXT,
                insertcolor=C_HDR_TXT, borderwidth=0, relief="flat")
style.map("TEntry", fieldbackground=[("focus", "#383858")])
style.configure("TScrollbar", background=C_SIDEBAR, troughcolor=C_BORDER,
                borderwidth=0, arrowsize=14)
style.configure("Vert.TScrollbar", background=C_SIDEBAR)

# ── Helper: styled tk.Button ────────────────────────────────────
def mbutton(parent, text, cmd, bg, fg, font=F11B, padx=14, pady=5):
    b = tk.Button(parent, text=text, command=cmd,
                  bg=bg, fg=fg, activebackground=bg, activeforeground=fg,
                  font=font, bd=0, relief="flat",
                  padx=padx, pady=pady, cursor="hand2")
    return b

# ── Layout skeleton ──────────────────────────────────────────────
hdr      = tk.Frame(root, bg=C_HDR)
hdr.pack(fill="x", pady=(0, 0))

# thin accent line under header
tk.Frame(root, bg=C_GOLD, height=3).pack(fill="x")

body     = tk.Frame(root, bg=C_SIDEBAR)
body.pack(fill="both", expand=True)

sidebar  = tk.Frame(body, bg=C_SIDEBAR, width=210)
sidebar.pack(side="left", fill="y")
sidebar.pack_propagate(False)

# thin vertical separator between sidebar and main
tk.Frame(body, bg=C_BORDER, width=1).pack(side="left", fill="y")

main_area = tk.Frame(body, bg=C_EVEN)
main_area.pack(side="left", fill="both", expand=True)

statusbar = tk.Label(root, text="Enter a hex code or colour name to search.",
                     bg=C_STATUS, fg=C_STATUS_FG, font=F10,
                     anchor="w", padx=10, pady=5)
statusbar.pack(fill="x", side="bottom")
# thin accent line above status bar
tk.Frame(root, bg=C_BORDER, height=1).pack(fill="x", side="bottom")

# ── Header content ───────────────────────────────────────────────
title_row = tk.Frame(hdr, bg=C_HDR)
title_row.pack(fill="x", padx=18, pady=(14, 2))
tk.Label(title_row, text="🎨  Spray‑Paint Colour Finder",
         bg=C_HDR, fg=C_GOLD, font=F14B).pack(side="left")

input_row = tk.Frame(hdr, bg=C_HDR)
input_row.pack(fill="x", padx=18, pady=(6, 14))

def hdr_label(parent, text):
    return tk.Label(parent, text=text, bg=C_HDR, fg=C_HDR_TXT, font=F10)

def hdr_entry(parent, var, width):
    e = ttk.Entry(parent, textvariable=var, width=width)
    return e

hdr_label(input_row, "Hex:").pack(side="left")
hex_var = tk.StringVar()
hdr_entry(input_row, hex_var, 10).pack(side="left", padx=(4, 18))

hdr_label(input_row, "Name contains:").pack(side="left")
name_var = tk.StringVar()
hdr_entry(input_row, name_var, 18).pack(side="left", padx=(4, 18))

hdr_label(input_row, "Max results:").pack(side="left")
max_var = tk.StringVar(value="30")
hdr_entry(input_row, max_var, 5).pack(side="left", padx=(4, 18))

hdr_label(input_row, "Max ΔE:").pack(side="left")
de_var = tk.StringVar(value="")
hdr_entry(input_row, de_var, 5).pack(side="left", padx=(4, 22))

mbutton(input_row, "  Search  ", lambda: search(), C_BTN_S, C_BTN_S_FG).pack(side="left", padx=(0, 8))
mbutton(input_row, "Clear", lambda: clear(), C_BTN_C, C_BTN_C_FG, padx=10).pack(side="left")

# Bind Enter key to search
root.bind("<Return>", lambda _: search())

# ── Sidebar: brand filter ────────────────────────────────────────
# Section header
tk.Frame(sidebar, bg=C_SB_HDR, height=32).pack(fill="x")
tk.Label(sidebar, text="Filter by Brand", bg=C_SB_HDR, fg=C_SB_HDR_FG,
         font=F10B, anchor="w", padx=10, pady=7).place(x=0, y=0, relwidth=1)

# re‑pack properly
sb_hdr = tk.Frame(sidebar, bg=C_SB_HDR)
sb_hdr.pack(fill="x")
tk.Label(sb_hdr, text="Filter by Brand", bg=C_SB_HDR, fg=C_SB_HDR_FG,
         font=F10B, anchor="w", padx=10, pady=8).pack(fill="x")

brand_outer = tk.Frame(sidebar, bg=C_SIDEBAR, padx=8, pady=6)
brand_outer.pack(fill="both", expand=True)

brand_scroll = tk.Scrollbar(brand_outer, orient="vertical", bg=C_SIDEBAR)
brand_listbox = tk.Listbox(brand_outer, selectmode="multiple",
                            yscrollcommand=brand_scroll.set,
                            exportselection=False,
                            font=F11,
                            activestyle="none",
                            bg="#ffffff",
                            fg="#222244",
                            selectbackground=C_BTN_S,
                            selectforeground="#ffffff",
                            bd=1, relief="solid",
                            highlightthickness=0)
brand_scroll.config(command=brand_listbox.yview)
brand_scroll.pack(side="right", fill="y")
brand_listbox.pack(side="left", fill="both", expand=True)

for brand in ALL_BRANDS:
    brand_listbox.insert("end", brand)

btn_row = tk.Frame(sidebar, bg=C_SIDEBAR)
btn_row.pack(fill="x", padx=8, pady=(0, 8))
mbutton(btn_row, "Select All", lambda: brand_listbox.select_set(0, "end"),
        C_BTN_SM, C_BTN_SM_FG, font=F10, padx=8, pady=4).pack(side="left", expand=True, fill="x", padx=(0, 4))
mbutton(btn_row, "None", lambda: brand_listbox.selection_clear(0, "end"),
        C_BTN_SM, C_BTN_SM_FG, font=F10, padx=8, pady=4).pack(side="left", expand=True, fill="x")

# ── Results area (scrollable canvas) ────────────────────────────
canvas  = tk.Canvas(main_area, highlightthickness=0, background=C_EVEN)
vscroll = ttk.Scrollbar(main_area, orient="vertical", command=canvas.yview)
canvas.configure(yscrollcommand=vscroll.set)

vscroll.pack(side="right", fill="y")
canvas.pack(side="left", fill="both", expand=True)

results = tk.Frame(canvas, bg=C_EVEN)
canvas_window = canvas.create_window((0, 0), window=results, anchor="nw")

def _on_results_configure(_):
    canvas.configure(scrollregion=canvas.bbox("all"))

def _on_canvas_configure(event):
    canvas.itemconfig(canvas_window, width=event.width)

results.bind("<Configure>", _on_results_configure)
canvas.bind("<Configure>", _on_canvas_configure)
canvas.bind_all("<MouseWheel>", lambda e: canvas.yview_scroll(int(-1*(e.delta/120)), "units"))

# ── Column header ────────────────────────────────────────────────
COL_WIDTHS = [70, 130, 90, 210, 90, 180]  # swatch, brand, code, name, hex, ΔE

def make_col_header():
    hrow = tk.Frame(results, bg=C_COL_HDR)
    hrow.pack(fill="x")
    for text, w in zip(["", "Brand", "Code", "Color Name", "Hex", "ΔE (color diff)"], COL_WIDTHS):
        tk.Label(hrow, text=text, bg=C_COL_HDR, fg=C_COL_FG,
                 font=F10B, width=w//8, anchor="w",
                 padx=4, pady=6).pack(side="left")

# ── Search logic ─────────────────────────────────────────────────
def get_selected_brands():
    indices = brand_listbox.curselection()
    if not indices:
        return None
    return [ALL_BRANDS[i] for i in indices]

def search():
    hex_raw  = re.sub(r"[^0-9a-fA-F]", "", hex_var.get())
    name_raw = name_var.get().strip()
    brands   = get_selected_brands()

    try:
        max_n = int(max_var.get()) if max_var.get().strip() else None
    except ValueError:
        messagebox.showerror("Invalid", "Max results must be a number.")
        return
    try:
        de_thresh = float(de_var.get()) if de_var.get().strip() else None
    except ValueError:
        messagebox.showerror("Invalid", "Max ΔE must be a number.")
        return

    has_hex  = re.fullmatch(r"[0-9a-fA-F]{6}", hex_raw) is not None
    has_name = len(name_raw) > 0

    if not has_hex and not has_name:
        messagebox.showerror("No input", "Enter a hex code and/or a colour name to search.")
        return

    work = df.copy()
    if brands:
        work = work[work["Brand"].isin(brands)]
    if has_name:
        work = work[work["Color Name"].str.contains(name_raw, case=False, na=False)]
    if has_hex:
        target    = hex_to_lab(hex_raw.lower())
        work["ΔE"] = work["Lab"].apply(lambda L: dE(L, target))
        if de_thresh is not None:
            work = work[work["ΔE"] <= de_thresh]
        work = work.sort_values("ΔE")
    else:
        work["ΔE"] = float("nan")
        work = work.sort_values(["Brand", "Color Name"])

    if max_n is not None:
        work = work.head(max_n)

    # ── Clear & rebuild results ──────────────────────────────────
    for w in results.winfo_children():
        w.destroy()

    make_col_header()

    # ΔE legend strip
    legend = tk.Frame(results, bg="#f8f8fd", padx=10, pady=5)
    legend.pack(fill="x")
    tk.Label(legend,
             text="ΔE = perceptual colour difference  ·  "
                  "<1 Imperceptible  ·  1–2 Very close  ·  2–5 Close match  ·  "
                  "5–10 Similar  ·  10–20 Loosely related  ·  >20 Distant",
             bg="#f8f8fd", fg="#555577", font=F9,
             wraplength=720, justify="left", anchor="w").pack(fill="x")
    tk.Frame(results, bg=C_BORDER, height=1).pack(fill="x")

    if work.empty:
        tk.Label(results, text="No results found.", bg=C_EVEN,
                 font=F11, fg="#9999bb", pady=24).pack()
        statusbar.config(text="No results found.")
        return

    for i, row in work.reset_index(drop=True).iterrows():
        bg = C_EVEN if i % 2 == 0 else C_ODD

        line = tk.Frame(results, bg=bg, pady=5, padx=8)
        line.pack(fill="x")

        # ── Swatch (with thin border frame) ─────────────────────
        sw_border = tk.Frame(line, bg=C_BORDER, padx=1, pady=1)
        sw_border.pack(side="left", padx=(0, 10))
        sw = tk.Canvas(sw_border, width=52, height=28, bd=0,
                       highlightthickness=0)
        sw.create_rectangle(0, 0, 52, 28, fill="#" + row.Hex, outline="")
        sw.pack()

        # Brand
        tk.Label(line, text=row.Brand, bg=bg, fg="#222244",
                 font=F11, width=12, anchor="w").pack(side="left", padx=(0, 6))

        # Code
        tk.Label(line, text=str(row.Code), bg=bg, fg="#444466",
                 font=F11, width=10, anchor="w").pack(side="left", padx=(0, 6))

        # Color Name
        tk.Label(line, text=row["Color Name"], bg=bg, fg="#111133",
                 font=F11B, width=22, anchor="w").pack(side="left", padx=(0, 6))

        # Hex value — monospaced feel via explicit text
        tk.Label(line, text="#" + row.Hex.upper(), bg=bg, fg="#3a3a8a",
                 font=F11I, width=8, anchor="w").pack(side="left", padx=(0, 10))

        # ΔE badge
        if not math.isnan(row["ΔE"]):
            de_val           = row["ΔE"]
            badge_bg, badge_fg = de_badge_colors(de_val)
            badge_text       = f"  ΔE {de_val:.1f}  {de_label(de_val)}  "
            tk.Label(line, text=badge_text,
                     bg=badge_bg, fg=badge_fg,
                     font=F10B, relief="flat", padx=2, pady=2,
                     bd=1).pack(side="left")

    count = len(work)
    total = len(df) if not brands else len(df[df["Brand"].isin(brands)])
    brand_str = f"  ·  brands: {', '.join(brands)}" if brands else ""
    statusbar.config(text=f"Showing {count} of {total} paint(s){brand_str}")

def clear():
    hex_var.set("")
    name_var.set("")
    max_var.set("30")
    de_var.set("")
    brand_listbox.selection_clear(0, "end")
    for w in results.winfo_children():
        w.destroy()
    statusbar.config(text="Cleared. Enter a hex code or colour name to search.")

root.mainloop()
