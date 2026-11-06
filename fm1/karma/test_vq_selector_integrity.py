#!/usr/bin/env python3
"""Host-only regression: final 8x4 VQ selectors reference the FINAL 256-tile dictionary.

Synthetic integer-index input is a regression fixture, not generated video
and never becomes a user-visible EUREKA source frame or firmware asset.
"""
import numpy as np

from pack_eureka_vq_candidate import (
    DICT_SIZE, FRAMES, PAD_H, TILE_COUNT, TILE_PIXELS, WIDTH,
    rgb_palette, tile_expand, tile_extract, train_clip,
)

rng = np.random.default_rng(20261106)
indices = rng.integers(0, 16, size=(FRAMES, PAD_H, WIDTH), dtype=np.uint8)
palette565 = np.array([
    0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x7BEF, 0xFFE0, 0xF81F,
    0x07FF, 0x4208, 0x8410, 0xC618, 0xFBE0, 0x03EF, 0x780F, 0xA514,
], dtype=np.uint16)

def verify_dictionary_integrity(book, selectors, restored, quality):
    assert book.shape == (DICT_SIZE, TILE_PIXELS)
    assert selectors.shape == (FRAMES, TILE_COUNT)
    assert book.max() < 16
    assert np.array_equal(restored, tile_expand(book, selectors))

    palette = rgb_palette(palette565)
    source_tiles = tile_extract(indices)
    source_rgb = palette[source_tiles].reshape(-1, TILE_PIXELS * 3).astype(np.float32)
    dict_rgb = palette[book].reshape(DICT_SIZE, TILE_PIXELS * 3).astype(np.float32)
    source_norms = np.einsum("ij,ij->i", source_rgb, source_rgb)
    dict_norms = np.einsum("ij,ij->i", dict_rgb, dict_rgb)
    distances = (source_norms[:, None] + dict_norms[None, :]
                 - 2 * (source_rgb @ dict_rgb.T))
    expected_selectors = np.argmin(distances, axis=1).astype(np.uint8)
    assert np.array_equal(selectors.ravel(), expected_selectors), (
        "VQ selectors refer to a stale dictionary, not the final firmware dictionary"
    )
    assert quality[0] <= quality[1]
    assert quality[2] == FRAMES and quality[3] == FRAMES

baseline = train_clip(indices, palette565)
verify_dictionary_integrity(*baseline)
for alpha in (1.0, 2.0, 3.0, 4.0, 6.0):
    balanced = train_clip(indices, palette565, loops=8, frame_balance=alpha)
    verify_dictionary_integrity(*balanced)
    assert np.isfinite(balanced[3][0]) and np.isfinite(balanced[3][1]), (
        "frame-balanced VQ numeric overflow at weighting power " + str(alpha))
    chosen = max((baseline, balanced), key=lambda result: (result[3][0], result[3][1]))
    assert chosen[3][0] >= baseline[3][0]
# Deterministic restart must reproduce the *same* dictionary, selectors
# and worst-frame PSNR, and every restart uses the 8x4 / 256 codec.
for seed in (2, 5, -11, -17):
    first = train_clip(indices, palette565, loops=8,
                       frame_balance=6.0, seed=seed)
    second = train_clip(indices, palette565, loops=8,
                        frame_balance=6.0, seed=seed)
    verify_dictionary_integrity(*first)
    verify_dictionary_integrity(*second)
    assert np.array_equal(first[0], second[0])
    assert np.array_equal(first[1], second[1])
    assert first[3] == second[3]
print("PASS: 6 baseline VQ paths and 4 deterministic multi-start paths; codec unchanged")
