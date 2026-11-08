#!/usr/bin/env python3
"""Independent no-build proof for the staged MTL1 628-frame MIYAKO resource.

References the SHA-pinned canonical VQ archive, then checks ALL frames
against both a Python LZ4 decoder and, optionally, the C host decoder.
MTL1 is a lossless re-packing of the already lossy VQ bitmap, NOT a
reconstruction of original full-resolution user artwork.

This test does not build firmware, flash the device or modify res.bin.
"""
from __future__ import annotations
import argparse
import base64
from pathlib import Path
import random
import struct
import subprocess
import tempfile

from pack_miyako_resource import CHUNK_DIR, read_payload

ROOT = Path(__file__).resolve().parents[3]
N, CLIPS, STEP, MAP, BOOK = 628, 24, 27, 288, 4096
MAX_DATA = 69 * 4096 - 32
HEADER = struct.Struct("<4sHHHHIIII")
ENTRY = struct.Struct("<IHH")
PAL = HEADER.size
BOOK_DIR = PAL + CLIPS * 32
MAP_DIR = BOOK_DIR + CLIPS * ENTRY.size
BODY = MAP_DIR + N * ENTRY.size
PACKED_RESOURCE = ROOT / "Resources/fm1_photo_tile_poc_current.b64"


def lz4_decode(data: bytes, limit: int) -> bytes:
    out, i = bytearray(), 0
    while i < len(data):
        token = data[i]
        i += 1
        literal = token >> 4
        if literal == 15:
            while True:
                if i == len(data):
                    raise ValueError("truncated literal")
                ext = data[i]
                i += 1
                literal += ext
                if ext != 255:
                    break
        if i + literal > len(data) or len(out) + literal > limit:
            raise ValueError("literal outside bounds")
        out.extend(data[i:i + literal])
        i += literal
        if i == len(data):
            break
        if i + 2 > len(data):
            raise ValueError("truncated offset")
        offset = data[i] | data[i + 1] << 8
        i += 2
        if offset == 0 or offset > len(out):
            raise ValueError("invalid back-reference")
        count = (token & 15) + 4
        if (token & 15) == 15:
            while True:
                if i == len(data):
                    raise ValueError("truncated match")
                ext = data[i]
                i += 1
                count += ext
                if ext != 255:
                    break
        if len(out) + count > limit:
            raise ValueError("match outside bounds")
        for _ in range(count):
            out.append(out[-offset])
    if len(out) != limit:
        raise ValueError("decoded size mismatch")
    return bytes(out)


def block(data: bytes, at: int, expected: int) -> bytes:
    offset, size, raw = ENTRY.unpack_from(data, at)
    if raw not in (0, 1) or not size or offset < BODY or size > len(data) - offset:
        raise ValueError("invalid indexed record")
    encoded = data[offset:offset + size]
    if raw:
        if size != expected:
            raise ValueError("invalid raw block")
        return encoded
    return lz4_decode(encoded, expected)


def decoded_frame(payload: bytes, f: int) -> tuple[bytes, bytes]:
    if not BODY <= len(payload) <= MAX_DATA or not 0 <= f < N:
        raise ValueError("resource/frame out of bounds")
    if HEADER.unpack_from(payload) != (
        b"MTL1", 1, N, CLIPS, STEP, PAL, BOOK_DIR, MAP_DIR, BODY
    ):
        raise ValueError("resource header mismatch")
    clip = f // STEP
    palette = payload[PAL + 32 * clip:PAL + 32 * (clip + 1)]
    book = block(payload, BOOK_DIR + clip * ENTRY.size, BOOK)
    selectors = block(payload, MAP_DIR + f * ENTRY.size, MAP)
    out = bytearray(96 * 96 // 2)
    for ty in range(24):
        for tx in range(12):
            tile = selectors[ty * 12 + tx]
            tile_bytes = book[tile * 16:(tile + 1) * 16]
            for row in range(4):
                off = (ty * 4 + row) * 48 + tx * 4
                out[off:off + 4] = tile_bytes[row * 4:row * 4 + 4]
    return palette, bytes(out)


def original_frame(old: bytes, f: int) -> tuple[bytes, bytes]:
    c = f // STEP
    palette = old[32 * c:32 * (c + 1)]
    book = old[768 + c * BOOK:768 + (c + 1) * BOOK]
    selectors = old[99072 + f * MAP:99072 + (f + 1) * MAP]
    pixels = bytearray(4608)
    for ty in range(24):
        for tx in range(12):
            tile = selectors[ty * 12 + tx]
            for row in range(4):
                k = (ty * 4 + row) * 48 + tx * 4
                pixels[k:k + 4] = book[tile * 16 + row * 4:tile * 16 + row * 4 + 4]
    return palette, bytes(pixels)


def test():
    old = read_payload(CHUNK_DIR)  # checksum verified by the existing approved tool
    encoded = base64.b64decode("".join(PACKED_RESOURCE.read_text().split()), validate=True)
    assert len(old) == 279936
    assert len(encoded) == 272521 and len(encoded) <= MAX_DATA
    for f in range(N):
        assert decoded_frame(encoded, f) == original_frame(old, f), f
    order = list(range(N))
    random.Random(0x4D49).shuffle(order)
    for f in order:
        assert decoded_frame(encoded, f) == original_frame(old, f), ("random", f)
    bad = bytearray(encoded)
    bad[0] ^= 255
    try:
        decoded_frame(bad, 0)
    except ValueError:
        pass
    else:
        raise AssertionError("corrupt magic accepted")
    bad = bytearray(encoded)
    struct.pack_into("<I", bad, MAP_DIR, 0xFFFFFFFF)
    try:
        decoded_frame(bad, 0)
    except ValueError:
        pass
    else:
        raise AssertionError("out-of-bounds index accepted")
    print("PASS: 628 sequential, 628 shuffled, palette+pixel exact VQ restoration")
    return old, encoded


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="cc", help="host C compiler")
    ap.add_argument("--skip-c", action="store_true", help="Python-only check")
    args = ap.parse_args()
    old, new = test()
    if args.skip_c:
        return
    with tempfile.TemporaryDirectory() as d:
        root = Path(d)
        (root / "old.bin").write_bytes(old)
        (root / "new.bin").write_bytes(new)
        cmd = [
            args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
            "-I", str(ROOT / "fm1/native"),
            str(ROOT / "fm1/native/mi_shared_tile_codec_poc.c"),
            str(ROOT / "fm1/native/test_mi_shared_tile_codec_poc.c"),
            "-o", str(root / "test"),
        ]
        subprocess.run(cmd, check=True)
        subprocess.run(
            [str(root / "test"), str(root / "old.bin"), str(root / "new.bin")],
            check=True,
        )
        print("PASS: independent C decoder compiled and verified")


if __name__ == "__main__":
    main()
