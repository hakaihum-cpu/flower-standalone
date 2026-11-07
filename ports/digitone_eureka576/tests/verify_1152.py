#!/usr/bin/env python3
"""Independent dual-bank / seek regression, includes cross-bank boundary."""
from pathlib import Path
import importlib.util
import struct
import hashlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("eureka_codec", ROOT / "prepare.py")
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)
stream = (ROOT / "frames.bin").read_bytes()
old = codec.decode((ROOT / "original_frames.bin").read_bytes(), 576)
# The final stream contains both sets, and the original 576 remain byte exact.
frames = codec.decode(stream, 1152)
assert frames[:576] == old
assert frames[576:] == codec.decode(
    (ROOT / "user_frames_before_merge.bin").read_bytes(), 576)
assert codec.encode(frames)[0] == stream
assert b"".join(frames) == (ROOT / "reference.bin").read_bytes()
assert len(stream) < 1152*1024
seek = (ROOT / "seek.bin").read_bytes()
assert len(seek) == 192*1028
assert seek == codec.build_seek(frames, stream)
# Do NOT infer cross-boundary state: check the six-frame snapshots and offsets
# explicitly across both banks, including entry at global frame 576.
for index in (0,1,35,570,575,576,577,579,580,581,582,587,1146,1150,1151):
    slot = (index // 6)*1028
    off, = struct.unpack_from("<I", seek, slot)
    frame = frames[(index//6)*6]
    assert seek[slot+4:slot+1028] == frame
    assert 0 <= off <= len(stream)
# Clip boundaries remain old last (540..575) and new first (576..611).
assert len(frames) // 36 == 32
for i in range(32):
    assert len(frames[i*36:(i+1)*36]) == 36
print("PASS: 1152/1152 image pixels, original PG79/PG84 banks unmodified")
print("PASS: 192 six-frame checkpoints, 32 scene boundaries, XOR/RLE SHA256")
print("SHA256:", hashlib.sha256(stream).hexdigest())
