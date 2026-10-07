#!/usr/bin/env python3
from pathlib import Path
import base64
import hashlib

ROOT = Path(__file__).resolve().parents[1]

# Existing 300-frame classroom bank.
CLASSROOM_CHUNK_DIR = ROOT / 'Resources' / 'frame_chunks'
CLASSROOM_OUT = ROOT / 'Resources' / 'classroom_frames.pack'
CLASSROOM_SHA256 = '1e888dbed69e259c52d2cb2bd192faa5c7f29dcf76eafcda9fdb2a2d03f2d658'
CLASSROOM_SIZE = 4464088

# User-supplied EUREKA videos: four 8-second MP4 sources sampled to 36 JPEG frames each (144 total).
EUREKA_CHUNK_DIR = ROOT / 'Resources' / 'eureka_frame_chunks'
EUREKA_OUT = ROOT / 'Resources' / 'eureka_frames.pack'
EUREKA_SHA256 = '827b5aa6a339927cb909a5ffe1b129513d946911cc78ea96d34aa110e6cd1883'
EUREKA_SIZE = 1872671
EUREKA_COUNT = 144

# User-selected launcher artwork.
ICON_B64 = ROOT / 'Resources' / 'effects_app_icon.b64'
ICON_OUT = ROOT / 'Resources' / 'effects_app_icon.jpg'
ICON_SHA256 = 'f4d9d841e5f4789588c53c8babcc849663911583907b259436e683c8268ee703'
ICON_SIZE = 27540


def decode_chunks(paths):
    decoded = bytearray()
    for path in paths:
        encoded = ''.join(path.read_text(encoding='ascii').split())
        decoded.extend(base64.b64decode(encoded, validate=True))
    return bytes(decoded)


def validate_crf1(data, expected_count, label):
    if data[:4] != b'CRF1':
        raise SystemExit(f'[FAIL] {label} magic mismatch')
    count = int.from_bytes(data[4:8], 'little')
    if count != expected_count:
        raise SystemExit(
            f'[FAIL] {label} frame count {count} != {expected_count}')


classroom_parts = sorted(CLASSROOM_CHUNK_DIR.glob('chunk_*.b64'))
if len(classroom_parts) != 13:
    raise SystemExit(
        f'[FAIL] expected 13 classroom frame chunks, got {len(classroom_parts)}')
classroom = decode_chunks(classroom_parts)
if len(classroom) != CLASSROOM_SIZE:
    raise SystemExit(
        f'[FAIL] reconstructed classroom pack size {len(classroom)} != {CLASSROOM_SIZE}')
classroom_sha = hashlib.sha256(classroom).hexdigest()
if classroom_sha != CLASSROOM_SHA256:
    raise SystemExit(
        f'[FAIL] reconstructed classroom pack sha256 {classroom_sha} != {CLASSROOM_SHA256}')
validate_crf1(classroom, 300, 'classroom frame pack')
CLASSROOM_OUT.write_bytes(classroom)

eureka_parts = sorted(EUREKA_CHUNK_DIR.glob('*.b64'))
if len(eureka_parts) != 21:
    raise SystemExit(
        f'[FAIL] expected 21 EUREKA frame chunks, got {len(eureka_parts)}')
eureka = decode_chunks(eureka_parts)
if len(eureka) != EUREKA_SIZE:
    raise SystemExit(
        f'[FAIL] reconstructed EUREKA pack size {len(eureka)} != {EUREKA_SIZE}')
eureka_sha = hashlib.sha256(eureka).hexdigest()
if eureka_sha != EUREKA_SHA256:
    raise SystemExit(
        f'[FAIL] reconstructed EUREKA pack sha256 {eureka_sha} != {EUREKA_SHA256}')
validate_crf1(eureka, EUREKA_COUNT, 'EUREKA frame pack')
EUREKA_OUT.write_bytes(eureka)

icon_encoded = ''.join(ICON_B64.read_text(encoding='ascii').split())
icon = base64.b64decode(icon_encoded, validate=True)
if len(icon) != ICON_SIZE:
    raise SystemExit(
        f'[FAIL] launcher icon size {len(icon)} != {ICON_SIZE}')
icon_sha = hashlib.sha256(icon).hexdigest()
if icon_sha != ICON_SHA256:
    raise SystemExit(
        f'[FAIL] launcher icon sha256 {icon_sha} != {ICON_SHA256}')
if not icon.startswith(b'\xff\xd8'):
    raise SystemExit('[FAIL] launcher icon is not JPEG')
ICON_OUT.write_bytes(icon)

print(
    f'[PASS] classroom pack: {len(classroom)} bytes sha256={classroom_sha}')
print(
    f'[PASS] EUREKA pack: {len(eureka)} bytes / {EUREKA_COUNT} frames sha256={eureka_sha}')
print(
    f'[PASS] launcher icon: {len(icon)} bytes sha256={icon_sha}')
