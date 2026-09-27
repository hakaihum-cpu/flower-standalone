#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re
import sys

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Builds/Android/app/build.gradle")
if not path.is_file():
    raise SystemExit(f"[FAIL] generated Gradle file missing: {path}")

text = path.read_text(encoding="utf-8")
pattern = re.compile(r'abiFilters\("arm64-v8a"\)')
matches = list(pattern.finditer(text))

if not matches:
    if 'abiFilters("x86_64")' in text:
        print("[PASS] Android smoke ABI already set to x86_64")
        raise SystemExit(0)
    raise SystemExit("[FAIL] expected arm64-v8a abiFilters entry was not found")

updated = pattern.sub('abiFilters("x86_64")', text)

if 'abiFilters("arm64-v8a")' in updated:
    raise SystemExit("[FAIL] arm64-v8a abiFilters remained after smoke ABI patch")

if updated.count('abiFilters("x86_64")') < len(matches):
    raise SystemExit("[FAIL] x86_64 abiFilters patch count is inconsistent")

path.write_text(updated, encoding="utf-8")
print(f"[PASS] Replaced {len(matches)} generated abiFilters entries with x86_64 for emulator smoke")
