#!/usr/bin/env python3
"""Audit only native MIYAKO source closure, and explicitly forbid old framework linkage.
This check does not compile or authorize creation of firmware.
"""
from __future__ import annotations
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "fm1" / "native"
MANIFEST = NATIVE / "SOURCE_ALLOWLIST.txt"
FORBIDDEN_PATH_BITS = ("/karma/", "felucca", "cloud.c", "eng_prism", "eng_elem", "eng_phys")
FORBIDDEN_SYMBOLS = ("KARMA", "FELUCCA", "ui_input(", "cloud_process(", "ENG_PRISM", "ENG_ELEM")
GENERATED_MEDIA_HEADERS = {"mi_eureka_vq_frames.h", "mi_effects_vq_frames.h"}
source_paths = []
for raw in MANIFEST.read_text(encoding="utf-8").splitlines():
    name = raw.strip()
    if not name or name.startswith("#"):
        continue
    if Path(name).is_absolute() or ".." in Path(name).parts:
        raise SystemExit(f"FAIL: unsafe path: {name}")
    if not name.startswith("fm1/native/") or any(bit in name.lower() for bit in FORBIDDEN_PATH_BITS):
        raise SystemExit(f"FAIL: nonnative source in allowlist: {name}")
    file = ROOT / name
    if not file.is_file() or file.suffix != ".c":
        raise SystemExit(f"FAIL: missing native C source: {name}")
    source_paths.append(file)
if not source_paths:
    raise SystemExit("FAIL: empty standalone source manifest")
for file in source_paths:
    body = file.read_text(encoding="utf-8")
    for symbol in FORBIDDEN_SYMBOLS:
        if symbol in body:
            raise SystemExit(f"FAIL: forbidden implementation token {symbol} in {file}")
    for header in re.findall(r'#\s*include\s*"([^"]+)"', body):
        relative = (file.parent / header).resolve()
        if NATIVE.resolve() not in relative.parents:
            raise SystemExit(f"FAIL: nonnative local include {header} in {file}")
        if not relative.is_file() and header not in GENERATED_MEDIA_HEADERS:
            raise SystemExit(f"FAIL: missing native header {header} in {file}")
print(f"PASS: {len(source_paths)} native MIYAKO source unit(s), no external framework units")
print("NOT READY: board HAL, MIYAKO DSP/FX, video, flash/boot and FM-1 linking are still missing")
