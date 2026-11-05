#!/usr/bin/env python3
"""Generate a unique monotonic M-VAVE FM-1 FWSC updater identity.

The M-UPGRADE 'CurrentVersion' is NOT the label on the LCD. Upstream
tools/build.py embeds PRODUCT in all three places:
  - the app's FELUCCA_ID, emitted by the OTA identity handshake
  - the loader's 'ota-' + PRODUCT identity
  - the 20 FWSC identity marker bytes
Changing only FELUCCA_VERSION or the output filename cannot fix equality.
The previous MIYAKO P1 firmware reused upstream FM-1_926.

CircleCI's CIRCLE_BUILD_NUM is monotonic for this job; derive the identity
from it and fail on overflow instead of silently reusing an old number.
No version is changed at import time or during static checks.
"""
import argparse
import re
from pathlib import Path

OLD_PRODUCT = 'PRODUCT = "FM-1_926"'
OLD_VERSION = "VERSION = None"
PREVIOUS_ID = 926
MAX_COMPAT_ID = 999  # current upstream ota loader regex: ota-FM-1_9dd


def run(build_py: Path, build_number: int, record: Path) -> str:
    if not 1 <= build_number <= MAX_COMPAT_ID - PREVIOUS_ID:
        raise RuntimeError(
            f"CircleCI job build number {build_number} is outside 1..73. "
            "STOP rather than reuse an updater identity or cross the "
            "FM-1_9xx recovery-loader compatibility range."
        )
    identity_number = PREVIOUS_ID + build_number
    product = f"FM-1_{identity_number}"
    src = build_py.read_text(encoding="utf-8")
    if src.count(OLD_PRODUCT) != 1 or src.count(OLD_VERSION) != 1:
        raise RuntimeError("Pinned KARMA build.py identity anchors changed; refuse unsafe package")
    patched = src.replace(
        OLD_PRODUCT, f'PRODUCT = "{product}"', 1
    ).replace(
        OLD_VERSION, f'VERSION = "MIYAKO FM1 {identity_number}"', 1
    )
    if patched.count(f'PRODUCT = "{product}"') != 1:
        raise RuntimeError("Could not set unique package identity")
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise RuntimeError("Incompatible M-UPGRADE loader identity")
    build_py.write_text(patched, encoding="utf-8")
    record.parent.mkdir(parents=True, exist_ok=True)
    record.write_text(
        f"FWSC_PRODUCT={product}\n"
        f"FELUCCA_VERSION=MIYAKO FM1 {identity_number}\n"
        f"CIRCLE_BUILD_NUM={build_number}\n"
        "PREVIOUS_BASE=FM-1_926\n",
        encoding="ascii",
    )
    print(f"PASS: unique M-UPGRADE identity {product}, previous P1 was FM-1_926; "
          f"build number {build_number}")
    return product


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("build_py", type=Path)
    p.add_argument("record", type=Path)
    p.add_argument("--build-number", required=True, type=int)
    a = p.parse_args()
    run(a.build_py, a.build_number, a.record)


if __name__ == "__main__":
    main()
