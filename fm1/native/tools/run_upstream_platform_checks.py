#!/usr/bin/env python3
"""Run the ACTUAL pinned fm1-base MMIO/BSP source audit before media work.

This is not a text approximation of the platform's rules: it imports the exact
upstream build.py and runs mmio_check() against the same physical source tree
and app staging directory used for the real target link. Does not build or
flash, and does not bypass the audit with mmio_skip/vendored settings.
"""
from pathlib import Path
import argparse
import os
import sys

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--platform", type=Path, required=True)
    ap.add_argument("--app", type=Path, required=True)
    args = ap.parse_args()
    base = args.platform.resolve(strict=True)
    app = args.app.resolve(strict=True)
    if base not in app.parents:
        raise SystemExit(f"FAIL: app must be staged inside pinned platform tree: {app}")
    if not (base / "bsp/chip").is_dir():
        raise SystemExit("FAIL: FM-1 board SDK submodule is absent")
    if not (app / "fm1.toml").is_file():
        raise SystemExit("FAIL: fm1.toml is absent")
    sys.path.insert(0, str(base / "tools"))
    import build as fm1build

    assert fm1build.SRC.resolve() == base, "wrong upstream fm1-base revision"
    fm1build.APP = app
    fm1build.ROOT = Path(os.path.commonpath([base, app]))
    fm1build.CONF = fm1build.read_conf(app / "fm1.toml")
    # The only permitted skip is the empty list: do not disable platform policy
    for name in ("mmio_skip", "vendored"):
        if fm1build.CONF.get(name, []):
            raise SystemExit(f"FAIL: platform audit escape hatch forbidden: {name}")
    problems = fm1build.mmio_check()
    if problems:
        for p in problems:
            print("FAIL FM-1 platform source audit:", p, file=sys.stderr)
        raise SystemExit(f"FAIL: {len(problems)} real upstream FM-1 MMIO violation(s)")
    print("PASS: upstream fm1-base build.py mmio_check() on actual staged"
          " MIYAKO and platform/BSP code")

if __name__ == "__main__":
    main()
