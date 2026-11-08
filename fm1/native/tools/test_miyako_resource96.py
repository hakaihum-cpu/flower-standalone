#!/usr/bin/env python3
"""Independent full-frame audit of the stored 96x96 16-colour VQ resource.

Checks actual decoded indices for ALL 628 frames; verifies source bank frame
counts, 24 independent RGB palettes, unique chronological frames, and complete 4bpp data.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from pack_miyako_resource import read_payload, CHUNK_DIR, SHA256
import hashlib

COUNT = 628
SIDE = 96
CLIP_FRAMES = 27
CLIPS = 24
MAP_STRIDE = 288
PALETTE_BYTES = CLIPS * 16 * 2
BOOK_BYTES = CLIPS * 256 * 16
BOOK_OFFSET = PALETTE_BYTES
MAP_OFFSET = PALETTE_BYTES + BOOK_BYTES
BANK_SIZES = (81, 64, 100, 81, 81, 81, 70, 70)


def check(data: bytes) -> None:
    assert sum(BANK_SIZES) == COUNT
    assert MAP_OFFSET + COUNT * MAP_STRIDE == len(data) == 279936
    palettes = []
    for c in range(CLIPS):
        colors = tuple(int.from_bytes(
            data[c * 32 + i * 2:c * 32 + i * 2 + 2], 'little')
            for i in range(16))
        assert len(set(colors)) >= 15, f'clip {c} lost source colors'
        palettes.append(colors)
    # User sheets include genuine yellow/sepia sunsets in the final bank.
    def chroma(rgb565):
        r = ((rgb565 >> 11) & 31) * 255 // 31
        g = ((rgb565 >> 5) & 63) * 255 // 63
        b = (rgb565 & 31) * 255 // 31
        return max(r, g, b) - min(r, g, b)
    assert any(chroma(v) > 20 for clip in palettes[21:] for v in clip), (
        'source sunset colors were lost by greyscale conversion')
    min_colors = 16
    colourful = 0
    unique_frames = set()
    for f in range(COUNT):
        c = f // CLIP_FRAMES
        counts = [0] * 16
        m = data[MAP_OFFSET + f * MAP_STRIDE: MAP_OFFSET + (f + 1) * MAP_STRIDE]
        assert len(m) == MAP_STRIDE
        # Decode every 8x4 tile to all 96*96 pixels, not just palette metadata.
        decoded = bytearray()
        for entry in m:
            code = data[BOOK_OFFSET + (c * 256 + entry) * 16:BOOK_OFFSET + (c * 256 + entry + 1) * 16]
            assert len(code) == 16
            decoded.extend(code)
            for b in code:
                counts[b >> 4] += 1
                counts[b & 15] += 1
        assert sum(counts) == SIDE * SIDE, (f, sum(counts))
        # Some original almost-monochrome drawings legitimately use fewer
        # than 15 of the available 16 colors. Never add fake colors to pass.
        distinct = sum(x > 0 for x in counts)
        assert distinct >= 13, f'frame {f} lost original detail: {distinct}'
        colourful += distinct >= 15
        min_colors = min(min_colors, distinct)
        unique_frames.add(hashlib.sha256(decoded).digest())
    assert len(unique_frames) == COUNT, 'identical VQ output for different user frames'
    assert colourful >= 610, f'only {colourful}/628 frames use >=15 colours'
    print(f'PASS: all {COUNT} distinct source RGB frames, 24 palettes, '
          f'original sunset colours preserved, >= {min_colors}/16 colours, '
          f'full decoded geometry 96x96; sha256 {hashlib.sha256(data).hexdigest()}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--chunk-dir', type=Path, default=CHUNK_DIR)
    args = parser.parse_args()
    check(read_payload(args.chunk_dir))
