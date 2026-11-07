"""Detect and replace visually black VQ frames with the nearest SAME-clip frame.

The 16-colour RGB565 palettes and 256-tile dictionaries remain untouched.
"""
from __future__ import annotations
import numpy as np

FRAME_COUNT = 36
WIDTH, HEIGHT, PADDED_HEIGHT = 128, 78, 80
TILES = 320
# Less than 32 visible display pixels out of 9,984 is effectively black.
MIN_VISIBLE_PIXELS = 32


def visible_pixel_counts(frames, palette565):
    if frames.shape[1:] != (PADDED_HEIGHT, WIDTH):
        raise ValueError(f"bad frame geometry {frames.shape}")
    pal = np.asarray(palette565, dtype=np.uint16)
    if pal.shape != (16,):
        raise ValueError("expected one 16-colour RGB565 palette")
    visible = ((pal >> 11) >= 4) | (((pal >> 5) & 63) >= 7) | ((pal & 31) >= 4)
    visible[0] = False  # The FM-1 renderer skips palette-index 0.
    return np.count_nonzero(visible[frames[:, :HEIGHT, :]], axis=(1, 2))


def replace_black_frames(source, decoded, selectors, palette565, label, clip):
    if source.shape != (FRAME_COUNT, PADDED_HEIGHT, WIDTH):
        raise ValueError(f"{label} clip {clip}: invalid input frames")
    if decoded.shape != source.shape or selectors.shape != (FRAME_COUNT, TILES):
        raise ValueError(f"{label} clip {clip}: invalid VQ map dimensions")
    src_count = visible_pixel_counts(source, palette565)
    dec_count = visible_pixel_counts(decoded, palette565)
    bad = (src_count < MIN_VISIBLE_PIXELS) | (dec_count < MIN_VISIBLE_PIXELS)
    good = np.flatnonzero(~bad)
    if not len(good):
        raise ValueError(f"{label} clip {clip}: all frames are black; no same-clip replacement")
    replacements = []
    for frame in np.flatnonzero(bad):
        i = int(frame)
        donor = min((int(x) for x in good), key=lambda x: (abs(x-i), x))
        selectors[i] = selectors[donor].copy()
        decoded[i] = decoded[donor].copy()
        replacements.append((clip*FRAME_COUNT+i, clip*FRAME_COUNT+donor))
    if np.any(visible_pixel_counts(decoded, palette565) < MIN_VISIBLE_PIXELS):
        raise ValueError(f"{label} clip {clip}: black output remains")
    for i, donor in replacements:
        print(f"{label}: black frame {i} replaced by frame {donor} (same clip)", flush=True)
    return replacements
