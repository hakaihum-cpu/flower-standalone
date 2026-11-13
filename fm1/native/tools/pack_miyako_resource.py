#!/usr/bin/env python3
"""Pack approved original-source MHP2 SEL images for the existing FM-1 resource installer.

The FM-1 file contains a CRC32-protected MHP2 core and a per-frame tile
correction directory. It is NOT an installable resource by itself: the
existing upstream builder adds the 32-byte res.bin header/CRC.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CHUNK_DIR = ROOT / 'Resources' / 'fm1_mhp2_96_chunks'
PAYLOAD_BYTES = 282071
SHA256 = '35ab028b0bb48659ecab47c36afe2a4c6873e6b91f29c0b46313606f41cb5a3f'
RESOURCE_CAPACITY = 69 * 4096 - 32
FILES = 12
PALETTE_CLIPS = 24
PALETTE_ENTRIES = 16
MHP2_CORE_BYTES = 209067
MHP2_OVERLAY_BYTES = PAYLOAD_BYTES - MHP2_CORE_BYTES
FRAMES = 628
FRAME_OFFSETS_BYTES = (FRAMES + 1) * 4


def read_payload(directory: Path) -> bytes:
    files = sorted(directory.glob('part_????.b64'))
    if len(files) != FILES or [p.name for p in files] != [f'part_{i:04}.b64' for i in range(1, FILES + 1)]:
        raise ValueError('MHP2 resource requires all 12 numbered image chunks')
    encoded = ''.join(''.join(p.read_text(encoding='ascii').split()) for p in files)
    data = base64.b64decode(encoded, validate=True)
    if len(data) != PAYLOAD_BYTES or len(data) > RESOURCE_CAPACITY:
        raise ValueError(f'MHP2 resource size invalid: {len(data)}')
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError('MHP2 resource SHA-256 mismatch')
    if data[:4] != b'MHP2' or struct.unpack_from('<I', data, 8)[0] != MHP2_CORE_BYTES:
        raise ValueError('MHP2 magic/size mismatch')
    if zlib.crc32(data[64:MHP2_CORE_BYTES]) != struct.unpack_from('<I', data, 12)[0]:
        raise ValueError('MHP2 core CRC mismatch')
    if struct.unpack_from('<HHHH', data, 16) != (PALETTE_CLIPS, FRAMES, 96, 96):
        raise ValueError('MHP2 clip/frame/geometry mismatch')
    if len(data[MHP2_CORE_BYTES:]) != MHP2_OVERLAY_BYTES:
        raise ValueError('MHP2 overlay length mismatch')
    overlay = data[MHP2_CORE_BYTES:]
    directory_entries = struct.unpack_from('<629I', overlay)
    if directory_entries[0] or directory_entries[-1] != MHP2_OVERLAY_BYTES - FRAME_OFFSETS_BYTES:
        raise ValueError('MHP2 overlay directory invalid')
    patches = 0
    for i in range(FRAMES):
        a, b = directory_entries[i:i+2]
        if a > b or (b - a) % 18 or b > directory_entries[-1]:
            raise ValueError(f'MHP2 tile directory bounds invalid frame {i}')
        patches += (b - a) // 18
        for off in range(a, b, 18):
            tile = struct.unpack_from('<H', overlay, FRAME_OFFSETS_BYTES + off)[0]
            if tile >= 288:
                raise ValueError(f'MHP2 tile ID invalid frame {i}')
            for v in overlay[FRAME_OFFSETS_BYTES + off + 2:FRAME_OFFSETS_BYTES + off + 18]:
                if (v >> 4) > 15 or (v & 15) > 15:
                    raise ValueError('MHP2 palette index overflow')
    if patches != 3916:
        raise ValueError(f'MHP2 correction count mismatch: {patches}')
    palettes = []
    for clip in range(PALETTE_CLIPS):
        pal = struct.unpack_from('<16H', data, 64 + clip*32)
        if len(set(pal)) < 15:
            raise ValueError(f'MHP2 palette too small clip {clip}')
        palettes.append(pal)
    print(f'PASS MHP2 resource: {len(data)}B, {FRAMES} frames, {patches} source patches, 16 RGB565 colours/clip')
    return data


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('resource_payload', type=Path)
    parser.add_argument('--chunk-dir', type=Path, default=CHUNK_DIR)
    args = parser.parse_args()
    data = read_payload(args.chunk_dir)
    args.resource_payload.parent.mkdir(parents=True, exist_ok=True)
    args.resource_payload.write_bytes(data)
    print('PASS resource SHA256 ' + hashlib.sha256(data).hexdigest())
    print('MHP2 is build-time encoded. The FM-1 firmware contains only the freestanding decoder.')


if __name__ == '__main__':
    main()