#!/usr/bin/env python3
"""Preserve the approved MIYAKO VQ source in the fm1-base resource region.

Only storage location changes: the existing 16-colour RGB565 palette, 8x4
256-word codebook and per-frame maps are copied byte-for-byte out of the
existing C arrays. No frames are generated, transcoded or omitted.
fm1-base's own resource facility wraps this payload with its 32-byte CRC
header and writes the separate NAME-res.bin after the FWSC update.
"""
import argparse
import hashlib
import re
from pathlib import Path

EXPECTED_SHA256 = "429f2aa8f561079481c3b60562ef3f88e2f5abd044a446eca1b1ebe1c6bc11ee"
EXPECTED_BYTES = 92928
ADDITIONAL_BYTES = 26592
TOTAL_BYTES = EXPECTED_BYTES + ADDITIONAL_BYTES
ADDITIONAL_SHA256 = "7cf48b59c60028829c0d7db6f49a2e52750d3d49b7da33943cba2599d18344ca"
MAX_RESOURCE_SECTORS = 69
SECTOR_BYTES = 4096
RESOURCE_HEADER_BYTES = 32


def array(source: str, name: str, expected: int, unit: int) -> bytes:
    match = re.search(
        r"static\s+const\s+uint(?:8|16)_t\s+" + re.escape(name) +
        r"\s*\[[^;=]+\]\s*=\s*\{(.*?)\}\s*;",
        source, re.S)
    if not match:
        raise ValueError(f"original array not found: {name}")
    numbers = [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]+|\b\d+", match.group(1))]
    if len(numbers) != expected:
        raise ValueError(f"{name}: {len(numbers)} elements instead of {expected}")
    if any(x < 0 or x >= (1 << (unit * 8)) for x in numbers):
        raise ValueError(f"{name}: palette/tile byte out of range")
    return b"".join(x.to_bytes(unit, "little") for x in numbers)


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("source_header", type=Path)
    p.add_argument("additional_header", type=Path)
    p.add_argument("resource_payload", type=Path)
    args = p.parse_args()
    source = args.source_header.read_text(encoding="ascii")
    palettes = array(source, "mi_photo_vq_palette", 14 * 16, 2)
    books = array(source, "mi_photo_vq_tilebook", 14 * 256 * 16, 1)
    maps = array(source, "mi_photo_vq_maps", 488 * 72, 1)
    result = palettes + books + maps
    digest = hashlib.sha256(result).hexdigest()
    if len(result) != EXPECTED_BYTES or digest != EXPECTED_SHA256:
        raise SystemExit("FAIL: existing MIYAKO image payload changed; refuse resource")
    # The additional two approved contact sheets use exactly the same
    # 16-colour RGB565 palette/8x4 tilebook/selector map representation.
    additional = args.additional_header.read_text(encoding="ascii")
    new_palettes = array(additional, "mi_new_vq_palette", 4 * 16, 2)
    new_books = array(additional, "mi_new_vq_tilebook", 4 * 256 * 16, 1)
    new_maps = array(additional, "mi_new_vq_maps", 140 * 72, 1)
    extra = new_palettes + new_books + new_maps
    if len(extra) != ADDITIONAL_BYTES or hashlib.sha256(extra).hexdigest() != ADDITIONAL_SHA256:
        raise SystemExit("FAIL: any of 140 additional approved frames were changed")
    result += extra
    if len(result) != TOTAL_BYTES:
        raise SystemExit("FAIL: resource payload size differs from expected 628 frames")
    # The existing app store object begins at 0xDC000, the resource region
    # ends immediately before it: [0x97000, 0xDC000), 69 sectors.
    if RESOURCE_HEADER_BYTES + len(result) > MAX_RESOURCE_SECTORS * SECTOR_BYTES:
        raise SystemExit("FAIL: MIYAKO resources overlap saved user settings")
    args.resource_payload.parent.mkdir(parents=True, exist_ok=True)
    args.resource_payload.write_bytes(result)
    print(f"PASS: 488 preserved + 140 newly supplied photo tiles -> FM1 standard resource payload "
          f"{len(result)} B, original SHA-256={digest}, new SHA-256={ADDITIONAL_SHA256}")


if __name__ == "__main__":
    main()
