# swap12_inplace.py
import re
import pathlib
import shutil

FILE = r"C:/Users/oewil/OneDrive/Desktop/Mural-Bot/mural/gcode.txt"  # set this to your text file

pat = re.compile(r'(["\'])([12]{4})\1')  # matches "1111", '1212', etc.

def repl(m):
    swapped = m.group(2).translate(str.maketrans("12", "21"))
    return f'{m.group(1)}{swapped}{m.group(1)}'

p = pathlib.Path(FILE)
text = p.read_text(encoding="utf-8")

# optional backup
shutil.copy2(p, p.with_suffix(p.suffix + ".bak"))

out = pat.sub(repl, text)
p.write_text(out, encoding="utf-8")
