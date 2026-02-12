import re
import ast
from PIL import Image, ImageDraw, ImageFont
import matplotlib.pyplot as plt
import math

def parse_gcode_file(filepath):
    """
    Reads the entire text file and extracts:
      - color_map: dict of { '1': '#xxxxxx', '2': '#xxxxxx', ... , 'x': '#ffffff' }
      - stripes: list of stripe patterns [ [row0, row1, ...], [row0, row1, ...], ... ]
        Each rowN is a 4-character string (e.g. "2223").
    """
    color_map = {}
    stripes = []

    # Predefine 'x' => white in case it's missing from the file
    color_map['x'] = '#ffffff'

    # Regex to detect lines like: "Index 1 => #e59e60"
    color_line_regex = re.compile(r'Index\s+(\S+)\s*=>\s*(#[0-9a-fA-F]{6})')

    # Regex to detect start of a STRIPE block: "STRIPE - column #..."
    stripe_start_regex = re.compile(r'^STRIPE\s*-\s*column\s*#(\d+)')

    # Regex to detect "pattern: [ ... ]" - to capture the whole bracketed list
    pattern_list_regex = re.compile(r'pattern:\s*(\[.*\])')

    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Parse color mappings and stripes
    in_color_mapping = False

    current_stripe = None

    for line in lines:
        line_stripped = line.strip()

        # Detect start or end of color mapping block
        if '-- MULTI-COLOR INDEX MAPPING --' in line_stripped:
            in_color_mapping = True
            continue
        if '-- END OF COLOR MAPPING --' in line_stripped:
            in_color_mapping = False
            continue

        # If we are inside the color mapping lines, parse them
        if in_color_mapping:
            match = color_line_regex.search(line_stripped)
            if match:
                index_value = match.group(1)
                hex_color = match.group(2)
                color_map[index_value] = hex_color
            continue

        # Detect the start of a new stripe
        s_match = stripe_start_regex.search(line_stripped)
        if s_match:
            # We found a new stripe, prepare a holder for its pattern
            current_stripe = []
            continue

        # Extract the pattern array if found in the line
        p_match = pattern_list_regex.search(line_stripped)
        if p_match:
            # Convert the bracketed string "[ "2222", "2233", ... ]" into a Python list
            pattern_str = p_match.group(1)

            # ast.literal_eval can safely parse the JSON-like list of strings
            try:
                pattern_list = ast.literal_eval(pattern_str)
            except Exception:
                pattern_list = []

            # Each element in pattern_list is a string (width depends on num_nozzles, e.g. "222233" or "2223")
            # We'll store it in current_stripe
            current_stripe = pattern_list
            stripes.append(current_stripe)

    return color_map, stripes

def create_image_from_stripes(color_map, stripes):
    """
    Create a Pillow Image from the stripes + color mapping.
      - Each stripe's width is determined dynamically from the pattern strings.
      - The height is len(stripe_pattern).
      - The total width is stripe_width * len(stripes).
    """
    if not stripes:
        raise ValueError("No stripe data found.")

    # Assume all stripes have the same pattern height
    stripe_height = len(stripes[0])
    
    # Dynamically determine stripe width from the first stripe's pattern
    stripe_width = len(stripes[0][0]) if stripes[0] else 0
    
    if stripe_width == 0:
        raise ValueError("No pattern data found in stripes.")
    
    total_width = stripe_width * len(stripes)

    # Create a new image (RGBA or RGB)
    img = Image.new('RGB', (total_width, stripe_height), color='white')
    draw = ImageDraw.Draw(img)

    for s_idx, stripe in enumerate(stripes):
        for y, row_str in enumerate(stripe):
            # row_str is something like "222233" or "x1x2x1" (width determined dynamically)
            for x_offset, char in enumerate(row_str):
                # Determine color
                pixel_color = color_map.get(char, '#ffffff')  # default to white if unknown
                # Compute real X position in final image
                x = s_idx * stripe_width + x_offset
                # Draw pixel
                draw.point((x, y), fill=pixel_color)

    return img

def add_legend_to_image(img, color_map):
    """
    Adds a legend at the bottom of the given image showing "Index => Color".
    Returns a new, taller (and potentially wider) image with the legend appended.
    """
    from PIL import ImageDraw, ImageFont

    sorted_map_keys = sorted(color_map.keys(), key=lambda k: (k.isdigit(), k))
    legend_lines = len(sorted_map_keys)
    legend_height_per_line = 20
    spacing = 5

    font = ImageFont.load_default()
    dummy_img = Image.new("RGB", (1, 1))
    dummy_draw = ImageDraw.Draw(dummy_img)
    max_text_width = 0
    for idx in sorted_map_keys:
        line_text = f"Index {idx} => {color_map[idx]}"
        bbox = dummy_draw.textbbox((0, 0), line_text, font=font)
        text_width = bbox[2] - bbox[0]
        max_text_width = max(max_text_width, text_width)

    legend_required_width = 10 + 5 + max_text_width + 10
    new_width = max(img.width, legend_required_width)
    legend_height = legend_height_per_line * legend_lines + spacing * 2
    new_height = img.height + legend_height

    new_img = Image.new('RGB', (new_width, new_height), 'white')
    new_img.paste(img, (0, 0))
    draw = ImageDraw.Draw(new_img)

    text_x = 10
    text_y = img.height + spacing
    for idx in sorted_map_keys:
        color_hex = color_map[idx]
        line_text = f"Index {idx} => {color_hex}"
        swatch_size = 10
        draw.rectangle([text_x, text_y, text_x + swatch_size, text_y + swatch_size], fill=color_hex)
        draw.text((text_x + swatch_size + 5, text_y), line_text, fill='black', font=font)
        text_y += legend_height_per_line

    # Also print legend in terminal
    print("Color Index Mapping:")
    for idx in sorted_map_keys:
        print(f"  Index {idx} => {color_map[idx]}")

    return new_img
def main():
    # 1) Parse data from file
    filename = "C:/Users/oewil/OneDrive/Desktop/Mural-Bot/mural/gcode.txt"  # Replace with your gcode-like text file

    color_map, stripes = parse_gcode_file(filename)

    print("Color Index Mapping:")
    for k, v in color_map.items():
        print(f"  Index {k} => {v}")
    # 2) Create the main image
    img = create_image_from_stripes(color_map, stripes)

    # 3) Add legend
    final_img = add_legend_to_image(img, color_map)

    # 4) Display the final image using matplotlib
    plt.figure()
    plt.imshow(final_img)
    plt.axis('off')
    plt.show()

if __name__ == "__main__":
    main()
