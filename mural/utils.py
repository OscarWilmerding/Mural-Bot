import os
import math
import webcolors
from PIL import Image
import matplotlib.pyplot as plt
from datetime import datetime


# Utilities extracted from the main script. These functions are pure helpers and
# accept explicit parameters to avoid relying on globals.


def validate_image(file_path):
    try:
        with Image.open(file_path) as img:
            img.verify()
        return True
    except (IOError, SyntaxError) as e:
        print(f"Invalid image file: {file_path} - {e}")
        return False


def create_text_file(file_path):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    starting_lines = ["//this is the start of the gcode", f"// generated {timestamp}", "\n"]

    with open(file_path, 'w') as file:
        for line in starting_lines:
            file.write(line + '\n')
    print(f"File created and written to {file_path}")


def append_to_text_file(line, gcode_filepath):
    with open(gcode_filepath, 'a') as file:
        file.write(line + '\n')
    print(f"Line appended to {gcode_filepath}")


def generate_preview_image(processed_image_path):
    try:
        with Image.open(processed_image_path) as img:
            plt.imshow(img)
            plt.axis('off')
            plt.show()
    except Exception as e:
        print(f"Error displaying processed image: {e}")


def length_a(x, y, dist_from_pulley, cable_sepperation, width, pixel_size, offset=0.0):
    y_component = dist_from_pulley - y * pixel_size
    x_horizontal_no_offset = (cable_sepperation / 2) - ((width * pixel_size) / 2) + x * pixel_size
    x_horizontal = x_horizontal_no_offset + offset
    length_A = math.sqrt(y_component ** 2 + x_horizontal ** 2)

    print(f"\n    length_a calculation:")
    print(f"      y_component = dist_from_pulley - y*pixel_size = {dist_from_pulley} - {y}*{pixel_size} = {y_component:.4f}")
    print(f"      x_horizontal (no offset) = (cable_sep/2) - (width*pixel_size/2) + x*pixel_size")
    print(f"               = ({cable_sepperation/2:.2f}) - ({width * pixel_size / 2:.4f}) + {x * pixel_size:.4f}")
    print(f"               = {x_horizontal_no_offset:.4f}")
    print(f"      apply offset: x_horizontal = x_horizontal_no_offset + offset = {x_horizontal_no_offset:.4f} + {offset:.4f} = {x_horizontal:.4f}")
    print(f"      (positive offset shifts image right of center)")
    print(f"      length_A = sqrt({y_component:.4f}² + {x_horizontal:.4f}²) = sqrt({y_component**2:.6f} + {x_horizontal**2:.6f}) = {length_A:.6f}")

    return length_A


def length_b(x, y, dist_from_pulley, cable_sepperation, width, pixel_size, offset=0.0):
    y_component = dist_from_pulley - y * pixel_size
    x_horizontal_a = (cable_sepperation / 2) - ((width * pixel_size) / 2) + x * pixel_size + offset
    x_horizontal = cable_sepperation - x_horizontal_a
    length_B = math.sqrt(y_component ** 2 + x_horizontal ** 2)

    print(f"\n    length_b calculation:")
    print(f"      y_component = dist_from_pulley - y*pixel_size = {dist_from_pulley} - {y}*{pixel_size} = {y_component:.4f}")
    print(f"      x_horizontal_a = (cable_sep/2) - (width*pixel_size/2) + x*pixel_size + offset")
    print(f"               = ({cable_sepperation/2:.2f}) - ({width * pixel_size / 2:.4f}) + {x * pixel_size:.4f} + {offset:.4f}")
    print(f"               = {x_horizontal_a:.4f}")
    print(f"      x_horizontal = cable_sep - x_horizontal_a = {cable_sepperation} - {x_horizontal_a:.4f} = {x_horizontal:.4f}")
    print(f"      length_B = sqrt({y_component:.4f}² + {x_horizontal:.4f}²) = sqrt({y_component**2:.6f} + {x_horizontal**2:.6f}) = {length_B:.6f}")

    return length_B


def get_color_name(hex_code):
    try:
        return webcolors.hex_to_name(hex_code)
    except ValueError:
        min_diff = float("inf")
        closest_name = None
        for name, code in webcolors.CSS3_HEX_TO_NAMES.items():
            r1, g1, b1 = webcolors.hex_to_rgb(hex_code)
            r2, g2, b2 = webcolors.hex_to_rgb(code)
            diff = (r1 - r2) ** 2 + (g1 - g2) ** 2 + (b1 - b2) ** 2
            if diff < min_diff:
                min_diff = diff
                closest_name = name
        return closest_name


