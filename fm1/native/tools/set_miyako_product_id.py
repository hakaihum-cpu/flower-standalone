#!/usr/bin/env python3
"""Derive one FM-1 updater identity AND visible app version from CircleCI.

Pinned fm1-base hardcodes suffix 00. Keep the required 3-digit M-UPGRADE
identity while deriving its trailing digits from the real pipeline number.
The baseline observed for CircleCI #288 is FM-1_938; #289 becomes 939.
Fail before compiling when the three-digit space is exhausted. Never wrap
to an older version, and never patch completed FWSC bytes afterward.
"""
from pathlib import Path
import argparse
import os
import re

OLD = '    PRODUCT = prefix + "00"'
APP_OLD = '#define APP_VERSION "UNVERSIONED"'
PIPELINE_OFFSET = 650


def product_for_pipeline(number: int) -> str:
    value = number + PIPELINE_OFFSET
    if not 939 <= value <= 999:
        raise ValueError(
            f"CircleCI pipeline #{number}: three-digit FM-1 updater version "
            f"{value} outside strict increasing 939..999 range; do not wrap")
    return f"FM-1_{value}"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True, type=Path)
    ap.add_argument("--pipeline-number", type=int, default=None)
    ap.add_argument("--product-file", type=Path, default=Path("/tmp/fm1_product_id.txt"))
    a = ap.parse_args()
    raw = (str(a.pipeline_number) if a.pipeline_number is not None
           else os.environ.get("CIRCLE_PIPELINE_NUMBER", ""))
    if not raw.isdecimal():
        raise SystemExit("FAIL: CircleCI pipeline number is required for version identity")
    try:
        product = product_for_pipeline(int(raw))
    except ValueError as exc:
        raise SystemExit(f"FAIL: {exc}") from exc
    base = a.base.resolve(strict=True)
    builder = base / "tools/build.py"
    manifest = base / "miyako/fm1.toml"
    header = base / "miyako/app.h"
    src = builder.read_text(encoding="utf-8")
    conf = manifest.read_text(encoding="utf-8")
    app = header.read_text(encoding="utf-8")
    if src.count(OLD) != 1 or "prefix + \"%02d%02d%02d\"" not in src:
        raise SystemExit("FAIL: unexpected pinned fm1-base PRODUCT construction")
    if len(re.findall(r'^id_prefix = "FM-1_9"$',
                      conf, flags=re.MULTILINE)) != 1:
        raise SystemExit("FAIL: MIYAKO prefix must stay FM-1_9")
    if app.count(APP_OLD) != 1:
        raise SystemExit("FAIL: app.h visible version sentinel changed")
    suffix = product[len("FM-1_9"):]
    new = f'    PRODUCT = prefix + "{suffix}"  # {product} CircleCI pipeline {raw}'
    builder.write_text(src.replace(OLD, new), encoding="utf-8")
    header.write_text(app.replace(APP_OLD, f'#define APP_VERSION "{product}"'), encoding="utf-8")
    if builder.read_text(encoding="utf-8").count(new) != 1:
        raise SystemExit("FAIL: builder identity write did not persist")
    if f'#define APP_VERSION "{product}"' not in header.read_text(encoding="utf-8"):
        raise SystemExit("FAIL: firmware visible version write did not persist")
    a.product_file.write_text(product + "\n", encoding="ascii")
    print(f"PASS: CircleCI pipeline #{raw}: FWSC + FELUCCA_ID + visible APP_VERSION = {product}")


if __name__ == "__main__":
    main()
