#!/usr/bin/env python3
from __future__ import annotations

import base64
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESOURCE_DIR = ROOT / "Resources"
PARTS = [
    RESOURCE_DIR / "xy_sheet_01.b64.part00",
    RESOURCE_DIR / "xy_sheet_01.b64.part01",
    RESOURCE_DIR / "xy_sheet_01.b64.part02",
    RESOURCE_DIR / "xy_sheet_01.b64.part03",
    RESOURCE_DIR / "xy_sheet_01.b64.part04",
    RESOURCE_DIR / "xy_sheet_01.b64.part05",
    RESOURCE_DIR / "xy_sheet_01.b64.part06",
]
OUTPUT = RESOURCE_DIR / "flower_xy_sheet_01.jpg"
EXPECTED_SHA256 = "959a90c4e04f158c54dc5cdb7d6cc16efea3db29b0a45d1f45c6cab140921852"

for part in PARTS:
    if not part.is_file():
        raise SystemExit(f"[FAIL] missing XY visual asset part: {part.relative_to(ROOT)}")

encoded = "".join(part.read_text(encoding="ascii").strip() for part in PARTS)
try:
    decoded = base64.b64decode(encoded, validate=True)
except Exception as exc:
    raise SystemExit(f"[FAIL] invalid XY visual base64: {exc}") from exc

actual = hashlib.sha256(decoded).hexdigest()
if actual != EXPECTED_SHA256:
    raise SystemExit(
        f"[FAIL] XY visual SHA256 mismatch: {actual} != {EXPECTED_SHA256}"
    )

OUTPUT.write_bytes(decoded)
print(f"[PASS] materialized {OUTPUT.relative_to(ROOT)} bytes={len(decoded)} sha256={actual}")
