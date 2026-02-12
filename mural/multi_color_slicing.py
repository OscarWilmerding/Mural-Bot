from PIL import Image
import json
import utils


# State used to ensure the color mapping is printed only once
HAS_PRINTED_COLOR_MAPPING = False


def generate_column_pattern_multi(img, column_index, color_index_map, output_file):
    width, height = img.size
    col_start = num_nozzles * column_index

    with open(output_file, 'w') as f:
        for y in range(height):
            row_pattern = ""
            color_comments = []

            for offset in range(num_nozzles):
                x_coord = col_start + offset
                if x_coord < 0 or x_coord >= width:
                    row_pattern += "x"
                    continue

                r, g, b, a = img.getpixel((x_coord, y))
                if a == 0:
                    row_pattern += "x"
                else:
                    hex_code = "#{:02x}{:02x}{:02x}".format(r, g, b)
                    if hex_code not in color_index_map:
                        print(f"[DEBUG] Pixel color {hex_code} not in color_index_map—treating as transparent.")
                        row_pattern += "x"
                    else:
                        row_pattern += str(color_index_map[hex_code])
                    color_comments.append(utils.get_color_name(hex_code))

            f.write(f"{y}: {row_pattern}\n")
            if color_comments:
                f.write(f"// {', '.join(color_comments)}\n")


def generate_position_data_multi_color_velocity_once(simplified_image_path, all_selected_hex_codes, gcode_filepath, pixel_size, cable_sepperation, dist_from_pulley, width, offset, num_nozzles):
    """Perform multi-color velocity slicing and write results to the given gcode file.
    This function mirrors the original behavior exactly.
    """
    global HAS_PRINTED_COLOR_MAPPING

    try:
        img = Image.open(simplified_image_path).convert('RGBA')
        w, h = img.size

        color_index_map = {}
        for i, hex_col in enumerate(all_selected_hex_codes, start=1):
            color_index_map[hex_col.lower()] = i

        if not HAS_PRINTED_COLOR_MAPPING:
            with open(gcode_filepath, 'a') as f:
                f.write("\n-- MULTI-COLOR INDEX MAPPING --\n")
                for i, hex_col in enumerate(all_selected_hex_codes, start=1):
                    f.write(f"Index {i} => {hex_col}\n")
                f.write("-- END OF COLOR MAPPING --\n\n")
            HAS_PRINTED_COLOR_MAPPING = True

        with open(gcode_filepath, 'a') as f:
            number_of_drawn_columns = w // num_nozzles
            f.write(f"number of drawn columns = {number_of_drawn_columns}\n")
            f.write(f"pulley spacing = {cable_sepperation}\n")
            f.write("BEGIN MULTI-COLOR VELOCITY SLICING\n")

            for c in range(number_of_drawn_columns):
                f.write(f"STRIPE - column #{c}\n")
                start_x = (c * num_nozzles) - (num_nozzles // 2)
                f.write(f"starting/ending position pixel values:  ({start_x},{h}),({start_x},{0})\n")

                patterns = []
                col_start = num_nozzles * c

                for y in range(h):
                    row_pattern = ""
                    for offset in range(num_nozzles):
                        x_coord = col_start + offset
                        if x_coord < 0 or x_coord >= w:
                            row_pattern += "x"
                            continue

                        r, g, b, a = img.getpixel((x_coord, y))
                        if a == 0 or (r == 0 and g == 0 and b == 0):
                            row_pattern += "x"
                        else:
                            px_hex = "#{:02x}{:02x}{:02x}".format(r, g, b).lower()
                            if px_hex in color_index_map:
                                row_pattern += str(color_index_map[px_hex])
                            else:
                                print(f"[DEBUG] Unrecognized color {px_hex}; treating as transparent.")
                                row_pattern += "x"

                    patterns.append(row_pattern)

                f.write('pattern: ' + json.dumps(patterns) + "\n")

                drop_val = pixel_size * h
                f.write(f"drop: {drop_val}\n")

                la = round(utils.length_a((c*4)-2, h, dist_from_pulley, cable_sepperation, width, pixel_size, offset), 6)
                lb = round(utils.length_b((c*4)-2, h, dist_from_pulley, cable_sepperation, width, pixel_size, offset), 6)
                f.write(f"starting pulley values:  {la},{lb}\n")

            f.write("END MULTI-COLOR VELOCITY SLICING\n")

        print("Multi-color velocity slicing complete.")

    except Exception as e:
        print(f"Error generating multi-color velocity slicing: {e}")
