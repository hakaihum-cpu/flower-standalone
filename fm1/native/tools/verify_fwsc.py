#!/usr/bin/env python3
"""Fail closed on an incorrectly packaged MIYAKO development firmware.

Uses the independent fm1-base installer's package parser (without touching a
MIDI device). Never treats a successful package parse as hardware validation.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import sys

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("package", type=Path)
    ap.add_argument("application", type=Path)
    ap.add_argument("loader", type=Path)
    ap.add_argument("--base", type=Path, required=True)
    ap.add_argument("--product", required=True)
    a = ap.parse_args()
    sys.path.insert(0, str(a.base / "tools"))
    import fm1_install  # pylint: disable=import-outside-toplevel

    raw = a.package.read_bytes()
    app = a.application.read_bytes()
    loader = a.loader.read_bytes()
    assert app and len(app) <= 0x8DFBC, (
        f"application XIP exceeds 581,564 B: {len(app)}")
    assert 256 <= len(loader) < 0x20000, "OTA loader is missing or invalid"
    assert len(raw) > 0x4000, "FWSC has no flash image"
    product, logical, digest = fm1_install.load_package(str(a.package), force=False)
    assert product == a.product, (
        f"wrong device identity: {product!r}; expected {a.product!r}")
    # M-UPGRADE rejected the old custom FM-1_9100 trial identity.
    # Check the three-digit identifier actually embedded in the package.
    assert product.startswith("FM-1_") and len(product.split("_", 1)[1]) == 3, (
        f"unexpected package model/version markers: {product!r}")
    assert digest == hashlib.sha256(raw).digest(), "package hash mismatch"
    assert b"FELUCCA-LOADER-1" in raw, "missing update-loader marker"
    # Every byte must round-trip the same identity-marker removal used
    # by the hardware-compatible FM-1 installer.
    assert len(logical) == len(raw)-20, "corrupt FWSC marker layout"
    assert fm1_install.logical_image(raw) == logical, "FWSC decode mismatch"
    print(f"PASS {a.package.name}: {len(raw)}B, product {product}, "
          f"app {len(app)}/{0x8DFBC}B, OTA {len(loader)}B, "
          f"SHA256 {digest.hex()}")
    print("NOT AN FM-1 HARDWARE TEST; DREAMY/FULL MULTI-FX NOT CERTIFIED")

if __name__ == "__main__":
    main()
