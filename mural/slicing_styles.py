from PIL import Image
import json
import random
import os
import utils


def generate_column_pattern(img, column_index, num_nozzles):
    width, height = img.size
    col_start = num_nozzles * column_index

    patterns = []
    for y in range(height):
        row_pattern = ""
        for offset in range(num_nozzles):
            x_coord = col_start + offset
            if x_coord < 0 or x_coord >= width:
                row_pattern += "o"
                continue
            r, g, b, a = img.getpixel((x_coord, y))
            row_pattern += "x" if a > 0 else "o"
        patterns.append(row_pattern)
    return patterns


def generate_position_data(hex_path, hex_code, gcode_filepath, dist_from_pulley, cable_sepperation, width, pixel_size, offset, num_nozzles):
    try:
        img = Image.open(hex_path).convert('RGBA')
        w, h = img.size

        with open(gcode_filepath, 'a') as f:
            f.write(f"change color to:{hex_code}\n")
            for x in range(w):
                for y in range(h):
                    r, g, b, a = img.getpixel((x, y))
                    if a != 0:
                        y_flipped = h - 1 - y
                        length_a_value = utils.length_a(x, y_flipped, dist_from_pulley, cable_sepperation, width, pixel_size, offset)
                        length_b_value = utils.length_b(x, y_flipped, dist_from_pulley, cable_sepperation, width, pixel_size, offset)
                        f.write(f"({length_a_value},{length_b_value}),({x},{y_flipped}),{hex_code}\n")

        print(f"Position data has been appended to {gcode_filepath}")
    except Exception as e:
        print(f"Error generating position data: {e}")


def generate_position_data_mono_velocity_sequential_colors(hex_path, hex_code, gcode_filepath, dist_from_pulley, cable_sepperation, width, pixel_size, offset, num_nozzles):
    try:
        print("flag 1")
        img = Image.open(hex_path).convert('RGBA')
        w, h = img.size
        number_of_drawn_columns = w // num_nozzles

        with open(gcode_filepath, 'a') as f:
            f.write(f"number of drawn columns = {number_of_drawn_columns}\n")
            f.write(f"pulley spacing = {cable_sepperation}\n")

            f.write(f"change color to:{hex_code}\n")
            for c in range(number_of_drawn_columns):
                f.write(f"STRIPE - column #{c}\n")
                start_x = (c * num_nozzles) - (num_nozzles // 2)
                f.write(f"starting/ending position pixel values:  ({start_x},{h}),({start_x},{0})\n")
                f.write(str(generate_column_pattern(img, c, num_nozzles)) + "\n")
                f.write(
                    f"drop: {pixel_size*h}\n"
                    f"starting pulley values:  "
                    f"{round(utils.length_a(start_x, h, dist_from_pulley, cable_sepperation, width, pixel_size, offset), 6)},"
                    f"{round(utils.length_b(start_x, h, dist_from_pulley, cable_sepperation, width, pixel_size, offset), 6)}\n"
                )

    except Exception as e:
        print(f"Error generating position data: {e}")


def generate_column_pattern_multi(img, column_index, color_index_map, output_file):
    width, height = img.size
    col_start = 4 * column_index
    # This function was moved to `multi_color_slicing.py` to separate multi-color logic.
    raise NotImplementedError("generate_column_pattern_multi moved to multi_color_slicing.py")


def generate_position_data_multi_color_velocity_once(*args, **kwargs):
    raise NotImplementedError("generate_position_data_multi_color_velocity_once moved to multi_color_slicing.py")
