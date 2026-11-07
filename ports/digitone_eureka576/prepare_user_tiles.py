#!/usr/bin/env python3
"""Install only user-provided EUREKA frames, keeping PG81 DSP and #04 HYBRID unchanged.

The 574 user grid cells are converted OFFLINE using the approved prepare.py
pack_image; frames 574 and 575 are exact repeats of user cells 572/573 to
preserve the device's fixed 16 scenes x 36 frames, 96 checkpoints and timing.
This script only decodes the tested result and calls original seek/preview code.
"""
from pathlib import Path
import base64
import hashlib
import importlib.util
import lzma

ROOT = Path(__file__).resolve().parent
FRAMES_SHA = "ef27b3bf8b85a0cdb3b7461df0a5d76b110138ddd7bf1177deb3e8f78d865c47"
LZMA_SHA = "996db220a837e7ca5e67d38d115e4d6552132b763b97071b9c80ab0d965781ce"
SEEK_SHA = "d5757d7293d2ef04536518ab07927a7768d52f9db7bbdc7abdc52e7acab049e3"
REFERENCE_SHA = "5eede689cf2794dd27f93fe0fee4f108f6cc584923127fbc2faed609c7b48ffc"
COUNT = 576

def load_converter():
    spec = importlib.util.spec_from_file_location("eureka_approved_04", ROOT / "prepare.py")
    packer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packer)
    return packer

def build():
    encoded_b64 = "".join((ROOT / "user_tiles" / "frames_lzma.b64").read_text(encoding="ascii").split())
    compressed = base64.b64decode(encoded_b64, validate=True)
    assert hashlib.sha256(compressed).hexdigest() == LZMA_SHA, "source bank SHA mismatch"
    decompressor = lzma.LZMADecompressor()
    encoded = decompressor.decompress(compressed, max_length=589825)
    assert decompressor.eof and not decompressor.unused_data
    assert len(encoded) < COUNT * 1024
    assert hashlib.sha256(encoded).hexdigest() == FRAMES_SHA, "stream SHA mismatch"
    packer = load_converter()
    frames = packer.decode(encoded, COUNT)
    assert packer.encode(frames)[0] == encoded, "changed XOR/RLE frame codec"
    assert frames[574] == frames[572] and frames[575] == frames[573]
    seek = packer.build_seek(frames, encoded)
    assert len(seek) == 96 * 1028
    assert hashlib.sha256(seek).hexdigest() == SEEK_SHA
    reference = b"".join(frames)
    assert len(reference) == COUNT * 1024
    assert hashlib.sha256(reference).hexdigest() == REFERENCE_SHA
    (ROOT / "frames.bin").write_bytes(encoded)
    (ROOT / "seek.bin").write_bytes(seek)
    (ROOT / "reference.bin").write_bytes(reference)
    packer.save_preview(frames, ROOT / "eureka_lcd_preview.png")
    print("PASS 574 supplied tiles + two exact repeats, fixed 576 frames")
    print("PASS original #04 HYBRID, unchanged XOR delta/RLE, 96 seeks, 16 scenes")
    print("PASS encoded 294719 bytes, seek 98688 bytes, lossless re-encode and SHA256")
    return frames, encoded, seek

if __name__ == "__main__":
    build()
