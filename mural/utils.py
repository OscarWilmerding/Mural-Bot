import os
import math
import webcolors
from PIL import Image
import matplotlib.pyplot as plt


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
    starting_lines = ["//this is the start of the gcode", "\n"]

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


def length_a(x, y, dist_from_pulley, cable_sepperation, width, pixel_size, offset):
    length_A = math.sqrt(
        (dist_from_pulley - y * pixel_size) ** 2 +
        (cable_sepperation - (
            cable_sepperation - ((cable_sepperation / 2) -
            ((width * pixel_size) / 2) + x * pixel_size - offset)
        )) ** 2
    )
    return length_A


def length_b(x, y, dist_from_pulley, cable_sepperation, width, pixel_size, offset):
    length_B = math.sqrt(
        (dist_from_pulley - y * pixel_size) ** 2 +
        (cable_sepperation - (
            (cable_sepperation / 2) - ((width * pixel_size) / 2) +
            x * pixel_size - offset
        )) ** 2
    )
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
    try:
        image = Image.open(image_path)
        image = image.convert("RGBA")
        pixels = list(image.getdata())
        hex_codes = {f'#{r:02x}{g:02x}{b:02x}' for r, g, b, a in pixels if a != 0}

        import tkinter as tk

        root = tk.Tk()
        root.title("Unique Colors")

        frame = tk.Frame(root)
        frame.pack(fill=tk.BOTH, expand=True)

        checkbox_vars = {}

        selected_colors = []

        def on_closing():
            nonlocal selected_colors
            selected_colors = [color for color, var in checkbox_vars.items() if var.get()]
            root.destroy()

        for hex_code in hex_codes:
            color_frame = tk.Frame(frame, borderwidth=1, relief="solid")
            color_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

            var = tk.BooleanVar(value=True)
            checkbox_vars[hex_code] = var
            checkbox = tk.Checkbutton(color_frame, variable=var)
            checkbox.pack(side=tk.LEFT)

            color_label = tk.Label(
                color_frame,
                text=hex_code,
                bg=hex_code,
                font=("Arial", 12),
                fg="white" if int(hex_code[1:], 16) < 0x888888 else "black",
            )
            color_label.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10, pady=10)

        root.protocol("WM_DELETE_WINDOW", on_closing)
        root.mainloop()

        return selected_colors
    except Exception as e:
        print(f"Error counting unique colors: {e}")
        return []


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
