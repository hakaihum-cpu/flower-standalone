#!/usr/bin/env python3
from pathlib import Path
import base64
import hashlib

ROOT = Path(__file__).resolve().parents[1]
CHUNK_DIR = ROOT / 'Resources' / 'frame_chunks'
OUT = ROOT / 'Resources' / 'classroom_frames.pack'
ICON_OUT = ROOT / 'Resources' / 'effects_app_icon.jpg'
EXPECTED_SHA256 = '1e888dbed69e259c52d2cb2bd192faa5c7f29dcf76eafcda9fdb2a2d03f2d658'
EXPECTED_SIZE = 4464088

parts = sorted(CHUNK_DIR.glob('chunk_*.b64'))
if len(parts) != 13:
    raise SystemExit(f'[FAIL] expected 13 frame chunks, got {len(parts)}')

data = bytearray()
for path in parts:
    encoded = ''.join(path.read_text(encoding='ascii').split())
    data.extend(base64.b64decode(encoded, validate=True))

if len(data) != EXPECTED_SIZE:
    raise SystemExit(f'[FAIL] reconstructed frame pack size {len(data)} != {EXPECTED_SIZE}')
sha = hashlib.sha256(data).hexdigest()
if sha != EXPECTED_SHA256:
    raise SystemExit(f'[FAIL] reconstructed frame pack sha256 {sha} != {EXPECTED_SHA256}')
OUT.write_bytes(data)

# The selected launcher artwork is supplied as frame 01 in the same exact
# classroom corpus. CRF1 layout: magic + u32 count + <u64 offset,u32 size> table.
if data[:4] != b'CRF1':
    raise SystemExit('[FAIL] classroom frame pack magic mismatch')
count = int.from_bytes(data[4:8], 'little')
if count < 1:
    raise SystemExit('[FAIL] classroom frame pack has no frames')

entry = 8
offset = int.from_bytes(data[entry:entry + 8], 'little')
size = int.from_bytes(data[entry + 8:entry + 12], 'little')
if offset + size > len(data) or size < 4:
    raise SystemExit('[FAIL] classroom frame 01 bounds invalid')

icon = bytes(data[offset:offset + size])
if not icon.startswith(b'\xff\xd8'):
    raise SystemExit('[FAIL] classroom frame 01 is not JPEG')
ICON_OUT.write_bytes(icon)

print(f'[PASS] materialized exact frame pack: {len(data)} bytes sha256={sha}')
print(f'[PASS] materialized launcher icon from supplied frame 01: {len(icon)} bytes')