def count_unique_hex_colors(image_path):
    """Show a window listing all unique colors in the image.

    Returns (selected_colors, color_index_map) where color_index_map is a
    dict mapping hex string -> int index (1-based, user-editable).
    """
    try:
        import tkinter as tk
        from collections import Counter

        print(f"[DEBUG] count_unique_hex_colors called with: {image_path}")
        image = Image.open(image_path).convert("RGBA")
        pixels = list(image.getdata())
        total_opaque = sum(1 for r, g, b, a in pixels if a != 0)

        # Count pixels per color
        color_counts = Counter(
            f'#{r:02x}{g:02x}{b:02x}' for r, g, b, a in pixels if a != 0
        )

        # Auto-guess indices: white last, others sorted by pixel count desc
        white_hex = '#ffffff'
        non_white = sorted(
            [c for c in color_counts if c.lower() != white_hex],
            key=lambda c: color_counts[c], reverse=True
        )
        has_white = white_hex in color_counts
        ordered = non_white + ([white_hex] if has_white else [])

        # --- Build UI ---
        BG = "#f5f5f5"
        PANEL = "#ffffff"
        ACCENT = "#2563eb"
        FG = "#1e293b"

        root = tk.Tk()
        root.title("Color Index Assignment")
        root.configure(bg=BG)
        root.resizable(False, False)
        root.lift()
        root.attributes('-topmost', True)
        root.after(200, lambda: root.attributes('-topmost', False))

        tk.Label(root, text="Assign Nozzle Index to Each Color",
                 bg=BG, fg=ACCENT, font=("Segoe UI", 12, "bold")).pack(pady=(14, 2))
        tk.Label(root, text="Uncheck to exclude a color. Edit index to override auto-assignment.",
                 bg=BG, fg="#64748b", font=("Segoe UI", 12)).pack(pady=(0, 10))

        tk.Frame(root, bg="#e2e8f0", height=1).pack(fill="x", padx=16, pady=(0, 4))

        # Single grid table — header + rows share the same column geometry
        table = tk.Frame(root, bg=BG)
        table.pack(fill="both", expand=True, padx=16)
        # Columns: 0=check, 1=swatch, 2=hex, 3=pixels, 4=%, 5=index
        COL_PAD = 10
        for col, minw in enumerate([28, 40, 110, 90, 60, 70]):
            table.grid_columnconfigure(col, minsize=minw, pad=COL_PAD)

        # Header
        for col, text in enumerate(["", "", "Hex", "Pixels", "%", "Index"]):
            tk.Label(table, text=text, bg=BG, fg="#94a3b8",
                     font=("Segoe UI", 11, "bold"), anchor="w"
                     ).grid(row=0, column=col, sticky="w", pady=(0, 4))

        checkbox_vars = {}
        index_vars = {}

        for data_row, (auto_idx, hex_code) in enumerate(enumerate(ordered, start=1), start=1):
            count = color_counts[hex_code]
            pct = 100 * count / total_opaque if total_opaque else 0
            bg = PANEL if data_row % 2 == 0 else "#f8fafc"

            var = tk.BooleanVar(value=True)
            checkbox_vars[hex_code] = var
            tk.Checkbutton(table, variable=var, bg=bg, activebackground=bg,
                           relief="flat").grid(row=data_row, column=0, sticky="w", pady=3)

            tk.Label(table, bg=hex_code, relief="solid", bd=1, width=3
                     ).grid(row=data_row, column=1, sticky="ew", padx=2, pady=3, ipady=6)

            tk.Label(table, text=hex_code, bg=bg, fg=FG,
                     font=("Consolas", 12), anchor="w"
                     ).grid(row=data_row, column=2, sticky="w", pady=3)

            tk.Label(table, text=f"{count:,}", bg=bg, fg="#475569",
                     font=("Segoe UI", 12), anchor="e"
                     ).grid(row=data_row, column=3, sticky="e", pady=3)

            tk.Label(table, text=f"{pct:.1f}%", bg=bg, fg="#475569",
                     font=("Segoe UI", 12), anchor="e"
                     ).grid(row=data_row, column=4, sticky="e", pady=3)

            idx_var = tk.StringVar(value=str(auto_idx))
            index_vars[hex_code] = idx_var
            tk.Spinbox(table, from_=1, to=99, textvariable=idx_var, width=4,
                       font=("Segoe UI", 12), relief="solid", bd=1,
                       bg=PANEL, fg=FG).grid(row=data_row, column=5, sticky="w", pady=3)

        tk.Frame(root, bg="#e2e8f0", height=1).pack(fill="x", padx=16, pady=(8, 0))

        result = {"colors": [], "index_map": {}}

        def on_submit():
            selected = []
            index_map = {}
            for hex_code in ordered:
                if checkbox_vars[hex_code].get():
                    selected.append(hex_code)
                    try:
                        idx = int(index_vars[hex_code].get())
                    except ValueError:
                        idx = ordered.index(hex_code) + 1
                    index_map[hex_code.lower()] = idx
            result["colors"] = selected
            result["index_map"] = index_map
            root.destroy()

        def on_closing():
            on_submit()

        tk.Button(root, text="Confirm & Continue →", command=on_submit,
                  bg=ACCENT, fg="white", relief="flat",
                  font=("Segoe UI", 12, "bold"), padx=16, pady=6,
                  activebackground="#1d4ed8", activeforeground="white",
                  cursor="hand2").pack(pady=12)

        root.protocol("WM_DELETE_WINDOW", on_closing)
        print(f"[DEBUG] Entering mainloop with {len(ordered)} colors, window: {root}")
        root.mainloop()
        print("[DEBUG] mainloop exited")

        return result["colors"], result["index_map"]
    except Exception as e:
        print(f"Error counting unique colors: {e}")
        return [], {}


def extract_color(image_path, hex_color, temp_images_folder):
    try:
        hex_color_stripped = hex_color.lstrip('#')
        target_color = tuple(int(hex_color_stripped[i:i+2], 16) for i in (0, 2, 4))

        image = Image.open(image_path).convert('RGBA')
        data = image.getdata()

        new_image = Image.new('RGBA', image.size, (255, 255, 255, 0))
        new_data = []

        for item in data:
            if item[:3] == target_color and item[3] != 0:
                new_data.append((*item[:3], 255))
            else:
                new_data.append((255, 255, 255, 0))

        new_image.putdata(new_data)
        output_path = os.path.join(temp_images_folder, hex_color_stripped + ".png")
        new_image.save(output_path, 'PNG')
        print(f"Image saved to {output_path}")
    except Exception as e:
        print(f"Error extracting color {hex_color}: {e}")
