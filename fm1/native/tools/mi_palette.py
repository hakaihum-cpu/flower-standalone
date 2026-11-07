#!/usr/bin/env python3
"""MIYAKO original-source EFFECTS clip palette encoding.

Exactly 36 user-supplied frames per clip; fixed 16-colour RGB565 palette.
No EUREKA audio, animation or code dependency.
"""
from PIL import Image, ImageStat, ImageFilter
from mi_reference_palette import reference_pixels

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
        max(0.0, min(1.0, (v - lo) / (hi - lo))) ** 0.86))))
        for v in range(256)]
    fixed = [Image.merge("YCbCr", (y.point(lut), cb, cr)).convert("RGB")
             for y, cb, cr in ycc]
    # Preserve original pixel structure, geometry and natural hair/skin.
    # Modest edge clarity only, before 16-color quantisation; no AI image
    # creation, face reshape, skin smoothing or per-frame new palette.
    fixed = [im.filter(ImageFilter.UnsharpMask(
        radius=0.65, percent=30, threshold=4)) for im in fixed]
    # Same color adjustment for all 36 frames prevents temporal flicker.
    # Strong yellow clips get a SMALL reduction in cast, not a forced gray
    # or orange replacement. Existing RGB channels stay otherwise intact.
    means = [ImageStat.Stat(im).mean for im in fixed]
    red = sum(m[0] for m in means) / len(means)
    green = sum(m[1] for m in means) / len(means)
    blue = sum(m[2] for m in means) / len(means)
    if (blue > 12.0 and green > 35.0 and
            red > blue * 1.30 and green > blue * 1.20):
        gain = min(1.10, (red + green) / (2.0 * blue))
        blue_lut = [min(255, round(i * gain)) for i in range(256)]
        fixed = [Image.merge("RGB", (
            im.getchannel("R"), im.getchannel("G"),
            im.getchannel("B").point(blue_lut))) for im in fixed]

    # The reference user's 16 warm colors replace grayscale-per-clip medcut.
    # Face outline and frame sequence remain the real source pixels only.
    return reference_pixels(fixed)