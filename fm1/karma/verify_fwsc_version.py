#!/usr/bin/env python3
"""Reject stale/mismatched firmware identities before publishing an FWSC."""
import argparse
import re
from pathlib import Path

BLOCKS, BLOCK, MARK = 20, 0x30, 0x2F
BASE_JOB, BASE_ID = 243, 926

def package_product(raw: bytes) -> str:
    if len(raw) < BLOCKS * BLOCK:
        raise RuntimeError("FWSC header missing")
    value = "".join(
        chr((raw[i * BLOCK + MARK] - i - 1) & 255)
        for i in range(BLOCKS)
        if raw[i * BLOCK + MARK] != 0x7D
    )
    if re.fullmatch(r"FM-1_9\d\d", value) is None:
        raise RuntimeError(f"bad updater identity {value!r}")
    return value

def verify(record: Path, package: Path, app: Path, loader: Path) -> str:
    settings = dict(line.split("=", 1) for line in record.read_text(encoding="ascii").splitlines())
    job = int(settings["CIRCLE_BUILD_NUM"])
    expected = f"FM-1_{BASE_ID + job - BASE_JOB}"
    if job <= BASE_JOB or not 927 <= BASE_ID + job - BASE_JOB <= 999:
        raise RuntimeError(f"invalid FWSC job number {job}: old 926 is forbidden")
    if settings.get("BASE_JOB") != "243" or settings.get("FWSC_PRODUCT") != expected:
        raise RuntimeError("FWSC version record disagrees with CircleCI job")
    actual = package_product(package.read_bytes())
    if actual != expected:
        raise RuntimeError(f"FWSC package identity {actual} != {expected}")
    if expected.encode("ascii") not in app.read_bytes():
        raise RuntimeError(f"app OTA identity != {expected}")
    if ("ota-" + expected).encode("ascii") not in loader.read_bytes():
        raise RuntimeError(f"loader OTA identity != ota-{expected}")
    print(f"PASS FWSC/app/loader updater identities: {expected} (job {job})")
    return expected

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("record", type=Path)
    p.add_argument("package", type=Path)
    p.add_argument("app", type=Path)
    p.add_argument("loader", type=Path)
    a = p.parse_args()
    verify(a.record, a.package, a.app, a.loader)
