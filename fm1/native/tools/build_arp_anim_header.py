#!/usr/bin/env python3
"""Emit freestanding 2-colour ARP frames from two exact user-supplied montages.

SOURCE: second sheet (head opens) first, 9x9; first sheet (carousel)
second, its ninth complete column/row being the last usable tile (the
physical file ends in a truncated tenth tile). Do not synthesize missing
artwork. 162 complete distinct frames, 48x48, black and #FFD10C only.
Packed 4x4 1-bit VQ / per-frame deltas: no target zlib, no dynamic memory.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
import struct
from pathlib import Path

SOURCE = Path(__file__).with_name("mi_arp_source_yellow_black.b64")
RAW_SHA256 = "5a36c5333b15bf6269212e13628e2fb4bce324a7cf09c62bda88a0ce97e32e87"
FRAME_COUNT = 162
FRAMES_INTRO = 81
W = H = 48
BLOCKS = 12 * 12
MASK_BYTES = (BLOCKS + 7) // 8
HEADER_BYTES = 256 * 2 + FRAME_COUNT * 2
PACK_BYTES = 10552

# Facial detail comes from the SAME user-provided two montages at 72px.
# Keep the original 48px outline, add a 36x28 exact-source facial inset.
# This avoids another ~23KB of full high-resolution images in the FWSC.
FACE_SOURCES = (
    Path(__file__).with_name("mi_arp_face_detail72_0.b64"),
    Path(__file__).with_name("mi_arp_face_detail72_1.b64"),
)
FACE_SHA256 = "a6ef0422a353ac536205836f689d26c3861fe93afe33a8c12da9812221279a19"
FACE_PACK_BYTES = 5115
FACE_W = 36
FACE_H = 28
FACE_X = 20
FACE_Y = 40
FACE_BLOCKS = (FACE_W // 4) * (FACE_H // 4)
FACE_MASK_BYTES = (FACE_BLOCKS + 7) // 8


def arr(name: str, width: int, vals: list[int]) -> str:
    if width == 8:
        kind = "uint8_t"
        body = [f"0x{v:02x}" for v in vals]
        span = 24
    else:
        kind = "uint16_t"
        body = [f"0x{v:04x}" for v in vals]
        span = 12
    rows = ["    " + ",".join(body[i:i + span]) + "," for i in range(0, len(body), span)]
    return f"static const {kind} {name}[{len(body)}u] = {{\n" + "\n".join(rows) + "\n};\n"


def read_and_verify() -> tuple[list[int], list[int], bytes]:
    b64 = SOURCE.read_text(encoding="ascii").strip()
    raw = base64.b64decode(b64, validate=True)
    if len(raw) != PACK_BYTES or hashlib.sha256(raw).hexdigest() != RAW_SHA256:
        raise SystemExit("FAIL: ARP source pack bytes/SHA256, source must be original")
    book = list(struct.unpack_from("<256H", raw, 0))
    offsets = list(struct.unpack_from(f"<{FRAME_COUNT}H", raw, 512))
    delta = raw[HEADER_BYTES:]
    if offsets[0] != 0 or not all(x < len(delta) for x in offsets):
        raise SystemExit("FAIL: ARP offsets outside data")
    decoded = [0] * BLOCKS
    for frame in range(FRAME_COUNT):
        at = offsets[frame]
        if frame in (0, FRAMES_INTRO):
            if at + BLOCKS > len(delta):
                raise SystemExit("FAIL: incomplete ARP source keyframe")
            decoded[:] = delta[at:at + BLOCKS]
            end = at + BLOCKS
        else:
            if at + MASK_BYTES > len(delta):
                raise SystemExit("FAIL: incomplete ARP delta mask")
            mask = delta[at:at + MASK_BYTES]
            end = at + MASK_BYTES
            for j in range(BLOCKS):
                if mask[j // 8] & (0x80 >> (j % 8)):
                    if end >= len(delta):
                        raise SystemExit("FAIL: incomplete ARP delta pixels")
                    decoded[j] = delta[end]
                    end += 1
        if frame + 1 < FRAME_COUNT and end != offsets[frame + 1]:
            raise SystemExit(f"FAIL: ARP delta frame {frame} overlaps/gaps")
        if frame + 1 == FRAME_COUNT and end != len(delta):
            raise SystemExit("FAIL: ARP delta trailing bytes")
        if not (any(book[k] for k in decoded) and len(decoded) == BLOCKS):
            raise SystemExit(f"FAIL: ARP frame {frame} black/invalid")
    return book, offsets, delta



def read_face_pack() -> tuple[list[int], list[int], bytes]:
    """Validate all 162 per-frame patches; never accept synthetic substitutes."""
    try:
        # Two small repository chunks concatenate into a single exact
        # 5115-byte original-source pack. Neither chunk is decoded alone.
        packed = "".join(p.read_text(encoding="ascii").strip()
                         for p in FACE_SOURCES)
        data = base64.b64decode(packed, validate=True)
    except (OSError, ValueError) as exc:
        raise SystemExit("FAIL: missing/invalid original-source ARP facial detail") from exc
    if len(data) != FACE_PACK_BYTES or hashlib.sha256(data).hexdigest() != FACE_SHA256:
        raise SystemExit("FAIL: original-source ARP facial detail SHA256/size mismatch")
    books = list(struct.unpack_from("<256H", data, 0))
    offsets = list(struct.unpack_from(f"<{FRAME_COUNT}H", data, 512))
    deltas = data[512 + FRAME_COUNT * 2:]
    if offsets[0] != 0 or not all(0 <= k < len(deltas) for k in offsets):
        raise SystemExit("FAIL: ARP facial detail offsets")
    current = [0] * FACE_BLOCKS
    for frame, offset in enumerate(offsets):
        if frame in (0, FRAMES_INTRO):
            if offset + FACE_BLOCKS > len(deltas):
                raise SystemExit("FAIL: truncated ARP facial keyframe")
            current[:] = deltas[offset:offset + FACE_BLOCKS]
            end = offset + FACE_BLOCKS
        else:
            if offset + FACE_MASK_BYTES > len(deltas):
                raise SystemExit("FAIL: truncated ARP facial delta mask")
            mask = deltas[offset:offset + FACE_MASK_BYTES]
            end = offset + FACE_MASK_BYTES
            for j in range(FACE_BLOCKS):
                if mask[j // 8] & (0x80 >> (j % 8)):
                    if end >= len(deltas):
                        raise SystemExit("FAIL: truncated ARP facial delta pixel")
                    current[j] = deltas[end]
                    end += 1
        if frame + 1 < FRAME_COUNT and end != offsets[frame + 1]:
            raise SystemExit(f"FAIL: ARP facial frame {frame} overlaps/gaps")
        if frame + 1 == FRAME_COUNT and end != len(deltas):
            raise SystemExit("FAIL: ARP facial data trailing bytes")
        if not any(books[k] for k in current):
            raise SystemExit(f"FAIL: ARP facial frame {frame} has no visible pixels")
    return books, offsets, deltas

def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    book, offsets, data = read_and_verify()
    face_books, face_offsets, face_data = read_face_pack()
    code = "".join([
        "/* Exact original user-supplied ARP tiles, 2-colour VQ, 162 frames. */\n",
        "#ifndef MI_ARP_MEDIA_H\n#define MI_ARP_MEDIA_H\n#include <stdint.h>\n",
        "#define MI_ARP_FRAME_W 48u\n#define MI_ARP_FRAME_H 48u\n",
        "#define MI_ARP_FRAME_COUNT 162u\n#define MI_ARP_INTRO_COUNT 81u\n",
        "#define MI_ARP_MAP_BYTES 144u\n#define MI_ARP_MASK_BYTES 18u\n",
        arr("mi_arp_books", 16, book),
        arr("mi_arp_offsets", 16, offsets),
        arr("mi_arp_deltas", 8, list(data)),
        "/* Facial source detail: overlay only, NO new or generated imagery. */\n",
        f"#define MI_ARP_FACE_X {FACE_X}u\n#define MI_ARP_FACE_Y {FACE_Y}u\n",
        f"#define MI_ARP_FACE_W {FACE_W}u\n#define MI_ARP_FACE_H {FACE_H}u\n",
        f"#define MI_ARP_FACE_MAP_BYTES {FACE_BLOCKS}u\n",
        f"#define MI_ARP_FACE_MASK_BYTES {FACE_MASK_BYTES}u\n",
        arr("mi_arp_face_books", 16, face_books),
        arr("mi_arp_face_offsets", 16, face_offsets),
        arr("mi_arp_face_deltas", 8, list(face_data)),
        "#endif\n",
    ])
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(code, encoding="ascii")
    print(f"PASS: 162 complete ARP frames (81 opening + 81 rotation), "
          f"original 2-colour yellow/black, {PACK_BYTES} source bytes; "
          f"{FACE_PACK_BYTES} bytes of 72px-source facial detail")


if __name__ == "__main__":
    main()