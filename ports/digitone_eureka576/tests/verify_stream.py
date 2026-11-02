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
    raise SystemExit("invalid original EUREKA CRF1 (face-outline conversion)")
count, = struct.unpack_from("<I", data, 4)
if count != 576:
    raise SystemExit("original source is not the complete 576 EUREKA frames")
index_end = 8 + count * 12
if len(data) < index_end:
    raise SystemExit("source index truncated")
frames = []
jpegs = []
for i in range(count):
    off, _flags, length = struct.unpack_from("<III", data, 8 + i * 12)
    if off < index_end or off + length > len(data):
        raise SystemExit(f"bad source frame {i}")
    frame = data[off:off+length]
    if not (frame.startswith(b"\xff\xd8") and frame.endswith(b"\xff\xd9")):
        raise SystemExit(f"corrupt user frame {i}")
    jpegs.append(frame)
regions = module.choose_clip_crops(jpegs)
for i, jpeg in enumerate(jpegs):
    frames.append(module.pack_image(jpeg, i, regions[i // 36]))
compressed = packed.read_bytes()
if module.decode(compressed, count) != frames:
    raise SystemExit("compressed target frames differ from original images")
if not 0 < len(compressed) < count * 1024:
    raise SystemExit(f"unexpected packed EUREKA contour size {len(compressed)}")
if module.encode(frames)[0] != compressed:
    raise SystemExit("encoded contour stream does not match exact source conversion")
density = [sum(int(byte).bit_count() for byte in frame) for frame in frames]
if not all(60 <= pixels <= 850 for pixels in density):
    raise SystemExit(f"unexpected contour pixel density {min(density)}..{max(density)}")
if module.build_seek(frames, compressed) != seek.read_bytes():
    raise SystemExit("seek table does not cover all 576 exact original frames")
if seek.stat().st_size != 96 * 1028:
    raise SystemExit("invalid checkpoint table length")
reference.write_bytes(b"".join(frames))
assert reference.stat().st_size == 576 * 1024
print(f"PASS: 576 exact EUREKA contour reference frames; packed {len(compressed)}; pixels {min(density)}..{max(density)}")
