#!/usr/bin/env python3
"""After build, prove package, app and loader expose the same new identity.

Do not rely on FWSC filename or FELUCCA_VERSION alone: M-UPGRADE compares
the numeric FM-1_9xx identity carried by the 20 marker bytes and by
the OTA 0x11 device handshake.
"""
import argparse
import re
from pathlib import Path

BLOCKS = 20
BLOCK = 0x30
KEEP = 0x2F


def package_product(raw: bytes) -> str:
    if len(raw) < BLOCKS * BLOCK:
        raise RuntimeError("FWSC too short to carry its update identity")
    text = "".join(
        chr((raw[i * BLOCK + KEEP] - i - 1) & 255)
        for i in range(BLOCKS)
        if raw[i * BLOCK + KEEP] != 0x7D
    )
    if not re.fullmatch(r"FM-1_9\d\d", text):
        raise RuntimeError(f"Invalid updater package identity: {text!r}")
    return text


def verify(record: Path, package: Path, app: Path, loader: Path) -> str:
    lines = record.read_text(encoding="ascii").splitlines()
    entries = dict(line.split("=", 1) for line in lines)
    identity = entries["FWSC_PRODUCT"]
    build_num = int(entries["CIRCLE_BUILD_NUM"])
    if not 244 <= build_num <= 316:
        raise RuntimeError("CircleCI job must be 244..316; actual FM-1_926 was job #243")
    if identity != f"FM-1_{926 + build_num - 243}":
        raise RuntimeError("Build number does not match the incremented identity")
    if int(identity.rsplit("_", 1)[1]) <= 926:
        raise RuntimeError("Previous FM-1_926 updater identity was reused")
    actual = package_product(package.read_bytes())
    if actual != identity:
        raise RuntimeError(f"FWSC identity {actual} differs from expected {identity}")
    app_data, loader_data = app.read_bytes(), loader.read_bytes()
    if identity.encode("ascii") not in app_data:
        raise RuntimeError(f"App OTA handshake missing {identity}")
    if ("ota-" + identity).encode("ascii") not in loader_data:
        raise RuntimeError(f"Update loader OTA handshake missing ota-{identity}")
    print(f"PASS: FWSC package/app/loader all expose {identity}; "
          f"previous baseline FM-1_926, CircleCI build #{build_num}")
    return identity


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("record", type=Path)
    parser.add_argument("package", type=Path)
    parser.add_argument("app", type=Path)
    parser.add_argument("loader", type=Path)
    args = parser.parse_args()
    verify(args.record, args.package, args.app, args.loader)
