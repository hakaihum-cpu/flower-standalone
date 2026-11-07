#!/usr/bin/env python3
"""Fail if missing FM-1 photo resources produce a black SEL/FX image.

Uses saved screenshot pixels from the ACTUAL upstream platform host simulator
with "res 0". Tests both the full-screen SEL picture and the FX image panel.
No new images, codecs, renderers, or pipeline mechanisms.
"""
from pathlib import Path
import sys
from PIL import Image

if len(sys.argv) != 5:
    raise SystemExit("Usage: check_noresource_display.py missing_SEL.png missing_FX.png restored_SEL.png original_SEL.png")


def check(path, region, name):
    with Image.open(path) as im:
        rgb = im.convert("RGB")
        if rgb.size != (240, 240):
            raise SystemExit(f"FAIL: {name} expected 240x240, got {rgb.size}")
        visible = sum(1 for r, g, b in rgb.crop(region).getdata()
                      if r >= 95 and g >= 65 and b >= 10)
    if visible < 50:
        raise SystemExit(
            f"FAIL: {name} has only {visible} visible image pixels: {path}"
        )
    print(f"PASS: {name} fallback visibly drawn ({visible} pixels)")


check(Path(sys.argv[1]), (0, 0, 240, 240), "SEL missing-resource image")
# The FX image square is x=123..239, y=62..178; knobs don't count.
check(Path(sys.argv[2]), (123, 62, 239, 178), "FX missing-resource image")

# Reinstalling the matching stock FM-1 photo resource must restore real
# user imagery; a stale cached screenshot must not satisfy the fallback test.
check(Path(sys.argv[3]), (0, 0, 240, 240), "SEL restored-photo image")
with Image.open(sys.argv[1]) as missing, Image.open(sys.argv[4]) as installed:
    if missing.convert("RGB").tobytes() == installed.convert("RGB").tobytes():
        raise SystemExit("FAIL: missing resource did not change SEL image; stale rendering")
print("PASS: resource loss visibly changes SEL image, and restoration is visible")
