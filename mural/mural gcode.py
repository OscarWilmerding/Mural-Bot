import tkinter as tk
from tkinter import filedialog, ttk
from PIL import Image
import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.patches as mpatches
import math
import random
from sklearn.cluster import KMeans
import sys
import webcolors
import json
# BEFORE RUNNING:
# 1) Run the command 'pip install tkinter pillow numpy matplotlib sklearn' in your terminal to install all necessary libraries.
# 2) Ensure that the folder the script will save files to exists or provide a new path for 'root_folder' below.
# 3) Provide a file path for 'gcode_filepath' below.

root_folder = r'C:/Users/oewil/OneDrive/Desktop/Mural-Bot/mural'
gcode_filepath = os.path.join(root_folder, "gcode.txt")
settings_filepath = os.path.join(root_folder, "settings.json")


def load_settings():
    defaults = {
        "file_path": r"C:/Users/oewil/OneDrive/Desktop/Mural-Bot/mural/imput images/jules eye 257 wide.jpg",
        "width": 310,
        "pixel_size": 0.01,
        "cable_sepperation": 4.6,
        "dist_from_pulley": 4.2,
        "floor_dist_from_pulleys": 6.0,
        "chassis_length_below_nozzles": 0.3,
        "offset": 0.0,
        "color_mode": "Simplify Image",
        "number_of_colors": 2,
        "n_value": 3,
        "peak_velocity": 0.5,
        "slicing_option": "multi color velocity slicing",
        "Num_nozzles": 8,
        "notes": "horizontal sep = 12mm, vertical sep = 20mm\nwall width 9ft → 228px at 12mm/px\nJules eye temp target 257px\n",
    }
    if os.path.exists(settings_filepath):
        try:
            with open(settings_filepath, 'r') as f:
                saved = json.load(f)
            defaults.update(saved)
        except Exception as e:
            print(f"Could not load settings, using defaults: {e}")
    return defaults


def save_settings(settings_dict):
    try:
        with open(settings_filepath, 'w') as f:
            json.dump(settings_dict, f, indent=2)
    except Exception as e:
        print(f"Could not save settings: {e}")

reduced_image_path = os.path.join(root_folder, "temp.png")
processed_image_path = os.path.join(root_folder, "processed_image_path.png")
preview_image_path = os.path.join(root_folder, "preview_image.png")

# Create the temp_images folder
temp_images_folder = os.path.join(root_folder, 'temp_images')
# Ensure temp_images_folder exists
if not os.path.exists(temp_images_folder):
    os.makedirs(temp_images_folder)
    print(f"Created directory: {temp_images_folder}")

# Global variables - loaded from settings.json if it exists, otherwise use defaults in load_settings()
_s = load_settings()
file_path = _s["file_path"]
width = _s["width"]
pixel_size = _s["pixel_size"]
cable_sepperation = _s["cable_sepperation"]
dist_from_pulley = _s["dist_from_pulley"]
floor_dist_from_pulleys = _s["floor_dist_from_pulleys"]
chassis_length_below_nozzles = _s["chassis_length_below_nozzles"]
offset = _s["offset"]
color_mode = _s["color_mode"]
number_of_colors = _s["number_of_colors"]
n_value = _s["n_value"]
peak_velocity = _s["peak_velocity"]
slicing_option = _s["slicing_option"]
Num_nozzles = _s["Num_nozzles"]
notes = _s["notes"]


HAS_PRINTED_COLOR_MAPPING = False


# Ensure root_folder exists
if not os.path.exists(root_folder):
    os.makedirs(root_folder)
    print(f"Created directory: {root_folder}")

# Ensure the directory for gcode_filepath exists
gcode_dir = os.path.dirname(gcode_filepath)
if not os.path.exists(gcode_dir):
    os.makedirs(gcode_dir)
    print(f"Created directory: {gcode_dir}")

# Import the new modules
import utils
import slicing_styles
import multi_color_slicing


