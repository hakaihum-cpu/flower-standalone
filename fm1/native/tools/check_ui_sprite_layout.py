#!/usr/bin/env python3
"""Validate real FM-1 host captures: exactly one complete fixed ENV/LFO tile.

The original ENV sheet is 9 x 10 (90 frames) and LFO is 10 x 10
(100 frames). Both 48x48 drawings must remain within the bottom-centre
x=96..143, y=192..239 rectangle, outside the upper waveform and knob readouts.
"""
from __future__ import annotations
import sys
from pathlib import Path
import numpy as np
from PIL import Image

def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: check_ui_sprite_layout.py ENV.png LFO.png")
    for tag, filename in zip(("ENV", "LFO"), sys.argv[1:]):
        image = Image.open(Path(filename)).convert("RGB")
        if image.size != (240, 240):
            raise SystemExit(f"FAIL: {tag} display size {image.size}")
        rgb = np.asarray(image, dtype=np.uint8)
        bottom = np.any(rgb[192:240] > 12, axis=2)
        sprite = bottom[:, 96:144]
        outside = np.concatenate((bottom[:, :96], bottom[:, 144:]), axis=1)
        pixel_count = int(sprite.sum())
        stray = int(outside.sum())
        if pixel_count < 180:
            raise SystemExit(
                f"FAIL: {tag} bottom-centre girl missing ({pixel_count} pixels)")
        if stray:
            raise SystemExit(
                f"FAIL: {tag} sprite pixels outside fixed 48x48 panel ({stray})")
        print(f"PASS: {tag} one 48x48 girl tile, bottom x96..143 y192..239;"
              f" {pixel_count} visible pixels, zero outside panel")
if __name__ == "__main__":
    main()
