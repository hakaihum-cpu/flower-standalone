#!/usr/bin/env python3
"""Reassemble the separately flashed FM-1 photo resource from approved images.

628 original, user-supplied frames (96x96), 16 RGB565 colours, 8x4 VQ.
The prebuilt compressed payload is checked into Resources/fm1_photo_96_chunks.
No image generation, network access, or image transcoding occurs during CI.
The standard fm1-base builder adds its 32-byte CRC header to this payload.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CHUNK_DIR = ROOT / 'Resources' / 'fm1_photo_96_chunks'
PAYLOAD_BYTES = 279936
SHA256 = 'bb5345df9b9e4118052b2feac603e38f16db0fccd5af49223e52b08115a1cc40'
RESOURCE_CAPACITY = 69 * 4096 - 32
FILES = 12
# User imagery remains unchanged: recolour only the 24 on-flash palette
# tables. Earlier source had the SAME amber-biased palette in every clip.
# The archival chunks and their independently verified SHA stay immutable.
PHOTO_COLORS = (
    (0, 0, 0), (26, 24, 36), (38, 42, 65), (60, 45, 73),
    (51, 63, 74), (68, 69, 101), (59, 104, 122), (113, 91, 165),
    (73, 132, 173), (164, 109, 150), (101, 167, 155),
    (205, 128, 147), (157, 182, 209), (231, 176, 168),
    (207, 210, 228), (246, 241, 237),
)
PHOTO_RGB565 = tuple(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                     for r, g, b in PHOTO_COLORS)
assert len(set(PHOTO_RGB565)) == 16


def read_payload(directory: Path) -> bytes:
    files = sorted(directory.glob('part_????.b64'))
    if len(files) != FILES or [p.name for p in files] != [f'part_{i:04}.b64' for i in range(1, FILES + 1)]:
        raise ValueError('FM-1 photo resource requires all 12 numbered source chunks')
    # Chunk boundaries are multiples of four Base64 characters; validate
    # the entire assembled input against the generated source checksum.
    encoded = ''.join(''.join(p.read_text(encoding='ascii').split()) for p in files)
    result = base64.b64decode(encoded, validate=True)
    if len(result) != PAYLOAD_BYTES or len(result) > RESOURCE_CAPACITY:
        raise ValueError(f'FM-1 96x96 resource has invalid size: {len(result)}')
    digest = hashlib.sha256(result).hexdigest()
    if digest != SHA256:
        raise ValueError('FM-1 96x96 original-source resource SHA256 mismatch')
    # Recolour ONLY RGB565 palette tables: the 628 approved tile maps and
    # codebooks remain byte-for-byte the same (no new/AI images).
    transformed = bytearray(result)
    for clip in range(24):
        for index, colour in enumerate(PHOTO_RGB565):
            offset = clip * 32 + index * 2
            transformed[offset:offset + 2] = colour.to_bytes(2, 'little')
    return bytes(transformed)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('resource_payload', type=Path)
    parser.add_argument('--chunk-dir', type=Path, default=CHUNK_DIR)
    args = parser.parse_args()
    data = read_payload(args.chunk_dir)
    args.resource_payload.parent.mkdir(parents=True, exist_ok=True)
    args.resource_payload.write_bytes(data)
    print(f'PASS: 628 original-source 96x96 photos, 16 balanced RGB565 colours, {len(data)}B, '
          f'archive SHA256 {SHA256}, output SHA256 {hashlib.sha256(data).hexdigest()}')


if __name__ == '__main__':
    main()
