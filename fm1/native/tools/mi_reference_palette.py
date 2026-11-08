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
    # MEDIANCUT is the historical baseline. Evaluate two other built-in
    # deterministic 16-colour palette quantizers on the same 36 original
    # images; keep the baseline unless worst-frame and mean RGB565 colour
    # fidelity objectively improve. No image synthesis or extra ROM.
    import numpy as np
    original = np.stack([np.asarray(im.convert("RGB"), dtype=np.int16)
                         for im in frames])
    # Weight color errors at genuine source edges, where facial lines and
    # small details are disproportionately harmed by palette collapse.
    gray = ((77 * original[..., 0] + 150 * original[..., 1] +
             29 * original[..., 2]) >> 8).astype(np.int16)
    edge = np.zeros_like(gray, dtype=np.float32)
    edge[:, :, 1:] += np.abs(gray[:, :, 1:] - gray[:, :, :-1])
    edge[:, 1:, :] += np.abs(gray[:, 1:, :] - gray[:, :-1, :])
    weight = 1.0 + np.minimum(edge, 96.0) / 192.0

    def palette_candidate(method):
        quantized = collage.quantize(
            colors=16, method=method, dither=Image.Dither.NONE)
        raw = quantized.getpalette()
        entries = quantized.getcolors() or []
        count = max(1, min(16, max((index for _, index in entries),
                                    default=0) + 1))
        colors = [tuple(raw[i * 3:i * 3 + 3]) for i in range(count)]
        while len(colors) < 16:
            colors.append(colors[-1])
        rgb565 = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                  for r, g, b in colors]
        palette_image = Image.new("P", (1, 1))
        palette_image.putpalette([c for rgb in colors for c in rgb])
        indices = np.stack([
            np.asarray(im.convert("RGB").quantize(
                palette=palette_image, dither=Image.Dither.NONE),
                dtype=np.uint8) for im in frames
        ])
        if np.any(indices >= 16):
            raise ValueError("source palette index exceeds 16 colours")
        recon565 = np.asarray(rgb565, dtype=np.uint16)[indices]
        expanded = np.stack([
            (recon565 >> 11) * 255 // 31,
            ((recon565 >> 5) & 63) * 255 // 63,
            (recon565 & 31) * 255 // 31,
        ], axis=-1).astype(np.int16)
        delta = original.astype(np.int32) - expanded.astype(np.int32)
        error = np.sum(delta * delta, axis=3).astype(np.float32)
        frame_loss = (error * weight).mean(axis=(1, 2))
        # Prioritise the worst individual source frame, then average.
        quality = (float(frame_loss.max()), float(frame_loss.mean()))
        packed = [(indices[f, :, 0::2] << 4 |
                   indices[f, :, 1::2]).tobytes()
                  for f in range(CLIP_LENGTH)]
        return quality, rgb565, packed

    best = palette_candidate(Image.Quantize.MEDIANCUT)
    for method in (Image.Quantize.MAXCOVERAGE,
                   Image.Quantize.FASTOCTREE):
        candidate = palette_candidate(method)
        if candidate[0] < best[0]:
            best = candidate
    _, rgb565, out = best
    if (len(rgb565) != 16 or len(out) != CLIP_LENGTH or
            any(len(b) != w * h // 2 for b in out)):
        raise ValueError("16-colour palette candidate corrupts frame count")
    return rgb565, out