def initial_popup():
    """
    Displays a GUI popup to collect user inputs for various parameters,
    with a live engineering drawing showing the bot geometry.
    """
    BG = "#f5f5f5"
    PANEL_BG = "#ffffff"
    ACCENT = "#2563eb"
    LABEL_FG = "#1e293b"
    ENTRY_BG = "#ffffff"
    BORDER = "#cbd5e1"
    WARN_COLOR = "#dc2626"

    def browse_file():
        global file_path
        filetypes = (
            ('Image files', '*.png *.jpg *.jpeg *.bmp *.gif *.tiff'),
            ('All files', '*.*')
        )
        file_path = filedialog.askopenfilename(filetypes=filetypes)
        file_path_entry.delete(0, tk.END)
        file_path_entry.insert(0, file_path)
        update_drawing()

    def submit():
        global file_path, width, pixel_size, number_of_colors
        global cable_sepperation, dist_from_pulley, offset, color_mode, n_value
        global slicing_option, Num_nozzles, floor_dist_from_pulleys, chassis_length_below_nozzles, notes
        file_path = file_path_entry.get()
        color_mode = color_mode_var.get()
        if color_mode == 'Exact Color Match':
            from PIL import Image as _PILImg
            width = _PILImg.open(file_path).size[0]
        else:
            width = int(width_entry.get())
        Num_nozzles = int(nozzles_entry.get())
        pixel_size = float(pixel_size_entry.get())
        cable_sepperation = float(cable_sepperation_entry.get())
        dist_from_pulley = float(dist_from_pulley_entry.get())
        floor_dist_from_pulleys = float(floor_dist_entry.get())
        chassis_length_below_nozzles = float(chassis_below_entry.get())
        offset = float(offset_entry.get())
        if color_mode in ['Simplify Image', 'Dynamic Scatter NxN']:
            number_of_colors = int(number_of_colors_entry.get())
        if color_mode in ['RGB Scatter NxN', 'Dynamic Scatter NxN']:
            n_value = int(n_value_entry.get())
        slicing_option = slicing_option_var.get()
        notes = notes_text.get('1.0', 'end-1c')
        save_settings({
            "file_path": file_path,
            "width": width,
            "pixel_size": pixel_size,
            "cable_sepperation": cable_sepperation,
            "dist_from_pulley": dist_from_pulley,
            "floor_dist_from_pulleys": floor_dist_from_pulleys,
            "chassis_length_below_nozzles": chassis_length_below_nozzles,
            "offset": offset,
            "color_mode": color_mode,
            "number_of_colors": number_of_colors,
            "n_value": n_value,
            "peak_velocity": peak_velocity,
            "slicing_option": slicing_option,
            "Num_nozzles": Num_nozzles,
            "notes": notes,
        })
        root.quit()
        root.destroy()

    def on_color_mode_change(*args):
        selected_mode = color_mode_var.get()
        if selected_mode == 'Exact Color Match':
            width_label.grid_remove()
            width_entry.grid_remove()
        else:
            width_label.grid(row=3, column=0, sticky="e", padx=(0, 8), pady=3)
            width_entry.grid(row=3, column=1, sticky="ew", pady=3)
        if selected_mode in ['Simplify Image', 'Dynamic Scatter NxN']:
            number_of_colors_label.grid(row=14, column=0, sticky="e", padx=(0, 8), pady=3)
            number_of_colors_entry.grid(row=14, column=1, sticky="ew", pady=3)
        else:
            number_of_colors_label.grid_remove()
            number_of_colors_entry.grid_remove()
        if selected_mode in ['RGB Scatter NxN', 'Dynamic Scatter NxN']:
            n_value_label.grid(row=15, column=0, sticky="e", padx=(0, 8), pady=3)
            n_value_entry.grid(row=15, column=1, sticky="ew", pady=3)
        else:
            n_value_label.grid_remove()
            n_value_entry.grid_remove()

    def safe_float(entry, fallback):
        try:
            return float(entry.get())
        except ValueError:
            return fallback

    def safe_int(entry, fallback):
        try:
            return int(entry.get())
        except ValueError:
            return fallback

    def update_drawing(*args):
        try:
            w_px = safe_int(width_entry, width)
            px_sz = safe_float(pixel_size_entry, pixel_size)
            cab_sep = safe_float(cable_sepperation_entry, cable_sepperation)
            d_pull = safe_float(dist_from_pulley_entry, dist_from_pulley)
            flr = safe_float(floor_dist_entry, floor_dist_from_pulleys)
            cbl = safe_float(chassis_below_entry, chassis_length_below_nozzles)
            off = safe_float(offset_entry, offset)
            n_noz = safe_int(nozzles_entry, Num_nozzles)

            # Derived mural dimensions
            mural_w = w_px * px_sz
            # Height not directly an input; approximate as ~0.75 * width for display
            # Use dist_from_pulley as the vertical span to bottom of mural
            mural_h = d_pull  # dist_from_pulley is dist from pulleys to bottom of mural

            ax.cla()
            ax.set_facecolor('#f8fafc')

            # --- coordinate system: origin at midpoint between pulleys ---
            # Pulleys at (-cab_sep/2, 0) and (+cab_sep/2, 0)
            # y increases downward (drawing convention)
            # Mural top starts at y = dist_from_pulley - mural_h (i.e., above mural bottom)
            # Mural bottom at y = dist_from_pulley
            # Floor at y = flr

            pl = -cab_sep / 2
            pr = +cab_sep / 2
            mural_bottom_y = d_pull
            img_center_x = off  # midpoint between pulleys, shifted by offset (positive = right)

            # Load image to get true physical dimensions
            img_path = file_path_entry.get()
            preview_img = None
            phys_w = mural_w  # fallback
            phys_h = mural_h  # fallback
            try:
                if os.path.isfile(img_path):
                    preview_img = Image.open(img_path).convert("RGB")
                    img_px_w, img_px_h = preview_img.size
                    mode = color_mode_var.get()
                    if mode == 'Exact Color Match':
                        phys_w = img_px_w * px_sz
                        phys_h = img_px_h * px_sz
                    else:
                        aspect = img_px_h / img_px_w
                        phys_w = w_px * px_sz
                        phys_h = w_px * aspect * px_sz
            except Exception:
                pass

            # Derive left/right from centre so image is always centred
            mural_left = img_center_x - phys_w / 2
            mural_right = img_center_x + phys_w / 2

            # Blue bounding box follows actual image physical size
            mural_top_y = mural_bottom_y - phys_h

            # Chassis at bottom-centre of mural
            chassis_x = img_center_x
            chassis_y = mural_bottom_y

            # Chassis below line bottom
            chassis_tail_y = chassis_y + cbl

            # --- Draw floor ---
            floor_color = WARN_COLOR if chassis_tail_y >= flr else "#64748b"
            ax.axhline(y=flr, color=floor_color, linewidth=2, linestyle='--', zorder=1)
            ax.text(pr + 0.1, flr, f"Floor\n{flr:.2f}m", color=floor_color,
                    fontsize=7, va='center', ha='left')

            # --- Draw mural area (matches image physical size) ---
            mural_rect = mpatches.FancyBboxPatch(
                (mural_left, mural_top_y),
                phys_w, phys_h,
                boxstyle="square,pad=0",
                linewidth=1.2, edgecolor="#2563eb", facecolor="#dbeafe", zorder=2
            )
            ax.add_patch(mural_rect)

            # --- Image preview ---
            if preview_img is not None:
                try:
                    preview_arr = np.array(preview_img)
                    ax.imshow(preview_arr,
                              extent=[mural_left, mural_left + phys_w,
                                      mural_bottom_y, mural_top_y],
                              aspect='auto', alpha=0.35, zorder=3,
                              interpolation='bilinear')
                except Exception:
                    pass

            ax.text(mural_left + phys_w / 2, mural_top_y + phys_h * 0.06,
                    f"{phys_w:.2f} × {phys_h:.2f} m",
                    fontsize=7, ha='center', va='top', color="#1e40af",
                    zorder=4, bbox=dict(facecolor='white', alpha=0.6,
                                        edgecolor='none', pad=1))

            # --- Draw pulleys ---
            for px_coord, label, ha in [(pl, "A", "right"), (pr, "B", "left")]:
                ax.plot(px_coord, 0, 'o', markersize=10, color="#475569", zorder=5)
                offset_x = -0.12 if ha == "right" else 0.12
                ax.text(px_coord + offset_x, 0, label, fontsize=9, fontweight='bold',
                        ha=ha, va='center', color="#475569")

            # --- Draw cables from pulleys to chassis ---
            ax.plot([pl, chassis_x], [0, chassis_y], color="#94a3b8", linewidth=1.2,
                    zorder=3, linestyle='-')
            ax.plot([pr, chassis_x], [0, chassis_y], color="#94a3b8", linewidth=1.2,
                    zorder=3, linestyle='-')

            # --- Draw chassis nozzle row ---
            nozzle_half_w = (n_noz - 1) * px_sz / 2 if n_noz > 1 else 0
            ax.plot([chassis_x - nozzle_half_w, chassis_x + nozzle_half_w],
                    [chassis_y, chassis_y],
                    color="#0f172a", linewidth=3, zorder=6, solid_capstyle='round')

            # --- Draw chassis tail below nozzles ---
            tail_color = WARN_COLOR if chassis_tail_y >= flr else "#0f172a"
            ax.plot([chassis_x, chassis_x], [chassis_y, chassis_tail_y],
                    color=tail_color, linewidth=2.5, zorder=6, solid_capstyle='round')
            # dimension annotation for chassis tail
            ax.annotate('', xy=(chassis_x + 0.15, chassis_tail_y),
                        xytext=(chassis_x + 0.15, chassis_y),
                        arrowprops=dict(arrowstyle='<->', color=tail_color, lw=1.2))
            ax.text(chassis_x + 0.22, (chassis_y + chassis_tail_y) / 2,
                    f"{cbl:.3f}m", fontsize=7, va='center', color=tail_color)

            if chassis_tail_y >= flr:
                overlap = chassis_tail_y - flr
                ax.text(chassis_x + 0.22, flr - 0.05,
                        f"⚠ {overlap:.3f}m overlap!", fontsize=7,
                        color=WARN_COLOR, va='top', fontweight='bold')

            # --- Dimension annotations ---
            # Pulley spacing
            ax.annotate('', xy=(pr, -0.35), xytext=(pl, -0.35),
                        arrowprops=dict(arrowstyle='<->', color="#475569", lw=1))
            ax.text(0, -0.42, f"Pulley spacing: {cab_sep:.2f}m",
                    fontsize=7, ha='center', color="#475569")

            # dist_from_pulley (pulley to mural bottom)
            ann_x = mural_left - 0.2
            ax.annotate('', xy=(ann_x, mural_bottom_y), xytext=(ann_x, 0),
                        arrowprops=dict(arrowstyle='<->', color="#475569", lw=1))
            ax.text(ann_x - 0.05, mural_bottom_y / 2,
                    f"{d_pull:.2f}m", fontsize=7, ha='right', va='center', color="#475569",
                    rotation=90)

            ax.set_title("Visualization",
                         fontsize=9, pad=8, color=LABEL_FG)
            ax.set_xlabel("x (m)", fontsize=8)
            ax.set_ylabel("y (m)", fontsize=8)
            ax.tick_params(labelsize=7)

            # Set limits with y inverted (large bottom = deep, 0 = pulley level)
            x_pad = cab_sep * 0.25
            y_pad = flr * 0.08
            ax.set_xlim(pl - x_pad - 0.4, pr + x_pad + 0.6)
            ax.set_ylim(flr + y_pad, -0.6)

            # Equal aspect AFTER limits so 1m on x == 1m on y
            ax.set_aspect('equal', adjustable='box')

            fig.tight_layout()
            canvas.draw()

            # --- Update stats labels ---
            import math
            dist_a_to_br = math.sqrt((mural_right - pl) ** 2 + d_pull ** 2)
            dist_b_to_bl = math.sqrt((mural_left - pr) ** 2 + d_pull ** 2)
            longest_a = flr + cab_sep + dist_a_to_br
            longest_b = flr + dist_b_to_bl
            stat_a_label.config(text=f"Longest A cable:  {longest_a:.3f} m  "
                                     f"(floor {flr:.2f} + spacing {cab_sep:.2f} + diagonal {dist_a_to_br:.3f})")
            stat_b_label.config(text=f"Longest B cable:  {longest_b:.3f} m  "
                                     f"(floor {flr:.2f} + diagonal {dist_b_to_bl:.3f})")
        except Exception:
            pass

    # ---- Root window ----
    root = tk.Tk()
    root.title("Mural Bot — Configuration")
    root.configure(bg=BG)
    root.resizable(True, True)

    # ---- Two-column layout: form left, drawing right ----
    form_frame = tk.Frame(root, bg=BG, padx=16, pady=16)
    form_frame.grid(row=0, column=0, sticky="nsew")

    draw_frame = tk.Frame(root, bg=PANEL_BG, padx=8, pady=8,
                          relief="flat", bd=1, highlightbackground=BORDER,
                          highlightthickness=1)
    draw_frame.grid(row=0, column=1, sticky="nsew", padx=(0, 16), pady=16)

    root.grid_columnconfigure(0, weight=0)
    root.grid_columnconfigure(1, weight=1)
    root.grid_rowconfigure(0, weight=1)

    # ---- Section header helper ----
    def section(parent, text, row):
        lbl = tk.Label(parent, text=text, bg=BG, fg=ACCENT,
                       font=("Segoe UI", 12, "bold"))
        lbl.grid(row=row, column=0, columnspan=2, sticky="w", pady=(12, 2))

    def lbl(parent, text, row):
        tk.Label(parent, text=text, bg=BG, fg=LABEL_FG,
                 font=("Segoe UI", 12)).grid(row=row, column=0, sticky="e",
                                            padx=(0, 8), pady=3)

    def entry(parent, val, row, width_chars=18):
        e = tk.Entry(parent, width=width_chars, bg=ENTRY_BG, fg=LABEL_FG,
                     relief="solid", bd=1, font=("Segoe UI", 12),
                     highlightthickness=0)
        e.grid(row=row, column=1, sticky="ew", pady=3)
        e.insert(0, str(val))
        return e

    form_frame.grid_columnconfigure(1, weight=1)

    # ---- Image ----
    section(form_frame, "IMAGE", 0)
    lbl(form_frame, "File Path:", 1)
    file_path_entry = tk.Entry(form_frame, width=30, bg=ENTRY_BG, fg=LABEL_FG,
                               relief="solid", bd=1, font=("Segoe UI", 12))
    file_path_entry.grid(row=1, column=1, sticky="ew", pady=3)
    file_path_entry.insert(0, file_path)
    browse_button = tk.Button(form_frame, text="Browse", command=browse_file,
                              bg=ACCENT, fg="white", relief="flat",
                              font=("Segoe UI", 12), padx=8, pady=2,
                              activebackground="#1d4ed8", activeforeground="white",
                              cursor="hand2")
    browse_button.grid(row=1, column=2, padx=(4, 0), pady=3)

    # ---- Print settings ----
    section(form_frame, "PRINT", 2)
    width_label = tk.Label(form_frame, text="Width (px):", bg=BG, fg=LABEL_FG,
                           font=("Segoe UI", 12))
    width_label.grid(row=3, column=0, sticky="e", padx=(0, 8), pady=3)
    width_entry = entry(form_frame, width, 3)

    lbl(form_frame, "Pixel Size (m):", 4)
    pixel_size_entry = entry(form_frame, pixel_size, 4)

    lbl(form_frame, "Nozzles per stripe:", 5)
    nozzles_entry = entry(form_frame, Num_nozzles, 5)

    # Number of Colors (hidden)
    number_of_colors_label = tk.Label(form_frame, text="Number of Colors:",
                                      bg=BG, fg=LABEL_FG, font=("Segoe UI", 12))
    number_of_colors_entry = tk.Entry(form_frame, width=18, bg=ENTRY_BG, fg=LABEL_FG,
                                      relief="solid", bd=1, font=("Segoe UI", 12))
    number_of_colors_entry.insert(0, str(number_of_colors))

    # N Value (hidden)
    n_value_label = tk.Label(form_frame, text="Value of N:", bg=BG, fg=LABEL_FG,
                             font=("Segoe UI", 12))
    n_value_entry = tk.Entry(form_frame, width=18, bg=ENTRY_BG, fg=LABEL_FG,
                             relief="solid", bd=1, font=("Segoe UI", 12))
    n_value_entry.insert(0, str(n_value))

    # ---- Geometry ----
    section(form_frame, "GEOMETRY", 6)
    lbl(form_frame, "Pulley Spacing (m):", 7)
    cable_sepperation_entry = entry(form_frame, cable_sepperation, 7)

    lbl(form_frame, "Pulleys → Mural Bottom (m):", 8)
    dist_from_pulley_entry = entry(form_frame, dist_from_pulley, 8)

    lbl(form_frame, "Pulleys → Floor (m):", 9)
    floor_dist_entry = entry(form_frame, floor_dist_from_pulleys, 9)

    lbl(form_frame, "Chassis Length Below Nozzles (m):", 10)
    chassis_below_entry = entry(form_frame, chassis_length_below_nozzles, 10)

    lbl(form_frame, "Offset from Center Left (m):", 11)
    offset_entry = entry(form_frame, offset, 11)

    # ---- Color / Slicing ----
    section(form_frame, "COLOR & SLICING", 12)
    color_mode_var = tk.StringVar(value=color_mode)
    color_mode_var.trace('w', on_color_mode_change)
    lbl(form_frame, "Color Mode:", 13)
    color_options = ['RGB', 'CMYK', 'Simplify Image', 'Exact Color Match', 'RGB Scatter NxN', 'Dynamic Scatter NxN']
    color_dropdown = tk.OptionMenu(form_frame, color_mode_var, *color_options)
    color_dropdown.config(bg=ENTRY_BG, fg=LABEL_FG, relief="solid",
                          font=("Segoe UI", 12), highlightthickness=0)
    color_dropdown.grid(row=13, column=1, sticky="ew", pady=3)

    slicing_option_var = tk.StringVar(value=slicing_option)
    lbl(form_frame, "Slicing:", 16)
    slicing_options = ['mono color velocity slicing', 'multi color velocity slicing']
    slicing_dropdown = tk.OptionMenu(form_frame, slicing_option_var, *slicing_options)
    slicing_dropdown.config(bg=ENTRY_BG, fg=LABEL_FG, relief="solid",
                            font=("Segoe UI", 12), highlightthickness=0)
    slicing_dropdown.grid(row=16, column=1, sticky="ew", pady=3)

    on_color_mode_change()

    # ---- Notes ----
    section(form_frame, "NOTES", 17)
    notes_text = tk.Text(form_frame, width=36, height=4, wrap='word',
                         font=("Segoe UI", 12), bg=PANEL_BG, fg="#1e293b",
                         relief="solid", bd=1)
    notes_text.insert('1.0', notes)
    notes_text.grid(row=18, column=0, columnspan=3, sticky="ew", pady=3)

    # ---- Submit ----
    submit_button = tk.Button(form_frame, text="Generate G-Code →", command=submit,
                              bg=ACCENT, fg="white", relief="flat",
                              font=("Segoe UI", 12, "bold"), padx=16, pady=6,
                              activebackground="#1d4ed8", activeforeground="white",
                              cursor="hand2")
    submit_button.grid(row=19, column=0, columnspan=3, pady=(16, 2))

    def visualize_existing():
        global visualize_only, visualize_gcode_path
        path = filedialog.askopenfilename(
            title="Select G-Code file",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        if path:
            visualize_only = True
            visualize_gcode_path = path
            root.quit()
            root.destroy()

    viz_button = tk.Button(form_frame, text="Visualize existing gcode", command=visualize_existing,
                           bg=BG, fg="#64748b", relief="flat",
                           font=("Segoe UI", 9), padx=6, pady=1,
                           activeforeground="#1e293b", cursor="hand2")
    viz_button.grid(row=20, column=0, columnspan=3, pady=(0, 4))

    # ---- Engineering drawing ----
    fig, ax = plt.subplots(figsize=(5, 7))
    fig.patch.set_facecolor(PANEL_BG)
    canvas = FigureCanvasTkAgg(fig, master=draw_frame)
    canvas.get_tk_widget().pack(fill="both", expand=True)

    # ---- Stats below drawing ----
    stats_frame = tk.Frame(draw_frame, bg=PANEL_BG, pady=6)
    stats_frame.pack(fill="x")
    tk.Frame(stats_frame, bg=BORDER, height=1).pack(fill="x", pady=(0, 6))
    stat_a_label = tk.Label(stats_frame, text="Longest A cable: —", bg=PANEL_BG,
                            fg=LABEL_FG, font=("Segoe UI", 12), anchor="w")
    stat_a_label.pack(fill="x", padx=8)
    stat_b_label = tk.Label(stats_frame, text="Longest B cable: —", bg=PANEL_BG,
                            fg=LABEL_FG, font=("Segoe UI", 12), anchor="w")
    stat_b_label.pack(fill="x", padx=8)

    # Bind live update to all relevant entries
    for widget in [file_path_entry, width_entry, pixel_size_entry, nozzles_entry,
                   cable_sepperation_entry, dist_from_pulley_entry,
                   floor_dist_entry, chassis_below_entry, offset_entry]:
        widget.bind("<KeyRelease>", update_drawing)

    update_drawing()
    root.mainloop()

def validate_image(file_path):
    return utils.validate_image(file_path)


def resize_image(file_path, new_width):
    """
    Resizes the input image to the specified width while maintaining the aspect ratio.
    If the image is already at the desired width, it does not resize it.
    """
    try:
        if not os.path.isfile(file_path):
            raise FileNotFoundError(f"File not found: {file_path}")
        with Image.open(file_path) as img:
            # Validate image format
            if img.format not in ['JPEG', 'PNG', 'BMP', 'GIF', 'TIFF']:
                raise ValueError(f"Unsupported image format: {img.format}")
            # Preserve the alpha channel if present
            if img.mode in ('RGBA', 'LA') or (img.mode == 'P' and 'transparency' in img.info):
                img = img.convert('RGBA')
            else:
                img = img.convert('RGB')
            original_width, original_height = img.size
            if original_width == new_width:
                print(f"Image is already at the desired width of {new_width}px. Resizing not needed.")
                img.save(reduced_image_path)
            else:
                new_height = int((new_width / original_width) * original_height)
                resized_img = img.resize((new_width, new_height), Image.LANCZOS)
                resized_img.save(reduced_image_path)
                print(f"Image resized and saved as {reduced_image_path}")
    except Exception as e:
        print(f"Error resizing image: {e}")


def simplify_image_pillow(reduced_image_path, num_colors):
    """
    Simplifies the image by reducing it to a specified number of colors using the Pillow library.
    """
    try:
        # Load image
        image = Image.open(reduced_image_path)
        # Preserve alpha channel if present
        has_alpha = image.mode == 'RGBA'
        if has_alpha:
            alpha = image.split()[-1]
            image = image.convert('RGB')
        else:
            alpha = None

        # Quantize to N colors then convert back to RGBA/RGB so the saved file
        # is a standard mode PNG that PIL can reliably reopen.
        simplified_image = image.convert(mode='P', palette=Image.ADAPTIVE, colors=num_colors)
        if has_alpha:
            simplified_image = simplified_image.convert('RGBA')
            simplified_image.putalpha(alpha)
        else:
            simplified_image = simplified_image.convert('RGB')

        # Save the new image
        simplified_image.save(processed_image_path)
        print(f'Simplified image saved as {processed_image_path}')
    except Exception as e:
        print(f"Error simplifying image: {e}")


def exact_color_match(reduced_image_path):
    """
    Like simplify_image_pillow but auto-detects the palette size from the image.
    Errors out if the image has more than 10 unique colors.
    """
    try:
        image = Image.open(reduced_image_path)
        has_alpha = image.mode == 'RGBA'
        if has_alpha:
            alpha = image.split()[-1]
            rgb_image = image.convert('RGB')
        else:
            alpha = None
            rgb_image = image.convert('RGB')

        unique_colors = len(set(rgb_image.getdata()))
        if unique_colors > 10:
            raise ValueError(
                f"this file is not meant for this mode reduce number of colors in your image "
                f"(found {unique_colors} unique colors, max is 10)"
            )

        simplified_image = rgb_image.convert(mode='P', palette=Image.ADAPTIVE, colors=unique_colors)
        if has_alpha:
            simplified_image = simplified_image.convert('RGBA')
            simplified_image.putalpha(alpha)
        else:
            simplified_image = simplified_image.convert('RGB')

        simplified_image.save(processed_image_path)
        print(f'Exact color match image saved as {processed_image_path} ({unique_colors} colors)')
    except Exception as e:
        print(f"Error in exact color match: {e}")
        raise


def process_image_rgb_scatter_nxn(image_path, n):
    print(f'Processing RGB Scatter {n}x{n}')
    try:
        # Open the input image and preserve alpha channel if present
        original_img = Image.open(image_path)
        has_alpha = original_img.mode in ('RGBA', 'LA')
        if has_alpha:
            original_img = original_img.convert('RGBA')
        else:
            original_img = original_img.convert('RGB')
        width, height = original_img.size

        # Create a new image with n times the width and height
        new_width = width * n
        new_height = height * n
        if has_alpha:
            new_img = Image.new('RGBA', (new_width, new_height), (255, 255, 255, 0))
        else:
            new_img = Image.new('RGB', (new_width, new_height), 'white')

        # Load pixel data for fast access
        original_pixels = original_img.load()
        new_pixels = new_img.load()

        # Process each pixel
        for x in range(width):
            for y in range(height):
                # Map y to start from the bottom
                orig_y = height - y - 1
                # Get the RGBA or RGB values
                pixel = original_pixels[x, orig_y]
                if has_alpha:
                    r, g, b, a = pixel
                    if a == 0:
                        continue  # Skip transparent pixels
                else:
                    r, g, b = pixel

                # Function to assign values based on RGB ranges
                def assign_value(component):
                    return max(1, int((component / 255.0) * (n * n / 3)))

                r_value = assign_value(r)
                g_value = assign_value(g)
                b_value = assign_value(b)

                total_pixels = r_value + g_value + b_value
                total_pixels = min(total_pixels, n * n)  # Ensure we don't exceed the block size

                positions = [(i, j) for i in range(n) for j in range(n)]
                random_positions = random.sample(positions, total_pixels)

                # Place red pixels
                for _ in range(r_value):
                    if random_positions:
                        pos = random_positions.pop()
                        new_x = x * n + pos[0]
                        new_y = new_height - (y + 1) * n + pos[1]
                        new_pixels[new_x, new_y] = (255, 0, 0, 255) if has_alpha else (255, 0, 0)
                    else:
                        break

                # Place green pixels
                for _ in range(g_value):
                    if random_positions:
                        pos = random_positions.pop()
                        new_x = x * n + pos[0]
                        new_y = new_height - (y + 1) * n + pos[1]
                        new_pixels[new_x, new_y] = (0, 255, 0, 255) if has_alpha else (0, 255, 0)
                    else:
                        break

                # Place blue pixels
                for _ in range(b_value):
                    if random_positions:
                        pos = random_positions.pop()
                        new_x = x * n + pos[0]
                        new_y = new_height - (y + 1) * n + pos[1]
                        new_pixels[new_x, new_y] = (0, 0, 255, 255) if has_alpha else (0, 0, 255)
                    else:
                        break

        # Save the new image
        new_img.save(processed_image_path)
    except Exception as e:
        print(f"Error in RGB Scatter NxN processing: {e}")


def process_image_with_dynamic_base_colors_nxn(image_path, n_base_colors, n):
    print(f'Processing Dynamic Scatter {n}x{n}')
    try:
        # Open the input image and preserve alpha channel if present
        original_img = Image.open(image_path)
        has_alpha = original_img.mode in ('RGBA', 'LA')
        if has_alpha:
            original_img = original_img.convert('RGBA')
        else:
            original_img = original_img.convert('RGB')
        width, height = original_img.size

        # Create a new image with n times the width and height
        new_width = width * n
        new_height = height * n
        if has_alpha:
            new_img = Image.new('RGBA', (new_width, new_height), (255, 255, 255, 0))
        else:
            new_img = Image.new('RGB', (new_width, new_height), 'white')

        # Extract pixels from the original image for color clustering
        pixels = np.array(original_img)
        if has_alpha:
            # Exclude transparent pixels
            mask = pixels[:, :, 3] > 0
            pixels_rgb = pixels[:, :, :3][mask]
        else:
            pixels_rgb = pixels.reshape(-1, 3)

        # Step 1: Use K-means to find the base colors
        kmeans = KMeans(n_clusters=n_base_colors, random_state=42)
        kmeans.fit(pixels_rgb)
        base_colors_rgb = kmeans.cluster_centers_.astype(int)
        base_colors_rgb = [tuple(color) for color in base_colors_rgb]

        # Load pixel data for fast access
        original_pixels = original_img.load()
        new_pixels = new_img.load()

        # Process each pixel in the original image
        for x in range(width):
            for y in range(height):
                # Get the RGBA values of the original pixel
                pixel = original_pixels[x, y]
                if has_alpha:
                    r, g, b, a = pixel
                    if a == 0:
                        continue  # Skip fully transparent pixels
                    # Calculate transparency factor (0 to 1)
                    transparency = a / 255.0
                else:
                    r, g, b = pixel
                    transparency = 1.0  # No transparency

                # Calculate the relative contribution of each base color
                target_color = np.array([r, g, b])
                color_distances = [np.linalg.norm(target_color - np.array(color)) for color in base_colors_rgb]
                contributions = np.array(color_distances)
                contributions = 1 / (contributions + 1e-5)  # Inverse distance weighting
                contributions /= contributions.sum()  # Normalize to sum to 1

                # Calculate total number of pixels based on transparency
                total_pixels = int(round(n * n * transparency))

                # Calculate the number of pixels for each base color
                if total_pixels > 0:
                    pixel_counts = (contributions * total_pixels).round().astype(int)
                    
                    # Ensure the sum matches total_pixels by adjusting for rounding errors
                    while pixel_counts.sum() < total_pixels:
                        pixel_counts[np.argmin(pixel_counts)] += 1
                    while pixel_counts.sum() > total_pixels:
                        pixel_counts[np.argmax(pixel_counts)] -= 1
                else:
                    continue  # Skip if no pixels should be drawn

                # Fill the NxN block with the assigned pixels
                block_x = x * n
                block_y = y * n

                # Assign colors to each pixel in the block based on calculated pixel counts
                positions = [(i, j) for i in range(n) for j in range(n)]
                random.shuffle(positions)
                pos_index = 0

                for color_index, count in enumerate(pixel_counts):
                    for _ in range(count):
                        if pos_index < len(positions):
                            px_offset, py_offset = positions[pos_index]
                            px = block_x + px_offset
                            py = block_y + py_offset
                            color = base_colors_rgb[color_index]
                            if has_alpha:
                                new_pixels[px, py] = (*color, 255)  # Full opacity for placed pixels
                            else:
                                new_pixels[px, py] = color
                            pos_index += 1

        # Save the new image
        new_img.save(processed_image_path)
    except Exception as e:
        print(f"Error in Dynamic Scatter NxN processing: {e}")


def process_image_rgb(image_path):
    try:
        # Open the input image and preserve alpha channel if present
        original_img = Image.open(image_path)
        has_alpha = original_img.mode in ('RGBA', 'LA')
        if has_alpha:
            original_img = original_img.convert('RGBA')
        else:
            original_img = original_img.convert('RGB')
        width, height = original_img.size

        # Create a new image with 3 times the width and height
        new_width = width * 3
        new_height = height * 3
        if has_alpha:
            new_img = Image.new('RGBA', (new_width, new_height), (0, 0, 0, 0))
        else:
            new_img = Image.new('RGB', (new_width, new_height), 'black')

        # Load pixel data for fast access
        original_pixels = original_img.load()
        new_pixels = new_img.load()

        # Process each pixel starting from the bottom-left corner
        for x in range(width):
            for y in range(height):
                # Map y to start from the bottom
                orig_y = height - y - 1
                # Get the RGBA or RGB values of the original pixel
                pixel = original_pixels[x, orig_y]
                if has_alpha:
                    r, g, b, a = pixel
                    if a == 0:
                        continue  # Skip transparent pixels
                else:
                    r, g, b = pixel

                # Function to assign values based on RGB ranges
                def assign_value(component):
                    if 0 <= component <= 85:
                        return 1
                    elif 86 <= component <= 172:
                        return 2
                    else:  # 173 to 255
                        return 3

                # Assign values for red, green, and blue components
                r_value = assign_value(r)
                g_value = assign_value(g)
                b_value = assign_value(b)

                # Calculate the position of the 3x3 block in the new image
                block_x = x * 3
                block_y = new_height - (y + 1) * 3  # Start from bottom

                # Stack red pixels in the first column from bottom to top
                for i in range(r_value):
                    new_pixels[block_x, block_y + i] = (255, 0, 0, 255) if has_alpha else (255, 0, 0)
                # Stack green pixels in the second column from bottom to top
                for i in range(g_value):
                    new_pixels[block_x + 1, block_y + i] = (0, 255, 0, 255) if has_alpha else (0, 255, 0)
                # Stack blue pixels in the third column from bottom to top
                for i in range(b_value):
                    new_pixels[block_x + 2, block_y + i] = (0, 0, 255, 255) if has_alpha else (0, 0, 255)

        # Save the new image
        new_img.save(processed_image_path)
    except Exception as e:
        print(f"Error processing image in RGB mode: {e}")


def process_image_cmy(image_path):
    try:
        # Open the input image and preserve alpha channel if present
        original_img = Image.open(image_path)
        has_alpha = original_img.mode in ('RGBA', 'LA')
        if has_alpha:
            # CMYK does not support alpha, so we need to separate it
            alpha = original_img.split()[-1]
            original_img = original_img.convert('CMYK')
        else:
            original_img = original_img.convert('CMYK')
        width, height = original_img.size

        # Create a new image with 3 times the width and height
        new_width = width * 3
        new_height = height * 3
        if has_alpha:
            new_img = Image.new('RGBA', (new_width, new_height), (0, 0, 0, 0))
        else:
            new_img = Image.new('RGB', (new_width, new_height), 'black')

        # Load pixel data for fast access
        original_pixels = original_img.load()
        new_pixels = new_img.load()

        # Process each pixel starting from the bottom-left corner
        for x in range(width):
            for y in range(height):
                # Map y to start from the bottom
                orig_y = height - y - 1
                # Get the CMYK values of the original pixel
                c, m, y_val, k = original_pixels[x, orig_y]

                if has_alpha:
                    a = alpha.getpixel((x, orig_y))
                    if a == 0:
                        continue  # Skip transparent pixels
                else:
                    a = 255  # Fully opaque

                # Adjust CMY values by combining with K (black) component
                c = min(255, c + k)
                m = min(255, m + k)
                y_val = min(255, y_val + k)

                # Function to assign values based on CMY ranges
                def assign_value(component):
                    if 0 <= component <= 85:
                        return 1
                    elif 86 <= component <= 172:
                        return 2
                    else:  # 173 to 255
                        return 3

                # Assign values for cyan, magenta, and yellow components
                c_value = assign_value(c)
                m_value = assign_value(m)
                y_value = assign_value(y_val)

                # Calculate the position of the 3x3 block in the new image
                block_x = x * 3
                block_y = new_height - (y + 1) * 3  # Start from bottom

                # Stack cyan pixels in the first column from bottom to top
                for i in range(c_value):
                    new_pixels[block_x, block_y + i] = (0, 255, 255, a) if has_alpha else (0, 255, 255)
                # Stack magenta pixels in the second column from bottom to top
                for i in range(m_value):
                    new_pixels[block_x + 1, block_y + i] = (255, 0, 255, a) if has_alpha else (255, 0, 255)
                # Stack yellow pixels in the third column from bottom to top
                for i in range(y_value):
                    new_pixels[block_x + 2, block_y + i] = (255, 255, 0, a) if has_alpha else (255, 255, 0)

        # Save the new image
        new_img.save(processed_image_path)
    except Exception as e:
        print(f"Error processing image in CMYK mode: {e}")


def count_unique_hex_colors(image_path):
    return utils.count_unique_hex_colors(image_path)


def extract_color(image_path, hex_color):
    return utils.extract_color(image_path, hex_color, temp_images_folder)


def create_text_file(file_path):
    """
    Creates a new text file for storing G-code instructions and writes the starting lines.
    """
    return utils.create_text_file(file_path)


def append_to_text_file(line):
    return utils.append_to_text_file(line, gcode_filepath)


def generate_preview_image(selected_hex_codes):
    """
    Displays the processed image.
    """
    return utils.generate_preview_image(processed_image_path)


def length_a(x, y):
    return utils.length_a(x, y, dist_from_pulley, cable_sepperation, width, pixel_size, offset)


def length_b(x, y):
    return utils.length_b(x, y, dist_from_pulley, cable_sepperation, width, pixel_size, offset)


def generate_column_pattern(img, column_index):
    """
    Given:
      - img: a PIL image in RGBA mode
      - column_index: which column you are referring to 
    Returns:
      - A list of strings, each representing a row:
          'x' means pixel is present (alpha > 0),
          'o' means no pixel (alpha == 0).
    """
    return slicing_styles.generate_column_pattern(img, column_index, Num_nozzles)


def generate_position_data(hex_path, hex_code):
    """
    Generates position data for the painting robot by recording the coordinates of the pixels
    for a specific color and appends it to the G-code file.
    """
    return slicing_styles.generate_position_data(hex_path, hex_code, gcode_filepath, dist_from_pulley, cable_sepperation, width, pixel_size, offset, Num_nozzles)


def generate_position_data_mono_velocity_sequential_colors(hex_paths_and_codes):
    """
    Generates position data for the painting robot in a 'mono color velocity slicing' manner.
    hex_paths_and_codes: list of (hex_path, hex_code) tuples.
    """
    return slicing_styles.generate_position_data_mono_velocity_sequential_colors(hex_paths_and_codes, gcode_filepath, dist_from_pulley, cable_sepperation, width, pixel_size, Num_nozzles, offset)


def get_color_name(hex_code):
    return utils.get_color_name(hex_code)

def generate_column_pattern_multi(img, column_index, color_index_map, output_file):
    """
    Similar to generate_column_pattern, but:
    - If alpha=0 => 'x' (and print a console message).
    - Otherwise => the color index (as a digit/char).
    
    Writes the pattern and corresponding color names to a file.
    """
    return multi_color_slicing.generate_column_pattern_multi(img, column_index, color_index_map, output_file)

def generate_position_data_multi_color_velocity_once(
    simplified_image_path,
    all_selected_hex_codes,
    color_index_map=None,
    skip_black=False,
):
    """
    Perform multi-color velocity slicing in a single pass.
    - Scans the entire 'simplified_image_path' (which is your processed_image_path).
    - Breaks columns in 4-pixel increments (same as the mono function).
    - For each pixel:
        * If alpha=0 => prints 'x'.
        * If skip_black=True and pure black => also prints 'x' (used for RGB/CMYK modes).
        * Otherwise => prints the digit that corresponds to that color in 'all_selected_hex_codes'.
    - Prints color mapping exactly once.
    """
    return multi_color_slicing.generate_position_data_multi_color_velocity_once(
        simplified_image_path,
        all_selected_hex_codes,
        gcode_filepath,
        pixel_size,
        cable_sepperation,
        dist_from_pulley,
        width,
        Num_nozzles,
        offset,
        color_index_map=color_index_map,
        skip_black=skip_black,
    )

# Initiate the GUI at the beginning
visualize_only = False
visualize_gcode_path = None
initial_popup()

if visualize_only:
    _viewer_ns = {"__name__": "not_main"}
    with open("C:/Users/oewil/OneDrive/Desktop/Mural-Bot/mural/gcode viewer.py") as _f:
        exec(_f.read(), _viewer_ns)
    _color_map, _stripes = _viewer_ns["parse_gcode_file"](visualize_gcode_path)
    _img = _viewer_ns["create_image_from_stripes"](_color_map, _stripes)
    _final_img = _viewer_ns["add_legend_to_image"](_img, _color_map)
    plt.figure()
    plt.imshow(_final_img)
    plt.axis('off')
    plt.show()
    sys.exit(0)

# Validate the image
if not validate_image(file_path):
    print("Exiting due to invalid image file.")
    sys.exit(1)

if color_mode != 'Exact Color Match':
    resize_image(file_path, width)
else:
    # Exact Color Match uses the image as-is — just copy to reduced_image_path
    import shutil
    shutil.copy2(file_path, reduced_image_path)

try:
    # Process image based on selected color mode
    if color_mode == 'RGB':
        process_image_rgb(reduced_image_path)
    elif color_mode == 'CMYK':
        process_image_cmy(reduced_image_path)
    elif color_mode == 'Simplify Image':
        simplify_image_pillow(reduced_image_path, number_of_colors)
    elif color_mode == 'Exact Color Match':
        exact_color_match(reduced_image_path)
    elif color_mode == 'RGB Scatter NxN':
        process_image_rgb_scatter_nxn(reduced_image_path, n_value)
    elif color_mode == 'Dynamic Scatter NxN':
        process_image_with_dynamic_base_colors_nxn(reduced_image_path, number_of_colors, n_value)
    else:
        print(f"Unsupported color mode selected: {color_mode}")
except Exception as e:
    print(f"Error during image processing: {e}")
    sys.exit(1)

# Close matplotlib figures so the embedded Tk backend doesn't poison the next Tk root
import matplotlib.pyplot as _plt
_plt.close('all')

print("[DEBUG] Opening color assignment window...")
selected_hex_codes, color_index_map = count_unique_hex_colors(processed_image_path)
print(f"[DEBUG] Color window closed. Got {len(selected_hex_codes)} colors: {selected_hex_codes}")
create_text_file(gcode_filepath)

if slicing_option == "multi color velocity slicing":
    generate_position_data_multi_color_velocity_once(processed_image_path, selected_hex_codes, color_index_map, skip_black=color_mode in ('RGB', 'CMYK'))
elif slicing_option == "mono color velocity slicing":
    hex_paths_and_codes = []
    for hex_code in selected_hex_codes:
        extract_color(processed_image_path, hex_code)
        hex_code_stripped = hex_code.lstrip("#")
        hex_path = os.path.join(temp_images_folder, hex_code_stripped + ".png")
        hex_paths_and_codes.append((hex_path, hex_code))
    generate_position_data_mono_velocity_sequential_colors(hex_paths_and_codes)

# Print the global variables to verify the input data
print("File Path:", file_path)
print("Width:", width)
print("Pixel Size:", pixel_size)
print("Pulley Spacing:", cable_sepperation)
print("Distance from Pulleys to Bottom of Mural:", dist_from_pulley)
print("Offset:", offset)
print("Color Mode:", color_mode)
if color_mode in ['Simplify Image', 'Dynamic Scatter NxN']:
    print("Number of Colors:", number_of_colors)
if color_mode in ['RGB Scatter NxN', 'Dynamic Scatter NxN']:
    print("Value of N:", n_value)
print("Slicing Option:", slicing_option)
print("Nozzles per stripe:", Num_nozzles)

if slicing_option == "multi color velocity slicing":
    with open("C:/Users/oewil/OneDrive/Desktop/Mural-Bot/mural/gcode viewer.py") as f:
        exec(f.read())
else:
    generate_preview_image(selected_hex_codes)

import shutil as _shutil
_arduino_data = r"C:\Users\oewil\OneDrive\Desktop\Mural-Bot\Arduino scripts\Base Module Platformio\data"
_shutil.copy2(gcode_filepath, os.path.join(_arduino_data, os.path.basename(gcode_filepath)))
print(f"GCode also copied to {_arduino_data}")

print("GCODE GENERATOR IS DONE")
