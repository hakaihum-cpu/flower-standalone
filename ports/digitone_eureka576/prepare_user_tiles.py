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
import zlib

ROOT = Path(__file__).resolve().parent
SOURCE_FRAMES_SHA = "ef27b3bf8b85a0cdb3b7461df0a5d76b110138ddd7bf1177deb3e8f78d865c47"
FRAMES_SHA = "bd7577db4d789f3a6b5bbae819cd6f5d5604eb4851d0dcf0d7f6a2ac67d4a411"
LZMA_SHA = "996db220a837e7ca5e67d38d115e4d6552132b763b97071b9c80ab0d965781ce"
SEEK_SHA = "1866938edb4abef475e01e20ffa8a096122637882369173b3c65585476fa8b6d"
REFERENCE_SHA = "0fe423f513638566d8505ce6ae30fb09cd22c2335b13e297317fe9dedbd9bc64"
COUNT = 576
# Exact contained-image widths measured from all 574 original user JPEG crops
# using the unchanged Pillow ImageOps.contain method; height is always 64px.
# The final two frames repeat the last two original source JPEGs.
WIDTHS_ZLIB_B64 = (
    "eNplj2luhDAMhW/FpCWJtwy9CctUKqH3/9HnsI3Uz8gytnk8i4iKwydydtASvLEQEZOjb9iezYo5IYSP4Ai3dSaocFNgfa+oVT49K1NM1eOuvoZSytDC4MQ8wqPrugcikBLlZqi4Pxi+OmyMnztHh6+da6RF8QuQOKWYABtRgmWWwpyxIaQ5J0J5da7iWua9A+mn4AMc4oJ9SjnvI9J9B4dLhmCEIP0fURPMpyBGOsChabFdMOaUw0FvbhiuIw9+MG7MqhH0eMjiAZV0IPJ5kLZat7oh//6crNM4TuPk+XXyfVfLPC/zgvy6e3Vd61qRt3X7A4denTk="
)
WIDTH_SHA = "dc0e69d4e412d3756009df20e3e9302b499f0f974b33e00294002a04de5d5eec"

def yellow_letterbox(original_frames):
    """Only outside the contained image: LCD bit 1 => yellow, 0 => black.

    Full-height JPEG crops permit fast 8-byte column writes. In-image bits
    remain exactly identical to PG82, including #04 HYBRID local CLAHE.
    """
    widths = zlib.decompress(base64.b64decode(WIDTHS_ZLIB_B64, validate=True))
    assert len(widths) == COUNT
    assert hashlib.sha256(widths).hexdigest() == WIDTH_SHA
    frames = []
    num_padding = 0
    for original, width in zip(original_frames, widths):
        assert 1 <= width <= 128 and len(original) == 1024
        left = (128-width)//2
        right = left+width
        frame = bytearray(original)
        for x in range(128):
            if x < left or x >= right:
                frame[8*x:8*x+8] = bytes([255])*8
                num_padding += 64
        assert frame[left*8:right*8] == original[left*8:right*8], "in-image pixels changed"
        frames.append(bytes(frame))
    assert num_padding == 2142720
    assert frames[574] == frames[572] and frames[575] == frames[573]
    return frames, widths

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
    source_encoded = decompressor.decompress(compressed, max_length=589825)
    assert decompressor.eof and not decompressor.unused_data
    assert len(source_encoded) < COUNT * 1024
    assert hashlib.sha256(source_encoded).hexdigest() == SOURCE_FRAMES_SHA, "PG82 bank mismatch"
    packer = load_converter()
    original = packer.decode(source_encoded, COUNT)
    assert packer.encode(original)[0] == source_encoded, "source codec changed"
    frames, widths = yellow_letterbox(original)
    encoded = packer.encode(frames)[0]
    assert hashlib.sha256(encoded).hexdigest() == FRAMES_SHA, "PG83 image SHA mismatch"
    assert packer.decode(encoded, COUNT) == frames
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
    print("PASS letterbox lit yellow; original image pixels unchanged, 576 frames")
    print("PASS encoded 291749 bytes, seek 98688 bytes, lossless re-encode and SHA256")
    return frames, encoded, seek

if __name__ == "__main__":
    build()
