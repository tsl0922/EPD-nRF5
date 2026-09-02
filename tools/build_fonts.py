#!/usr/bin/env python3
import subprocess
import os
import sys

FONT_TXT = "font.txt"

FONTS = [
    {
        'name': f"u8g2_font_wqy9_t_lunar",
        'bdf': f"fonts/wenquanyi_9ptb.bdf",
        'ascii': "32-128",
    },
    {
        'name': f"u8g2_font_wqy12_t_lunar",
        'bdf': f"fonts/wenquanyi_12ptb.bdf",
        'ascii': "48-57",
    }
]

EXTRA_FONTS = [
    f"fonts/u8g2_font_helvB14_tn.c",
    f"fonts/u8g2_font_helvB18_tn.c",
]

OUT_FILES = [f"_{font['name']}.c" for font in FONTS]
FINAL_OUT = "../GUI/fonts.c"
FINAL_OUT_H = "../GUI/fonts.h"

def extract_codes(txt_file):
    with open(txt_file, "r", encoding="utf-8") as f:
        text = f.read()
    codes = {ord(ch) for ch in text if ord(ch) > 127}
    return sorted(codes)


def write_map_file(map_file, codes, ascii_range=None):
    with open(map_file, "w", encoding="utf-8") as f:
        if ascii_range and ascii_range.strip():
            f.write(f"{ascii_range},")
        code_list = [f"${code:04X}" for code in codes]
        f.write(",".join(code_list))

def run_bdfconv(map_file, output_name, bdf_file):
    cmd = [
        "./bin/bdfconv.exe",
        "-v", "-b", "0", "-f", "1",
        "-M", map_file,
        "-n", output_name,
        "-o", f"_{output_name}.c",
        bdf_file
    ]
    print(f"== Running: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def main():
    codes = extract_codes(FONT_TXT)

    for font in FONTS:
        map_file = f"_{font['name']}.map"
        write_map_file(map_file, codes, ascii_range=font['ascii'])
        run_bdfconv(
            map_file=map_file,
            output_name=font['name'],
            bdf_file=font['bdf']
        )
        print(f"== Generated {font['name']} -> _{font['name']}.c")
        print(f"== Removing temporary map file {map_file}")
        os.remove(map_file)

    print(f"== Merging {', '.join(OUT_FILES)} -> {FINAL_OUT}")
    with open(FINAL_OUT, "w", encoding="utf-8") as out_f:
        final_parts = ["#include \"fonts.h\"\n"]
        for fname in OUT_FILES:
            if not os.path.isfile(fname):
                print(f"Warning: {fname} not found, skipping", file=sys.stderr)
                continue
            with open(fname, "r", encoding="utf-8") as in_f:
                final_parts.append(in_f.read())
        for fname in EXTRA_FONTS:
            if not os.path.isfile(fname):
                print(f"Warning: {fname} not found, skipping", file=sys.stderr)
                continue
            with open(fname, "r", encoding="utf-8") as in_f:
                final_parts.append(in_f.read())
        out_f.write("\n".join(final_parts))

    print("== Done. fonts.c created.")

    print(f"== Creating {FINAL_OUT_H}...")
    with open(FINAL_OUT_H, "w", encoding="utf-8") as out_h:
        out_h.write("#ifndef __FONTS_H\n#define __FONTS_H\n\n")
        out_h.write("#include \"u8g2_font.h\"\n\n")
        for font in FONTS:
            out_h.write(f"extern const uint8_t {font['name']}[] U8G2_FONT_SECTION(\"{font['name']}\");\n")
        out_h.write("\n")
        for fname in EXTRA_FONTS:
            font_name = os.path.splitext(os.path.basename(fname))[0]
            out_h.write(f"extern const uint8_t {font_name}[] U8G2_FONT_SECTION(\"{font_name}\");\n")
        out_h.write("\n#endif\n")

    print("== Cleaning up temporary files...")
    for fname in OUT_FILES:
        if os.path.isfile(fname):
            os.remove(fname)
            print(f"Removed {fname}")
        else:
            print(f"Warning: {fname} not found, skipping", file=sys.stderr)


if __name__ == "__main__":
    main()