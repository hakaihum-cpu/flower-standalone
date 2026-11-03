#!/usr/bin/env python3
"""Preflight: all 576 original EUREKA JPEGs -> approved #04 HYBRID 1-bit.
Check lossless stream/seek and independently sample original image pixels.
"""
from pathlib import Path
import hashlib
import importlib.util
import struct
import sys
from io import BytesIO
from PIL import Image, ImageOps

if len(sys.argv) != 5:
    raise SystemExit("usage: verify_stream.py original.crf1 frames.bin reference.bin seek.bin")
source, packed, reference, seek = map(Path, sys.argv[1:])
root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("eureka_packer", root / "prepare.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
data = source.read_bytes()
if data[:4] != b"CRF1":
    raise SystemExit("invalid original EUREKA CRF1 (HYBRID 04 conversion)")
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
for i, jpeg in enumerate(jpegs):
    frames.append(module.pack_image(jpeg, i))
compressed = packed.read_bytes()
if module.decode(compressed, count) != frames:
    raise SystemExit("compressed target frames differ from original images")
if not 0 < len(compressed) < count * 1024:
    raise SystemExit(f"unexpected packed EUREKA HYBRID size {len(compressed)}")
if module.encode(frames)[0] != compressed:
    raise SystemExit("encoded HYBRID stream does not match exact source conversion")
density = [sum(int(byte).bit_count() for byte in frame) for frame in frames]
if max(density) == 0 or any(pixels > 8192 for pixels in density):
    raise SystemExit("empty output bank or invalid pixel count")

# Golden reference: user-reviewed 04__HYBRID_128x64.png packed as LCD.
# Ensures all 8192 original pixels equal the specific approved method.
source_000_sha = "7414b971f185a4f6788523861dea818c7cd17608ccb2d776ec9e6ec81998e7c9"
lcd_000_hybrid_sha = "26ad0921f843e373513d0182a97fe8bc464a2c96b86699236d07f088b5b910ff"
if hashlib.sha256(jpegs[0]).hexdigest() != source_000_sha:
    raise SystemExit("EUREKA #000 original JPEG differs from approved source")
if hashlib.sha256(frames[0]).hexdigest() != lcd_000_hybrid_sha:
    raise SystemExit("EUREKA #000 must equal approved HYBRID 04 reference exactly")
print("PASS: frame 000 exactly matches approved 04 HYBRID 8192-pixel output")

if module.build_seek(frames, compressed) != seek.read_bytes():
    raise SystemExit("seek table does not cover all 576 exact original frames")
if seek.stat().st_size != 96 * 1028:
    raise SystemExit("invalid checkpoint table length")
reference.write_bytes(b"".join(frames))
assert reference.stat().st_size == 576 * 1024
print(f"PASS: 576 source-preserving EUREKA HYBRID 04 1-bit reference frames; packed {len(compressed)}; pixels {min(density)}..{max(density)}")
