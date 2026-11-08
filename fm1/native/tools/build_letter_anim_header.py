#!/usr/bin/env python3
"""Materialize the user's 72 paper-airplane frames; no AI-generated art.

8x9 uploaded montage -> 72 ordered scene-only frames -> 16-colour RGB565
palette -> 80x40 pixels -> lossy 4x4 VQ (256 codewords; 200 map bytes/frame).
The B64 file is the immutable user-derived pack. A full raw SHA-256 check
must succeed BEFORE emitting target header. No build-time internet or images.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
from pathlib import Path
import zlib

RAW_LENGTH = 32 + 256 * 8 + 72 * 200
RAW_SHA256 = "8e4a5b1f02d6dcf424e9ca28b7d1813e6837432ed23a56fd4f565400ad905e72"
PACK = Path(__file__).resolve().with_name("mi_letter_72frames.b64")


def read_asset() -> bytes:
    encoded = PACK.read_text(encoding="ascii").strip()
    try:
        compressed = base64.b64decode(encoded, validate=True)
        data = zlib.decompress(compressed)
    except (ValueError, zlib.error) as exc:
        raise SystemExit(f"FAIL: invalid approved MIYAKO letter animation: {exc}") from exc
    if len(data) != RAW_LENGTH or hashlib.sha256(data).hexdigest() != RAW_SHA256:
        raise SystemExit("FAIL: MIYAKO letter data length / SHA-256 mismatch")
    return data


def byte_array(name: str, data: bytes) -> str:
    lines = []
    for pos in range(0, len(data), 24):
        lines.append("    " + ",".join(f"0x{x:02x}" for x in data[pos:pos + 24]) + ",")
    return (f"static const uint8_t {name}[{len(data)}u] = {{\n" +
            "\n".join(lines) + "\n};\n")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, type=Path)
    a = ap.parse_args()
    raw = read_asset()
    palette = [int.from_bytes(raw[i:i + 2], "little") for i in range(0, 32, 2)]
    pal_text = ",".join(f"0x{x:04x}" for x in palette)
    out = (
        "/* Generated from the user's 72-frame source image. Never hand-edit. */\n"
        "#ifndef MI_LETTER_ANIM_H\n#define MI_LETTER_ANIM_H\n"
        "#include <stdint.h>\n"
        "#define MI_LETTER_W 80u\n#define MI_LETTER_H 40u\n"
        "#define MI_LETTER_FRAMES 72u\n"
        f"static const uint16_t mi_letter_palette[16] = {{{pal_text}}};\n"
        + byte_array("mi_letter_book", raw[32:32 + 2048])
        + byte_array("mi_letter_maps", raw[32 + 2048:])
        + "#endif\n"
    )
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(out, encoding="ascii")
    print("PASS: user source 72/72 frames, 16 RGB565 colors, " +
          f"VQ {len(raw)} bytes, SHA256 {RAW_SHA256}")


if __name__ == "__main__":
    main()
