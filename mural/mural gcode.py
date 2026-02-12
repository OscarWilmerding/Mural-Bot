import tkinter as tk
from tkinter import filedialog
from PIL import Image
import os
import numpy as np
import matplotlib.pyplot as plt
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

reduced_image_path = os.path.join(root_folder, "temp.png")
processed_image_path = os.path.join(root_folder, "processed_image_path.png")
preview_image_path = os.path.join(root_folder, "preview_image.png")

# Create the temp_images folder
temp_images_folder = os.path.join(root_folder, 'temp_images')
# Ensure temp_images_folder exists
if not os.path.exists(temp_images_folder):
    os.makedirs(temp_images_folder)
    print(f"Created directory: {temp_images_folder}")

# Global variables - THESE ALSO SET AUTOFILLED DEFAULTS
file_path = r"C:/Users/oewil/OneDrive/Desktop/Mural-Bot/mural/imput images/deadman.jpg"
width = 300  # in pixels (should be an integer)
pixel_size = 0.002  # size of each pixel in meters
cable_sepperation = 1.265  # in meters (this is the pulley spacing)
dist_from_pulley = 1.0  # distance from pulleys to bottom of mural (m)
offset = 0.0  # offset of image from center to the left (m)
color_mode = 'Simplify Image'  # default color mode
number_of_colors = 2  # default number of colors for 'Simplify Image' method
n_value = 3  # default N value for NxN methods
peak_velocity = 0.5  # in meters per sec
slicing_option = 'multi color velocity slicing'  # default slicing option
Num_nozzles = 6 # number of nozzles / pixels per stripe (was hardcoded as 4)


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
    Displays a GUI popup to collect user inputs for various parameters.
    """
    def browse_file():
        global file_path
        filetypes = (
            ('Image files', '*.png *.jpg *.jpeg *.bmp *.gif *.tiff'),
            ('All files', '*.*')
        )
        file_path = filedialog.askopenfilename(filetypes=filetypes)
        file_path_entry.delete(0, tk.END)
        file_path_entry.insert(0, file_path)

    def submit():
        global file_path, width, pixel_size, number_of_colors
        global cable_sepperation, dist_from_pulley, offset, color_mode, n_value
        global slicing_option
        file_path = file_path_entry.get()
        width = int(width_entry.get())
        pixel_size = float(pixel_size_entry.get())
        cable_sepperation = float(cable_sepperation_entry.get())
        dist_from_pulley = float(dist_from_pulley_entry.get())
        offset = float(offset_entry.get())
        color_mode = color_mode_var.get()
        if color_mode in ['Simplify Image', 'Dynamic Scatter NxN']:
            number_of_colors = int(number_of_colors_entry.get())
        if color_mode in ['RGB Scatter NxN', 'Dynamic Scatter NxN']:
            n_value = int(n_value_entry.get())
        slicing_option = slicing_option_var.get()
        root.destroy()

    def on_color_mode_change(*args):
        selected_mode = color_mode_var.get()
        # Show/hide Number of Colors
        if selected_mode in ['Simplify Image', 'Dynamic Scatter NxN']:
            number_of_colors_label.grid(row=2, column=0, padx=10, pady=10)
            number_of_colors_entry.grid(row=2, column=1, padx=10, pady=10)
        else:
            number_of_colors_label.grid_remove()
            number_of_colors_entry.grid_remove()
        # Show/hide N value
        if selected_mode in ['RGB Scatter NxN', 'Dynamic Scatter NxN']:
            n_value_label.grid(row=3, column=0, padx=10, pady=10)
            n_value_entry.grid(row=3, column=1, padx=10, pady=10)
        else:
            n_value_label.grid_remove()
            n_value_entry.grid_remove()

    root = tk.Tk()
    root.title("Input Window")

    tk.Label(root, text="File Path:").grid(row=0, column=0, padx=10, pady=10)
    file_path_entry = tk.Entry(root, width=40)
    file_path_entry.grid(row=0, column=1, padx=10, pady=10)
    file_path_entry.insert(0, file_path)
    browse_button = tk.Button(root, text="Browse", command=browse_file)
    browse_button.grid(row=0, column=2, padx=10, pady=10)

    tk.Label(root, text="Width in Pixels:").grid(row=1, column=0, padx=10, pady=10)
    width_entry = tk.Entry(root)
    width_entry.grid(row=1, column=1, padx=10, pady=10)
    width_entry.insert(0, str(width))

    # Number of Colors (initially hidden)
    number_of_colors_label = tk.Label(root, text="Number of Colors:")
    number_of_colors_entry = tk.Entry(root)
    number_of_colors_entry.insert(0, str(number_of_colors))
    number_of_colors_label.grid_remove()
    number_of_colors_entry.grid_remove()

    # N Value for NxN methods (initially hidden)
    n_value_label = tk.Label(root, text="Value of N:")
    n_value_entry = tk.Entry(root)
    n_value_entry.insert(0, str(n_value))
    n_value_label.grid_remove()
    n_value_entry.grid_remove()

    # Pixel Size (m)
    pixel_size_label = tk.Label(root, text="Pixel Size (m):")
    pixel_size_label.grid(row=4, column=0, padx=10, pady=10)
    pixel_size_entry = tk.Entry(root)
    pixel_size_entry.grid(row=4, column=1, padx=10, pady=10)
    pixel_size_entry.insert(0, str(pixel_size))

    # Pulley Spacing (m)
    cable_sepperation_label = tk.Label(root, text="Pulley Spacing (m):")
    cable_sepperation_label.grid(row=5, column=0, padx=10, pady=10)
    cable_sepperation_entry = tk.Entry(root)
    cable_sepperation_entry.grid(row=5, column=1, padx=10, pady=10)
    cable_sepperation_entry.insert(0, str(cable_sepperation))

    # Distance from Pulleys to Bottom of Mural (m)
    dist_from_pulley_label = tk.Label(root, text="Distance from Pulleys to Bottom of Mural (m):")
    dist_from_pulley_label.grid(row=6, column=0, padx=10, pady=10)
    dist_from_pulley_entry = tk.Entry(root)
    dist_from_pulley_entry.grid(row=6, column=1, padx=10, pady=10)
    dist_from_pulley_entry.insert(0, str(dist_from_pulley))

    # Offset of Image from Center to the Left (m)
    offset_label = tk.Label(root, text="Offset of Image from Center to the Left (m):")
    offset_label.grid(row=7, column=0, padx=10, pady=10)
    offset_entry = tk.Entry(root)
    offset_entry.grid(row=7, column=1, padx=10, pady=10)
    offset_entry.insert(0, str(offset))

    # Color Mode Dropdown
    color_mode_var = tk.StringVar(value=color_mode)
    color_mode_var.trace('w', on_color_mode_change)  # Trace changes
    tk.Label(root, text="Color Mode:").grid(row=8, column=0, padx=10, pady=10)
    color_options = ['RGB', 'CMYK', 'Simplify Image', 'RGB Scatter NxN', 'Dynamic Scatter NxN']
    color_dropdown = tk.OptionMenu(root, color_mode_var, *color_options)
    color_dropdown.grid(row=8, column=1, padx=10, pady=10)
    
    # Trigger initial visibility based on default color_mode
    on_color_mode_change()

    # Slicing Options Dropdown
    slicing_option_var = tk.StringVar(value=slicing_option)  # default value
    tk.Label(root, text="Slicing Options:").grid(row=9, column=0, padx=10, pady=10)

    # UPDATED slicing_options: 'mono color velocity slicing' (old velocity) + new 'multi color velocity slicing'
    slicing_options = ['position slicing', 'mono color velocity slicing', 'multi color velocity slicing']
    slicing_dropdown = tk.OptionMenu(root, slicing_option_var, *slicing_options)
    slicing_dropdown.grid(row=9, column=1, padx=10, pady=10)

    # Notes area (read-only in the GUI; edit the text directly in the code)
    notes_label = tk.Label(root, text="Notes:")
    notes_label.grid(row=10, column=0, padx=10, pady=10)
    notes_text = tk.Text(root, width=50, height=6, wrap='word', font=("Arial", 10))
    filler = (
        "horizontal seperation = 12mm , vertical seperation = 20mm\n"
        "aiming wall width 9ft (with 12mm pixels means 228 pixels wide)\n"
    )
    notes_text.insert('1.0', filler)
    notes_text.configure(state='disabled')
    notes_text.grid(row=10, column=1, columnspan=2, padx=10, pady=10)

    submit_button = tk.Button(root, text="Submit", command=submit)
    submit_button.grid(row=11, column=0, columnspan=3, pady=20)

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

        # Convert image to use a palette with the specified number of colors
        simplified_image = image.convert(mode='P', palette=Image.ADAPTIVE, colors=num_colors)
        if has_alpha:
            simplified_image.putalpha(alpha)

        # Save the new image
        simplified_image.save(processed_image_path)
        print(f'Simplified image saved as {processed_image_path}')
    except Exception as e:
        print(f"Error simplifying image: {e}")


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


def generate_position_data_mono_velocity_sequential_colors(hex_path, hex_code):
    """
    Generates position data for the painting robot in a 'mono color velocity slicing' manner.
    """
    return slicing_styles.generate_position_data_mono_velocity_sequential_colors(hex_path, hex_code, gcode_filepath, dist_from_pulley, cable_sepperation, width, pixel_size, offset, Num_nozzles)


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
    all_selected_hex_codes
):
    """
    Perform multi-color velocity slicing in a single pass.
    - Scans the entire 'simplified_image_path' (which is your processed_image_path).
    - Breaks columns in 4-pixel increments (same as the mono function).
    - For each pixel:
        * If alpha=0 or pure black => prints 'x' and also prints a debug message.
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
        offset,
        Num_nozzles,
    )

