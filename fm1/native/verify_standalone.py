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
# The JieLi pi32v2 target compiler has <stdint.h> but no hosted
# <string.h>/<stdio.h>/<stdlib.h>/<math.h> libc headers. This error previously
# surfaced only after the expensive 144-frame VQ stage; fail it at step 1.
allowed_target_headers = {"stdint.h", "stddef.h", "stdbool.h", "limits.h"}
target_files = source_paths + sorted(NATIVE.glob("*.h"))
for file in target_files:
    body = file.read_text(encoding="utf-8")
    for header in re.findall(r'#\\s*include\\s*<([^>]+)>', body):
        if header not in allowed_target_headers:
            raise SystemExit(
                f"FAIL: {file.relative_to(ROOT)} imports unavailable JieLi "
                f"freestanding header <{header}>; use mi_memory.h or platform APIs"
            )
if not (NATIVE / "mi_memory.h").is_file():
    raise SystemExit("FAIL: native freestanding MIYAKO zero/compare API missing")
print("PASS: no hosted libc includes in target firmware units or local headers")
print(f"PASS: {len(source_paths)} native MIYAKO source unit(s), no external framework units")
print("SOURCE-ONLY PASS: platform provided by pinned fm1-base; complete MIYAKO DSP, target link and physical tests require separate validation")
