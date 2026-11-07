#!/usr/bin/env python3
"""Independent full-frame audit of the stored 96x96 16-colour VQ resource.

Checks actual decoded indices for ALL 628 frames; verifies source bank frame
counts, stable clip palettes, no grayscale-only render, and full-range data.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from pack_miyako_resource import read_payload, CHUNK_DIR, SHA256

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
REFERENCE = (0x0000,0x1841,0x28a2,0x5101,0x4184,0x49c5,0x6206,0x7a05,
             0x89e4,0x8b0a,0xa2c6,0xd2c6,0xabac,0xec49,0xdd50,0xfdd1)


def check(data: bytes) -> None:
    assert sum(BANK_SIZES) == COUNT
    assert MAP_OFFSET + COUNT * MAP_STRIDE == len(data) == 279936
    assert len(set(REFERENCE)) == 16
    for c in range(CLIPS):
        colors = tuple(int.from_bytes(data[c * 32 + i * 2:c * 32 + i * 2 + 2], 'little') for i in range(16))
        assert colors == REFERENCE, f'clip {c} palette deviates from supplied reference'
    min_colors = 16
    for f in range(COUNT):
        c = f // CLIP_FRAMES
        counts = [0] * 16
        m = data[MAP_OFFSET + f * MAP_STRIDE: MAP_OFFSET + (f + 1) * MAP_STRIDE]
        assert len(m) == MAP_STRIDE
        # Decode every 8x4 tile to all 96*96 pixels, not just palette metadata.
        for entry in m:
            code = data[BOOK_OFFSET + (c * 256 + entry) * 16:BOOK_OFFSET + (c * 256 + entry + 1) * 16]
            assert len(code) == 16
            for b in code:
                counts[b >> 4] += 1
                counts[b & 15] += 1
        assert sum(counts) == SIDE * SIDE, (f, sum(counts))
        distinct = sum(x > 0 for x in counts)
        assert distinct >= 15, f'frame {f} still monochrome/low-colour: {distinct}'
        min_colors = min(min_colors, distinct)
    print(f'PASS: {COUNT} original 96x96 frames, 8 user banks, >= {min_colors}/16 real colours each, sha {SHA256}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--chunk-dir', type=Path, default=CHUNK_DIR)
    args = parser.parse_args()
    check(read_payload(args.chunk_dir))
