#!/usr/bin/env python3
"""FM-1 update identity: guarantee a fresh 9xx number on each CircleCI job.

Established from actual CircleCI job #243 (built FM-1_926).
The existing CircleCI project, workflow and branch stay unchanged.
"""
import argparse
from pathlib import Path

BASE_JOB = 243
BASE_ID = 926
MAX_ID = 999

def run(build_script: Path, job_number: int, record: Path) -> str:
    number = BASE_ID + job_number - BASE_JOB
    if job_number <= BASE_JOB or number > MAX_ID:
        raise RuntimeError(f"Invalid job number {job_number}: expected a unique version 927..999")
    product = f"FM-1_{number}"
    original = build_script.read_text(encoding="utf-8")
    if original.count('PRODUCT = "FM-1_926"') != 1 or original.count("VERSION = None") != 1:
        raise RuntimeError("KARMA build identity source changed; fail without producing a misleading FWSC")
    updated = original.replace('PRODUCT = "FM-1_926"', f'PRODUCT = "{product}"', 1)
    updated = updated.replace("VERSION = None", f'VERSION = "MIYAKO FM1 {number}"', 1)
    build_script.write_text(updated, encoding="utf-8")
    record.parent.mkdir(parents=True, exist_ok=True)
    record.write_text(f"FWSC_PRODUCT={product}\nCIRCLE_BUILD_NUM={job_number}\nBASE_JOB={BASE_JOB}\n", encoding="ascii")
    print(f"PASS: update identity {product} (old was 926, CircleCI job #{job_number})")
    return product

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("build_script", type=Path)
    p.add_argument("record", type=Path)
    p.add_argument("--build-number", type=int, required=True)
    args = p.parse_args()
    run(args.build_script, args.build_number, args.record)
