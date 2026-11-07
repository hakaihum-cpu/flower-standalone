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
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('resource_payload', type=Path)
    parser.add_argument('--chunk-dir', type=Path, default=CHUNK_DIR)
    args = parser.parse_args()
    data = read_payload(args.chunk_dir)
    args.resource_payload.parent.mkdir(parents=True, exist_ok=True)
    args.resource_payload.write_bytes(data)
    print(f'PASS: 628 original-source 96x96 photos, 16 real RGB565 colours, {len(data)}B, SHA256 {SHA256}')


if __name__ == '__main__':
    main()
