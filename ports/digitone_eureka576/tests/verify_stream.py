#!/usr/bin/env python3
"""Preflight: exact RGB source composition -> 1-bit Bayer output for 576 JPEGs.
Check lossless stream/seek and independently sample original image pixels.
"""
from pathlib import Path
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
    raise SystemExit("invalid original EUREKA CRF1 (RG DOT conversion)")
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
    raise SystemExit(f"unexpected packed EUREKA RG DOT size {len(compressed)}")
if module.encode(frames)[0] != compressed:
    raise SystemExit("encoded RG DOT stream does not match exact source conversion")
density = [sum(int(byte).bit_count() for byte in frame) for frame in frames]
if max(density) == 0 or any(pixels > 8192 for pixels in density):
    raise SystemExit("empty output bank or invalid pixel count")

# Independent sample of pixel-level output, not a second call to pack_image.
# This detects crop/redraw, differing Bayer phase, RGB weights and thresholds.
bayer4 = (0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5)
for i in (0, 35, 72, 143, 216, 323, 432, 575):
    with Image.open(BytesIO(jpegs[i])) as original:
        fitted = ImageOps.contain(original.convert("RGB"), (128, 64),
                                  method=Image.Resampling.LANCZOS)
    composition = Image.new("RGB", (128, 64), (0, 0, 0))
    composition.paste(fitted, ((128 - fitted.width) // 2,
                               (64 - fitted.height) // 2))
    pixels = composition.load()
    for y in (0, 1, 8, 17, 32, 42, 55, 63):
        for x in (0, 3, 17, 49, 65, 87, 111, 127):
            red, green, blue = pixels[x, y]
            light = (0.2126 * red + 0.7152 * green + 0.0722 * blue) / 255.0
            threshold = 0.13 + 0.63 * bayer4[(x % 4) + (y % 4) * 4] / 16.0
            expected = light >= threshold
            actual = bool(frames[i][7 - (y >> 3) + 8 * x] & (1 << (y & 7)))
            if actual != expected:
                raise SystemExit(f"RG DOT pixel mismatch: frame={i} x={x} y={y}")
if module.build_seek(frames, compressed) != seek.read_bytes():
    raise SystemExit("seek table does not cover all 576 exact original frames")
if seek.stat().st_size != 96 * 1028:
    raise SystemExit("invalid checkpoint table length")
reference.write_bytes(b"".join(frames))
assert reference.stat().st_size == 576 * 1024
print(f"PASS: 576 source-preserving EUREKA RG DOT 1-bit reference frames; packed {len(compressed)}; pixels {min(density)}..{max(density)}")
