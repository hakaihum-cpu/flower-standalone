#!/usr/bin/env python3
"""Host-only updater regression, no target build or device write."""
import tempfile
from pathlib import Path
from prepare_fwsc_version import run
from verify_fwsc_version import package_product, verify

def fwsc_mock(product: str) -> bytes:
    raw = bytearray(b"\xFF" * 2048)
    for i in range(20):
        raw[i * 0x30 + 0x2F] = (ord(product[i]) + i + 1) & 255 if i < len(product) else 0x7D
    return bytes(raw)

with tempfile.TemporaryDirectory() as d:
    home = Path(d)
    for job, number in ((244, 927), (245, 928), (316, 999)):
        build = home / "build.py"
        info = home / "version.txt"
        out = home / "fwsc.fwsc"
        app, loader = home / "app.bin", home / "loader.bin"
        build.write_text('PRODUCT = "FM-1_926"\nVERSION = None\n', encoding="utf-8")
        product = f"FM-1_{number}"
        assert run(build, job, info) == product
        assert f'PRODUCT = "{product}"' in build.read_text()
        out.write_bytes(fwsc_mock(product))
        app.write_bytes(b"APP " + product.encode("ascii"))
        loader.write_bytes(b"LOADER ota-" + product.encode("ascii"))
        assert package_product(out.read_bytes()) == product
        assert verify(info, out, app, loader) == product
    for job in (243, 317):
        try:
            run(build, job, home / "bad.txt")
        except RuntimeError:
            continue
        raise AssertionError(f"Stale or out-of-range build {job} accepted")
print("PASS: CircleCI #244=927, #245=928, #316=999; reject #243 and #317")
