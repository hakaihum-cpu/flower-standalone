#!/usr/bin/env python3
from pathlib import Path
import base64
import hashlib

ROOT = Path(__file__).resolve().parents[1]
CHUNK_DIR = ROOT / "Resources" / "effects_video_chunks"
OUT = ROOT / "Resources" / "effects_video_frames.pack"
EXPECTED_SHA256 = "827b5aa6a339927cb909a5ffe1b129513d946911cc78ea96d34aa110e6cd1883"
EXPECTED_SIZE = 1872671
EXPECTED_COUNT = 144

parts = sorted(CHUNK_DIR.glob("*.b64"))
if len(parts) != 21:
    raise SystemExit(f"[FAIL] expected 21 EFFECTS video frame chunks, got {len(parts)}")

data = bytearray()
for path in parts:
    encoded = "".join(path.read_text(encoding="ascii").split())
    data.extend(base64.b64decode(encoded, validate=True))

payload = bytes(data)
if len(payload) != EXPECTED_SIZE:
    raise SystemExit(f"[FAIL] frame pack size {len(payload)} != {EXPECTED_SIZE}")

actual_sha = hashlib.sha256(payload).hexdigest()
if actual_sha != EXPECTED_SHA256:
    raise SystemExit(f"[FAIL] frame pack sha256 {actual_sha} != {EXPECTED_SHA256}")

if payload[:4] != b"CRF1":
    raise SystemExit("[FAIL] frame pack magic mismatch")
count = int.from_bytes(payload[4:8], "little")
if count != EXPECTED_COUNT:
    raise SystemExit(f"[FAIL] frame count {count} != {EXPECTED_COUNT}")

OUT.write_bytes(payload)
print(f"[PASS] EFFECTS video frame pack: {len(payload)} bytes / {count} frames sha256={actual_sha}")
