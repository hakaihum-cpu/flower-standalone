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

def natural_pixels(frames: list[Image.Image]) -> tuple[list[int], list[bytes]]:
    """Derive a 16-colour RGB565 palette from all 36 actual source frames.

    Never apply the former single amber/sepia reference palette to every
    EFFECTS/EUREKA clip. Preserve source chroma without generated artwork.
    """
    if len(frames) != CLIP_LENGTH:
        raise ValueError(f"expected {CLIP_LENGTH} original source frames")
    w, h = frames[0].size
    if any(im.size != (w, h) for im in frames):
        raise ValueError("mixed source frame dimensions")
    collage = Image.new("RGB", (w * 6, h * 6))
    for i, frame in enumerate(frames):
        collage.paste(frame.convert("RGB"), ((i % 6) * w, (i // 6) * h))
    paletted = collage.quantize(
        colors=16, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE
    )
    raw = paletted.getpalette()
    entries = paletted.getcolors() or []
    count = max(1, min(16, max((index for _, index in entries), default=0) + 1))
    colors = [tuple(raw[i * 3:i * 3 + 3]) for i in range(count)]
    # Fill unused slots only. Every actual pixel is quantized to the original
    # source-derived entries, not to warm reference colors.
    while len(colors) < 16:
        colors.append(colors[-1])
    rgb565 = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
              for r, g, b in colors]
    palette_image = Image.new("P", (1, 1))
    palette_image.putpalette([c for rgb in colors for c in rgb])
    out = []
    for frame in frames:
        indices = frame.convert("RGB").quantize(
            palette=palette_image, dither=Image.Dither.NONE
        ).tobytes()
        if len(indices) != w * h or any(x >= 16 for x in indices):
            raise ValueError("out-of-range image palette index")
        out.append(bytes((indices[i] << 4) | indices[i + 1]
                         for i in range(0, len(indices), 2)))
    return rgb565, out
