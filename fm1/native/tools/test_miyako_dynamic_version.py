#!/usr/bin/env python3
"""Regression for per-pipeline FM-1 release identity and visible version."""
from __future__ import annotations
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SCRIPT = ROOT / "set_miyako_product_id.py"

with tempfile.TemporaryDirectory() as d:
    base = Path(d)
    (base / "tools").mkdir()
    (base / "miyako").mkdir()
    (base / "tools/build.py").write_text(
        '    PRODUCT = prefix + "00"\n'
        '    prefix + "%02d%02d%02d"\n', encoding="utf-8")
    (base / "miyako/fm1.toml").write_text(
        'id_prefix = "FM-1_9"\n', encoding="utf-8")
    (base / "miyako/app.h").write_text(
        '#define APP_VERSION "UNVERSIONED"\n', encoding="utf-8")
    info = base / "product.txt"
    subprocess.run([sys.executable, str(SCRIPT), "--base", str(base),
                    "--pipeline-number", "289", "--product-file", str(info)],
                   check=True, capture_output=True, text=True)
    assert info.read_text().strip() == "FM-1_939"
    assert 'PRODUCT = prefix + "39"' in (base / "tools/build.py").read_text()
    assert '#define APP_VERSION "FM-1_939"' in (
        base / "miyako/app.h").read_text()
    # Repeat with next pipeline, freshly seeded inputs, not a static 939.
    (base / "tools/build.py").write_text(
        '    PRODUCT = prefix + "00"\n'
        '    prefix + "%02d%02d%02d"\n', encoding="utf-8")
    (base / "miyako/app.h").write_text(
        '#define APP_VERSION "UNVERSIONED"\n', encoding="utf-8")
    subprocess.run([sys.executable, str(SCRIPT), "--base", str(base),
                    "--pipeline-number", "290", "--product-file", str(info)],
                   check=True, capture_output=True, text=True)
    assert info.read_text().strip() == "FM-1_940"
    assert 'PRODUCT = prefix + "40"' in (base / "tools/build.py").read_text()
    assert '#define APP_VERSION "FM-1_940"' in (
        base / "miyako/app.h").read_text()
    # Exercise the ACTUAL CI runtime path (job environment populated by
    # CircleCI's `pipeline.number` config interpolation), not just CLI args.
    (base / "tools/build.py").write_text(
        '    PRODUCT = prefix + "00"\n'
        '    prefix + "%02d%02d%02d"\n', encoding="utf-8")
    (base / "miyako/app.h").write_text(
        '#define APP_VERSION "UNVERSIONED"\n', encoding="utf-8")
    subprocess.run([sys.executable, str(SCRIPT), "--base", str(base),
                    "--product-file", str(info)],
                   env={**os.environ, "CIRCLE_PIPELINE_NUMBER": "291"},
                   check=True, capture_output=True, text=True)
    assert info.read_text().strip() == "FM-1_941"
    assert 'PRODUCT = prefix + "41"' in (base / "tools/build.py").read_text()
    assert '#define APP_VERSION "FM-1_941"' in (base / "miyako/app.h").read_text()
    missing = subprocess.run(
        [sys.executable, str(SCRIPT), "--base", str(base),
         "--product-file", str(info)],
        env={k: v for k, v in os.environ.items()
             if k != "CIRCLE_PIPELINE_NUMBER"},
        capture_output=True, text=True, check=False)
    assert missing.returncode != 0 and (
        "pipeline number is required" in missing.stderr)
    # The three-digit updater limit must fail, never roll back to 900.
    failed = subprocess.run(
        [sys.executable, str(SCRIPT), "--base", str(base),
         "--pipeline-number", "350", "--product-file", str(info)],
        capture_output=True, text=True, check=False)
    assert failed.returncode != 0 and "outside strict increasing" in failed.stderr
print("PASS: pipelines 289/290 via CLI, 291 via CircleCI environment, missing var and overflow rejected; UI=FWSC")
