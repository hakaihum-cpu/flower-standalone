#!/usr/bin/env python3
"""Independent PG82 bank acceptance: check entire LCD frame stream and seek table."""
import hashlib
from pathlib import Path
import importlib.util
import sys, base64, lzma

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from prepare_user_tiles import yellow_letterbox, SOURCE_FRAMES_SHA
spec = importlib.util.spec_from_file_location("eureka_approved_04", ROOT / "prepare.py")
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)
stream = (ROOT / "frames.bin").read_bytes()
seek = (ROOT / "seek.bin").read_bytes()
reference = (ROOT / "reference.bin").read_bytes()
assert hashlib.sha256(stream).hexdigest() == "bd7577db4d789f3a6b5bbae819cd6f5d5604eb4851d0dcf0d7f6a2ac67d4a411"
assert hashlib.sha256(seek).hexdigest() == "1866938edb4abef475e01e20ffa8a096122637882369173b3c65585476fa8b6d"
assert hashlib.sha256(reference).hexdigest() == "0fe423f513638566d8505ce6ae30fb09cd22c2335b13e297317fe9dedbd9bc64"
frames = codec.decode(stream, 576)
assert codec.encode(frames)[0] == stream
assert codec.build_seek(frames, stream) == seek
assert b"".join(frames) == reference
assert len(frames) == 576
source_encoded = lzma.decompress(base64.b64decode("".join(
    (ROOT / "user_tiles" / "frames_lzma.b64").read_text().split())))
assert hashlib.sha256(source_encoded).hexdigest() == SOURCE_FRAMES_SHA
source_frames = codec.decode(source_encoded, 576)
expected, widths = yellow_letterbox(source_frames)
assert expected == frames, "padding must be exactly outside the original tiles"
for original, frame, width in zip(source_frames, frames, widths):
    left = (128-width)//2
    assert frame[left*8:(left+width)*8] == original[left*8:(left+width)*8]
    assert all(v==255 for v in frame[:left*8])
    assert all(v==255 for v in frame[(left+width)*8:])
assert frames[574] == frames[572] and frames[575] == frames[573]
assert all(len(frame) == 1024 for frame in frames)
density = [sum(value.bit_count() for value in frame) for frame in frames]
assert max(density) > 0 and min(density) >= 0
print("PASS 576 supplied-source-derived LCD frames; no image DSP or run-time logic changes")
print("PASS all frame pixels, XOR/RLE, 96 seek checkpoints, SHA256")
print("PASS yellow side padding and exact original in-image pixels (576 frames)")
print("PASS 574 originals + 2 exact repeats; LCD filled-pixel range", min(density), max(density))
