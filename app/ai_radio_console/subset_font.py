#!/usr/bin/env python3
"""
Subset NotoSansSC font for DayNote embedded use.

Reduces 10MB full font to ~1-2MB by keeping only:
- ASCII printable characters
- GB2312 common Chinese characters (~6763)
- Common punctuation and symbols

Usage:
    python3 subset_font.py [input.ttf] [output.ttf]

Requires: pip install fonttools brotli
"""

import sys
import os

# GB2312 character ranges (common Chinese)
GB2312_RANGES = [
    (0x4E00, 0x9FFF),    # CJK Unified Ideographs
    (0x3000, 0x303F),    # CJK Symbols and Punctuation
    (0xFF00, 0xFFEF),    # Halfwidth and Fullwidth Forms
    (0x2000, 0x206F),    # General Punctuation
    (0x3400, 0x4DBF),    # CJK Unified Ideographs Extension A
]

def main():
    input_font = sys.argv[1] if len(sys.argv) > 1 else None
    output_font = sys.argv[2] if len(sys.argv) > 2 else "font.ttf"

    if not input_font:
        # Try default locations
        candidates = [
            "/home/bi4mib/openvela-build/apps/graphics/lvgl/lvgl/demos/benchmark/assets/NotoSansSC-Regular.ttf",
            os.path.join(os.path.dirname(__file__), "NotoSansSC-Regular.ttf"),
        ]
        for c in candidates:
            if os.path.exists(c):
                input_font = c
                break

    if not input_font or not os.path.exists(input_font):
        print(f"Error: Input font not found. Usage: {sys.argv[0]} <input.ttf> [output.ttf]")
        sys.exit(1)

    try:
        from fontTools.subset import Subsetter, Options
        from fontTools.ttLib import TTFont
    except ImportError:
        print("Error: fonttools not installed. Run: pip install fonttools brotli")
        sys.exit(1)

    print(f"Loading {input_font}...")
    font = TTFont(input_font)

    # Build character set
    chars = set()

    # ASCII printable
    for c in range(0x20, 0x7F):
        chars.add(c)

    # GB2312 ranges
    for start, end in GB2312_RANGES:
        for c in range(start, end + 1):
            chars.add(c)

    # Additional common characters
    extra = "·—…""''《》【】（）￥、。，！？：；"
    for c in extra:
        chars.add(ord(c))

    print(f"Subsetting to {len(chars)} characters...")

    options = Options()
    options.layout_features = []
    options.name_IDs = []
    options.glyph_names = False
    options.legacy_kern = False
    options.notdef_glyph = True
    options.ignore_missing_glyphs = True
    options.drop_tables = ['DSIG', 'GSUB', 'GPOS', 'GDEF']

    subsetter = Subsetter(options=options)
    subsetter.populate(unicodes=chars)
    subsetter.subset(font)

    print(f"Saving to {output_font}...")
    font.save(output_font)

    orig_size = os.path.getsize(input_font)
    new_size = os.path.getsize(output_font)
    print(f"Done: {orig_size/1024/1024:.1f}MB -> {new_size/1024/1024:.1f}MB")
    print(f"Copy {output_font} to /data/daynote/font.ttf on the device.")

if __name__ == "__main__":
    main()
