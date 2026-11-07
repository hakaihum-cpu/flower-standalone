#!/usr/bin/env python3
"""Pin MIYAKO's installer-visible FM-1 package identity in fm1-base.

The pinned fm1-base development build unconditionally appends "00" to
id_prefix. The historical MIYAKO config "FM-1_91" thus emitted
"FM-1_9100" (four digits), while the current device says "FM-1_926".
M-UPGRADE has not established support for this custom four-digit scheme.
MIYAKO trial uses "FM-1_934": a distinct three-digit firmware identity.

Patch the ONE existing PRODUCT assignment BEFORE host/target compilation.
This ensures package marker bytes, FELUCCA_ID built into the program, and
updater handshake all use the SAME value. No post-link marker forgery.
Aborts if upstream code is different from our reviewed pin.
"""
from pathlib import Path
import argparse
import re

PIN = "e07ebca2dda5fbd3bb40287d1f6083923f9ae999"
TARGET = "FM-1_934"
OLD = '    PRODUCT = prefix + "00"'
NEW = '    PRODUCT = prefix + "34"  # MIYAKO trial FM-1_934, keep in sync with app ID'

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True, type=Path)
    args = ap.parse_args()
    base = args.base.resolve(strict=True)
    code = base / "tools/build.py"
    s = code.read_text(encoding="utf-8")
    if s.count(OLD) != 1 or "    PRODUCT = prefix + " not in s:
        raise SystemExit("FAIL: pinned FM-1 build.py identity construction changed")
    manifest = base / "miyako/fm1.toml"
    doc = manifest.read_text(encoding="utf-8")
    if len(re.findall(r'^id_prefix = "FM-1_9"$',
                      doc, flags=re.MULTILINE)) != 1:
        raise SystemExit("FAIL: app config must use id_prefix FM-1_9")
    if "PRODUCT = prefix + \"31\"" in s:
        raise SystemExit("FAIL: patch was already applied")
    code.write_text(s.replace(OLD, NEW), encoding="utf-8")
    after = code.read_text(encoding="utf-8")
    assert after.count(NEW) == 1 and OLD not in after
    assert "prefix + \"%02d%02d%02d\"" in after, (
        "upstream release identity logic unexpectedly changed")
    print(f"PASS: pinned fm1-base MIYAKO dev identity {TARGET}; "
          "firmware and FWSC generated with the same ID")
    print("Compatibility with M-UPGRADE is NOT confirmed until the "
          "resulting FWSC is selected on the user's PC.")

if __name__ == "__main__":
    main()
