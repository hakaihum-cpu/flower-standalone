#!/usr/bin/env python3
"""MIYAKO original-source EFFECTS clip palette encoding.

Exactly 36 user-supplied frames per clip; fixed 16-colour RGB565 palette.
No EUREKA audio, animation or code dependency.
"""
from PIL import Image

WIDTH, HEIGHT = 128, 78
CLIP_LENGTH = 36
COLORS = 16

def clip_pixels(frames: list[Image.Image]) -> tuple[list[int], list[bytes]]:
    """Shared per-clip palette: colour, face midtones and stable brightness.

    Clip-wide luma mapping prevents frame-to-frame auto-contrast flicker.
    16-colour median-cut palette is FIXED throughout the 36-frame clip.
    No dithering: noisy pixel changes destroy source motion continuity.
    """
    if len(frames) != CLIP_LENGTH:
        raise ValueError(f"expected {CLIP_LENGTH} original frames per clip")
    hist = [0] * 256
    ycc = []
    for im in frames:
        y, cb, cr = im.convert("YCbCr").split()
        for value, count in enumerate(y.histogram()):
            hist[value] += count
        ycc.append((y, cb, cr))

    def percentile_cut(count: int, *, reverse: bool) -> int:
        total = 0
        indices = range(255, -1, -1) if reverse else range(256)
        for value in indices:
            total += hist[value]
            if total > count:
                return value
        return 0

    cutoff = sum(hist) // 100
    lo = percentile_cut(cutoff, reverse=False)
    hi = percentile_cut(cutoff, reverse=True)
    if hi <= lo:
        raise ValueError("source clip has no luminance range")
    lut = [max(0, min(255, round(255 * (
        max(0.0, min(1.0, (v - lo) / (hi - lo))) ** 0.75))))
        for v in range(256)]
    fixed = [Image.merge("YCbCr", (y.point(lut), cb, cr)).convert("RGB")
             for y, cb, cr in ycc]

    montage = Image.new("RGB", (WIDTH, HEIGHT * CLIP_LENGTH))
    for i, im in enumerate(fixed):
        montage.paste(im, (0, i * HEIGHT))
    colors = montage.quantize(colors=COLORS - 1,
                              method=Image.Quantize.MEDIANCUT,
                              dither=Image.Dither.NONE).getpalette()
    rgb = [(0, 0, 0)] + [tuple(colors[i * 3:i * 3 + 3])
                         for i in range(COLORS - 1)]
    if len(rgb) != COLORS:
        raise ValueError("incomplete RGB palette")
    pal = Image.new("P", (1, 1))
    pal.putpalette([c for color in rgb for c in color])
    palette565 = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                  for r, g, b in rgb]
    pixels = []
    for im in fixed:
        indices = im.quantize(palette=pal, dither=Image.Dither.NONE).tobytes()
        if len(indices) != WIDTH * HEIGHT or max(indices) >= COLORS:
            raise ValueError("Pillow produced out-of-range palette indices")
        pixels.append(bytes((indices[i] << 4) | indices[i + 1]
                            for i in range(0, len(indices), 2)))
    return palette565, pixels
