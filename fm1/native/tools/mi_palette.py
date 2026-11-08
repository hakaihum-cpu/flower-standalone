#!/usr/bin/env python3
"""MIYAKO original-source EFFECTS clip palette encoding.

Exactly 36 user-supplied frames per clip; fixed 16-colour RGB565 palette.
No EUREKA audio, animation or code dependency.
"""
from PIL import Image
from mi_reference_palette import natural_pixels

WIDTH, HEIGHT = 128, 78
CLIP_LENGTH = 36
COLORS = 16

def clip_pixels(frames: list[Image.Image]) -> tuple[list[int], list[bytes]]:
    """Quantise the ACTUAL supplied JPEG frames to 16 RGB565 colours.

    Do NOT apply the old 1%-99% luma remap, gamma 0.86, UnsharpMask or
    yellow/blue rebalancing. Those changes were already baked into the
    128x78 intermediary before lossy VQ; on verified source MP4 frames they
    decreased full-source fidelity by about 5 dB. The clip's 36 frames
    share one palette, with no random dithering or temporal flicker.
    """
    if len(frames) != CLIP_LENGTH:
        raise ValueError(f"expected {CLIP_LENGTH} original frames per clip")
    return natural_pixels(frames)