# Initiate the GUI at the beginning
initial_popup()

# Validate the image
if not validate_image(file_path):
    print("Exiting due to invalid image file.")
    sys.exit(1)

resize_image(file_path, width)

try:
    # Process image based on selected color mode
    if color_mode == 'RGB':
        process_image_rgb(reduced_image_path)
    elif color_mode == 'CMYK':
        process_image_cmy(reduced_image_path)
    elif color_mode == 'Simplify Image':
        simplify_image_pillow(reduced_image_path, number_of_colors)
    elif color_mode == 'RGB Scatter NxN':
        process_image_rgb_scatter_nxn(reduced_image_path, n_value)
    elif color_mode == 'Dynamic Scatter NxN':
        process_image_with_dynamic_base_colors_nxn(reduced_image_path, number_of_colors, n_value)
    else:
        print(f"Unsupported color mode selected: {color_mode}")
except Exception as e:
    print(f"Error during image processing: {e}")
    sys.exit(1)

selected_hex_codes = count_unique_hex_colors(processed_image_path)
create_text_file(gcode_filepath)

if slicing_option == 'multi color velocity slicing':
    # We only need one pass on the entire simplified image.
    # We'll pass in `processed_image_path` plus the entire list of `selected_hex_codes`.
    generate_position_data_multi_color_velocity_once(processed_image_path, selected_hex_codes)
else:
    # Existing logic for 'position slicing' or 'mono color velocity slicing' 
    # that loops over each color layer, if that’s still needed
    for hex_code in selected_hex_codes:
        hex_code_stripped = hex_code.lstrip('#')
        hex_path = os.path.join(temp_images_folder, hex_code_stripped + '.png')

        if slicing_option == 'position slicing':
            generate_position_data(hex_path, hex_code)
        elif slicing_option == 'mono color velocity slicing':
            generate_position_data_mono_velocity_sequential_colors(hex_path, hex_code)

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

if slicing_option == 'multi color velocity slicing':
    with open("C:/Users/oewil/OneDrive/Desktop/Mural-Bot/mural/gcode viewer.py") as f:
        exec(f.read())
else:
    generate_preview_image(selected_hex_codes)

print('GCODE GENERATOR IS DONE')
