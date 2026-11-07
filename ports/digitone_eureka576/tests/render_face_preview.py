#!/usr/bin/env python3
"""Render actual 128x64 RG DOT LCD samples from the exact 1-bit reference.bin."""
from pathlib import Path
from PIL import Image, ImageDraw
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: render_face_preview.py reference.bin sample.png")
frames = Path(sys.argv[1]).read_bytes()
assert len(frames) == 1152 * 1024
selected = [0, 143, 288, 575, 576, 719, 864, 1151]
scale = 4
margin = 12
header = 26
tile_w, tile_h = 128 * scale, 64 * scale
sheet = Image.new("RGB", (4 * (tile_w + margin) + margin,
                         2 * (tile_h + header + margin) + margin), (10, 10, 10))
draw = ImageDraw.Draw(sheet)
for pos, index in enumerate(selected):
    frame = frames[index * 1024:(index + 1) * 1024]
    lcd = Image.new("RGB", (128, 64), (0, 0, 0))
    pix = lcd.load()
    for x in range(128):
        for y in range(64):
            if frame[7 - (y >> 3) + 8 * x] & (1 << (y & 7)):
                pix[x, y] = (245, 187, 17)
    x0 = margin + (pos % 4) * (tile_w + margin)
    y0 = margin + (pos // 4) * (tile_h + header + margin)
    draw.text((x0, y0 + 4), f"EUREKA source frame #{index:03d}", fill=(208, 208, 208))
    sheet.paste(lcd.resize((tile_w, tile_h), Image.Resampling.NEAREST), (x0, y0 + header))
out = Path(sys.argv[2])
out.parent.mkdir(parents=True, exist_ok=True)
sheet.save(out)
print(f"PASS: true 1-bit LCD preview, 8 frames; saved {out}; {sheet.size}")
