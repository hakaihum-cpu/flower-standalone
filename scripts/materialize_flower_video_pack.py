#!/usr/bin/env python3
from pathlib import Path
import base64
import hashlib

ROOT = Path(__file__).resolve().parents[1]
CHUNK_DIR = ROOT / "Resources" / "eureka_frame_chunks"
OUT = ROOT / "Resources" / "flower_video_frames.pack"
EXPECTED_SHA256 = "827b5aa6a339927cb909a5ffe1b129513d946911cc78ea96d34aa110e6cd1883"
EXPECTED_SIZE = 1872671
EXPECTED_COUNT = 144

def decode_chunks(paths):
    decoded = bytearray()
    for path in paths:
        encoded = "".join(path.read_text(encoding="ascii").split())
        decoded.extend(base64.b64decode(encoded, validate=True))
    return bytes(decoded)

parts = sorted(CHUNK_DIR.glob("*.b64"))
if len(parts) != 21:
    raise SystemExit(f"[FAIL] expected 21 EFFECTS video chunks, got {len(parts)}")

data = decode_chunks(parts)
if len(data) != EXPECTED_SIZE:
    raise SystemExit(f"[FAIL] video pack size {len(data)} != {EXPECTED_SIZE}")

sha = hashlib.sha256(data).hexdigest()
if sha != EXPECTED_SHA256:
    raise SystemExit(f"[FAIL] video pack sha256 {sha} != {EXPECTED_SHA256}")

if data[:4] != b"CRF1":
    raise SystemExit("[FAIL] video pack magic mismatch")
count = int.from_bytes(data[4:8], "little")
if count != EXPECTED_COUNT:
    raise SystemExit(f"[FAIL] video frame count {count} != {EXPECTED_COUNT}")

OUT.write_bytes(data)
print(f"[PASS] FLOWER EFFECTS-video pack: {len(data)} bytes / {count} frames sha256={sha}")
