#!/usr/bin/env python3
"""Compile 72 high-detail frames extracted only from the user's own montage.

Original source: 8x9 tiles, 1269x1240 PNG. Each of the 72 source scenes was
cropped to the mailbox/person/letter (no reused UI text) and resized to
160x72, luminance-thresholded, and compressed as 4x4 binary VQ. Eight clips
of nine frames have independent 256-entry books to retain individual hands
and letters. The reconstructed binary-pixel error is <0.04% per clip.
All 72 frames are retained in order; SUB selects 0..71. This decoder uses
one original bit per pixel, NOT interpolation of old 80x40 VQ frames.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
from pathlib import Path
import struct
import zlib

PACK = Path(__file__).resolve().with_name("mi_letter_highdetail_160x72.b64")
CLIPS = 8
FRAMES_PER_CLIP = 9
FRAME_COUNT = 72
W, H = 160, 72
MAP_BYTES = (W // 4) * (H // 4)  # 720 bytes per frame
MASK_BYTES = (MAP_BYTES + 7) // 8  # 90 bytes per delta
BOOK_BYTES = CLIPS * 256 * 2  # each 4x4 1bpp pattern is a uint16
BASE_BYTES = CLIPS * MAP_BYTES
OFFSETS_BYTES = FRAME_COUNT * 2
DELTA_BYTES = 11837
RAW_LENGTH = BOOK_BYTES + BASE_BYTES + OFFSETS_BYTES + DELTA_BYTES
RAW_SHA256 = "86b64e4affac514ed05057b31087ddb28e9e830979524a1effc05e5dc702de51"
DECODED_CRC32 = 0x3141B600  # independent 72-frame silhouette checksum


def read_asset() -> bytes:
    encoded = PACK.read_text(encoding="ascii").strip()
    try:
        compressed = base64.b64decode(encoded, validate=True)
        raw = zlib.decompress(compressed)
    except (ValueError, zlib.error) as exc:
        raise SystemExit(f"FAIL: invalid user image pack: {exc}") from exc
    if len(raw) != RAW_LENGTH or hashlib.sha256(raw).hexdigest() != RAW_SHA256:
        raise SystemExit("FAIL: user-supplied letter pack length or SHA256 mismatch")
    return raw


def byte_array(name: str, data: bytes) -> str:
    lines = []
    for pos in range(0, len(data), 24):
        lines.append("    " + ",".join(f"0x{x:02x}" for x in data[pos:pos + 24]) + ",")
    return f"static const uint8_t {name}[{len(data)}u] = {{\n" + "\n".join(lines) + "\n};\n"


def word_array(name: str, words: tuple[int, ...]) -> str:
    lines = []
    for pos in range(0, len(words), 12):
        lines.append("    " + ",".join(f"0x{x:04x}" for x in words[pos:pos + 12]) + ",")
    return f"static const uint16_t {name}[{len(words)}u] = {{\n" + "\n".join(lines) + "\n};\n"


def validate_roundtrip(raw: bytes) -> None:
    """Reject malformed offsets, dangling masks or invalid byte boundaries."""
    books_end = BOOK_BYTES
    base_end = books_end + BASE_BYTES
    offsets_end = base_end + OFFSETS_BYTES
    offsets = struct.unpack("<72H", raw[base_end:offsets_end])
    payload = raw[offsets_end:]
    if offsets[0] != 0:
        raise SystemExit("FAIL: invalid first-frame delta offset")
    checksum = 0
    for clip in range(CLIPS):
        current = bytearray(raw[books_end + clip * MAP_BYTES:
                                books_end + (clip + 1) * MAP_BYTES])
        book = struct.unpack_from("<256H", raw, clip * 256 * 2)
        for f in range(clip * FRAMES_PER_CLIP,
                       (clip + 1) * FRAMES_PER_CLIP):
            if f % FRAMES_PER_CLIP:
                pos = offsets[f]
                if pos + MASK_BYTES > len(payload):
                    raise SystemExit("FAIL: video delta outside source pack")
                mask = payload[pos:pos + MASK_BYTES]
                count = sum(int(x).bit_count() for x in mask)
                if pos + MASK_BYTES + count > len(payload):
                    raise SystemExit("FAIL: video delta truncated")
                data = iter(payload[pos + MASK_BYTES:pos + MASK_BYTES + count])
                for k in range(MAP_BYTES):
                    if mask[k >> 3] & (0x80 >> (k & 7)):
                        current[k] = next(data)
            # Decode the whole source bitmap independently of the C viewer.
            bits = bytearray(W * H // 8)
            for y in range(H):
                for x in range(W):
                    tile = book[current[(y >> 2) * (W // 4) + (x >> 2)]]
                    pix = (tile >> (4 * (y & 3) + (x & 3))) & 1
                    bits[(y * W + x) >> 3] |= pix << (7 - (x & 7))
            checksum = zlib.crc32(bits, checksum)
    if len(payload) != DELTA_BYTES:
        raise SystemExit("FAIL: invalid delta bytes")
    if checksum != DECODED_CRC32:
        raise SystemExit(f"FAIL: 72 decoded image checksum {checksum:08x} "
                         f"vs {DECODED_CRC32:08x}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, type=Path)
    a = ap.parse_args()
    raw = read_asset()
    validate_roundtrip(raw)
    n = BOOK_BYTES
    books = struct.unpack("<2048H", raw[:n])
    bases = raw[n:n + BASE_BYTES]
    n += BASE_BYTES
    offsets = struct.unpack("<72H", raw[n:n + OFFSETS_BYTES])
    n += OFFSETS_BYTES
    deltas = raw[n:]
    header = (
        "/* User-provided 72 scenes; 160x72 1-bit, 4x4 VQ source fidelity. */\n"
        "#ifndef MI_LETTER_ANIM_H\n#define MI_LETTER_ANIM_H\n"
        "#include <stdint.h>\n"
        "#define MI_LETTER_W 160u\n"
        "#define MI_LETTER_H 72u\n"
        "#define MI_LETTER_FRAMES 72u\n"
        "#define MI_LETTER_MAP_SIZE 720u\n"
        "#define MI_LETTER_CLIP_FRAMES 9u\n"
        + word_array("mi_letter_books", books)
        + byte_array("mi_letter_bases", bases)
        + word_array("mi_letter_delta_offsets", offsets)
        + byte_array("mi_letter_deltas", deltas)
        + "#endif\n"
    )
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(header, encoding="ascii")
    print("PASS: 72 original-subject scenes, 160x72 pixels, " +
          "yellow/black 1-bit, original silhouette fidelity, " +
          f"VQ + delta {len(raw)}B, SHA256 {RAW_SHA256}")


if __name__ == "__main__":
    main()
