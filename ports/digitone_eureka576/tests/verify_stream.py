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
    raise SystemExit("invalid original EUREKA CRF1")
count, = struct.unpack_from("<I", data, 4)
if count != 576:
    raise SystemExit("original source is not the complete 576 frames")
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
if len(compressed) != 145430:
    raise SystemExit(f"unexpected compressed size {len(compressed)}")
expected_sha = "ed3f236f843dc30407b757beefc62424152b17d3ce63215e87b3e1a9e23c3016"
if hashlib.sha256(compressed).hexdigest() != expected_sha:
    raise SystemExit("target stream sha256 changed")
if module.build_seek(frames, compressed) != seek.read_bytes():
    raise SystemExit("seek table does not cover all 576 exact original frames")
if seek.stat().st_size != 96 * 1028:
    raise SystemExit("invalid checkpoint table length")
reference.write_bytes(b"".join(frames))
assert reference.stat().st_size == 576 * 1024
print(f"PASS: independent reference for {count} exact frames; stream {len(compressed)} bytes")
