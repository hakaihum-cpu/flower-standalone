#!/usr/bin/env python3
"""16 real RGB565 colors extracted from the user's supplied warm pixel sample.

Quantizes source pixels only. No generated media, no geometric face edits.
The same palette is held across all 36 source frames in every clip.
"""
from __future__ import annotations
from PIL import Image

WIDTH, HEIGHT = 128, 78
CLIP_LENGTH = 36
REFERENCE_RGB = (
    (0, 0, 0),
    (30, 9, 10),
    (47, 23, 20),
    (85, 33, 12),
    (68, 49, 36),
    (76, 56, 42),
    (102, 66, 53),
    (123, 66, 40),
    (136, 63, 35),
    (139, 96, 87),
    (167, 89, 52),
    (209, 90, 48),
    (175, 117, 98),
    (236, 136, 79),
    (222, 169, 133),
    (255, 185, 137),
)
REFERENCE_565 = tuple(
    ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    for r, g, b in REFERENCE_RGB
)

def reference_pixels(frames: list[Image.Image]) -> tuple[list[int], list[bytes]]:
    """Apply the reference 16 colors to original pixels, preserving shape."""
    if len(frames) != CLIP_LENGTH:
        raise ValueError(f"expected {CLIP_LENGTH} original source frames")
    if len(set(REFERENCE_565)) != 16:
        raise ValueError("reference palette has fewer than 16 colors")
    pal = Image.new("P", (1, 1))
    pal.putpalette([c for rgb in REFERENCE_RGB for c in rgb])
    pixels = []
    for frame in frames:
        if frame.size != (WIDTH, HEIGHT):
            raise ValueError(f"incorrect original frame size: {frame.size}")
        indices = frame.convert("RGB").quantize(
            palette=pal, dither=Image.Dither.NONE).tobytes()
        if len(indices) != WIDTH * HEIGHT or max(indices) >= 16:
            raise ValueError("palette index outside 0..15")
        pixels.append(bytes((indices[i] << 4) | indices[i + 1]
                            for i in range(0, len(indices), 2)))
    return list(REFERENCE_565), pixels
