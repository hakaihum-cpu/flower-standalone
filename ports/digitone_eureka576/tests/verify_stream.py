#!/usr/bin/env python3
"""Preflight independent of firmware build: every user-supplied JPEG -> target frame.
Compares the produced compressed file and its decoder to a new reference.
"""
from pathlib import Path
import hashlib
import importlib.util
import struct
import sys

if len(sys.argv) != 5:
    raise SystemExit("usage: verify_stream.py original.crf1 frames.bin reference.bin seek.bin")
source, packed, reference, seek = map(Path, sys.argv[1:])
root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("eureka_packer", root / "prepare.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
data = source.read_bytes()
if data[:4] != b"CRF1":
    raise SystemExit("invalid original MOOD CRF1")
count, = struct.unpack_from("<I", data, 4)
if count != 343:
    raise SystemExit("original source is not the complete 343 MOOD frames")
index_end = 8 + count * 12
if len(data) < index_end:
    raise SystemExit("source index truncated")
frames = []
for i in range(count):
    off, _flags, length = struct.unpack_from("<III", data, 8 + i * 12)
    if off < index_end or off + length > len(data):
        raise SystemExit(f"bad source frame {i}")
    frame = data[off:off+length]
    if not (frame.startswith(b"\xff\xd8") and frame.endswith(b"\xff\xd9")):
        raise SystemExit(f"corrupt user frame {i}")
    frames.append(module.pack_image(frame))
compressed = packed.read_bytes()
if module.decode(compressed, count) != frames:
    raise SystemExit("compressed target frames differ from original images")
if not 0 < len(compressed) < count * 1024 - 130000:
    raise SystemExit(f"compressed source is too large or empty: {len(compressed)}")
expected_stream, _modes = module.encode(frames)
if expected_stream != compressed:
    raise SystemExit("packed stream bytes differ from source-encoded frames")
# Every MOOD photo must produce clear yellow contours without filled-in white areas.
density = [sum(byte.bit_count() for byte in frame) for frame in frames]
if min(density) < 720 or max(density) > 1350:
    raise SystemExit(f"outline pixel count invalid: {min(density)}..{max(density)}")
if module.build_seek(frames, compressed) != seek.read_bytes():
    raise SystemExit("seek table does not cover all 343 exact MOOD frames")
if seek.stat().st_size != 58 * 1028:
    raise SystemExit("invalid checkpoint table length")
reference.write_bytes(b"".join(frames))
assert reference.stat().st_size == 343 * 1024
print(f"PASS: exact MOOD reference for {count} frames; yellow pixels {min(density)}..{max(density)}; packed {len(compressed)}; sha256 {hashlib.sha256(compressed).hexdigest()}")
