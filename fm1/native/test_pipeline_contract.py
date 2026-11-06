#!/usr/bin/env python3
"""CI contract: fail closed rather than accidentally shipping legacy FM-1 code."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
ci = (root / ".circleci/config.yml").read_text(encoding="utf-8")
required = (
    "version: 2.1",
    "default: false",
    "when: << pipeline.parameters.run_build >>",
    "only: feature/fm1-miyako-standalone-20261106",
    "fm1_standalone_preflight",
    "fm1/native/verify_standalone.py",
    "fm1/native/test_mi_control.c",
)
for item in required:
    assert item in ci, f"missing mandatory CircleCI gate: {item}"
for forbidden in (
    "eatyourgreensstudio-dotcom",
    "KARMA_PIN",
    "fm1/karma/",
    "fm1-karma",
    "tools/get_toolchain.sh",
    "build/felucca.fwsc",
):
    assert forbidden not in ci, f"legacy implementation in native CI: {forbidden}"
assert ci.count("workflows:") == 1
assert ci.count("when: << pipeline.parameters.run_build >>") == 1
assert re.search(r"^\s+only: feature/fm1-miyako-standalone-20261106\s*$", ci, re.M)
assert "fm1-dist/MIYAKO-FM1-PREVIEW.fwsc" in ci
assert "test ! -e fm1-dist/MIYAKO-FM1-PREVIEW.fwsc" in ci
print("PASS: strictly manual, standalone branch, no legacy backend or fake firmware")
