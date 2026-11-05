#!/usr/bin/env python3
"""Offline pre-build test: M-UPGRADE numeric identity never stays the same.

No toolchain, actual device or update attempt required. Tests package marker
encoding, app/loader identities and boundaries of the supported 9xx range.
"""
import tempfile
from pathlib import Path

from prepare_fwsc_version import run
from verify_fwsc_version import package_product, verify


def mock_package(product: str) -> bytes:
    raw = bytearray(b"\xFF" * 2048)
    for i in range(20):
        if i < len(product):
            raw[i * 0x30 + 0x2F] = (ord(product[i]) + i + 1) & 255
        else:
            raw[i * 0x30 + 0x2F] = 0x7D
    return bytes(raw)


with tempfile.TemporaryDirectory() as folder:
    home = Path(folder)
    for n in (1, 2, 73):
        buildpy = home / "build.py"
        version = home / "version.txt"
        fwsc = home / "test.fwsc"
        app = home / "app.bin"
        loader = home / "loader.bin"
        buildpy.write_text('PRODUCT = "FM-1_926"\nVERSION = None\n', encoding="utf-8")
        expected = f"FM-1_{926 + n}"
        got = run(buildpy, n, version)
        assert got == expected
        assert f'PRODUCT = "{expected}"' in buildpy.read_text()
        assert f'VERSION = "MIYAKO FM1 {926 + n}"' in buildpy.read_text()
        fwsc.write_bytes(mock_package(expected))
        app.write_bytes(b"audio OTA " + expected.encode("ascii"))
        loader.write_bytes(b"usb ota-" + expected.encode("ascii"))
        assert package_product(fwsc.read_bytes()) == expected
        assert verify(version, fwsc, app, loader) == expected
    try:
        run(home / "build.py", 74, home / "version.txt")
    except RuntimeError:
        pass
    else:
        raise AssertionError("FWSC updater identity reused or exceeded 999")
print("PASS: unique FM-1_927, FM-1_928, FM-1_999; reject #74")
