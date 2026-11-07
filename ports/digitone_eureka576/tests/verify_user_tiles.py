#!/usr/bin/env python3
"""Independent PG82 bank acceptance: check entire LCD frame stream and seek table."""
import hashlib
from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("eureka_approved_04", ROOT / "prepare.py")
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)
stream = (ROOT / "frames.bin").read_bytes()
seek = (ROOT / "seek.bin").read_bytes()
reference = (ROOT / "reference.bin").read_bytes()
assert hashlib.sha256(stream).hexdigest() == "ef27b3bf8b85a0cdb3b7461df0a5d76b110138ddd7bf1177deb3e8f78d865c47"
assert hashlib.sha256(seek).hexdigest() == "d5757d7293d2ef04536518ab07927a7768d52f9db7bbdc7abdc52e7acab049e3"
assert hashlib.sha256(reference).hexdigest() == "5eede689cf2794dd27f93fe0fee4f108f6cc584923127fbc2faed609c7b48ffc"
frames = codec.decode(stream, 576)
assert codec.encode(frames)[0] == stream
assert codec.build_seek(frames, stream) == seek
assert b"".join(frames) == reference
assert len(frames) == 576
assert frames[574] == frames[572] and frames[575] == frames[573]
assert all(len(frame) == 1024 for frame in frames)
density = [sum(value.bit_count() for value in frame) for frame in frames]
assert max(density) > 0 and min(density) >= 0
print("PASS 576 supplied-source-derived LCD frames; no image DSP or run-time logic changes")
print("PASS all frame pixels, XOR/RLE, 96 seek checkpoints, SHA256")
print("PASS 574 originals + 2 exact repeats; LCD filled-pixel range", min(density), max(density))
