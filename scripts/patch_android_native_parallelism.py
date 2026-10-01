#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

JOB_POOL_ARGS = [
    "-DCMAKE_JOB_POOLS:STRING=compile=2;link=1",
    "-DCMAKE_JOB_POOL_COMPILE:STRING=compile",
    "-DCMAKE_JOB_POOL_LINK:STRING=link",
]

def fail(message: str) -> None:
    print(f"[FAIL] {message}")
    raise SystemExit(1)

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Builds/Android/app/build.gradle")
if not path.is_file():
    fail(f"generated Gradle file missing: {path}")

text = path.read_text(encoding="utf-8")
lines = text.splitlines(keepends=True)

matches = [
    i for i, line in enumerate(lines)
    if "arguments(" in line and "-DANDROID_TOOLCHAIN=" in line
]

if len(matches) != 1:
    fail(f"expected exactly one defaultConfig CMake arguments line, found {len(matches)}")

index = matches[0]
line = lines[index]

present = [arg in line for arg in JOB_POOL_ARGS]
if all(present):
    print("[PASS] CMake Ninja job pools already injected")
    raise SystemExit(0)

if any(present):
    fail("partial CMake job-pool injection detected")

newline = "\n" if line.endswith("\n") else ""
body = line[:-1] if newline else line

close = body.rfind(")")
if close < 0:
    fail("CMake arguments line has no closing parenthesis")

injected = ", " + ", ".join(f'"{arg}"' for arg in JOB_POOL_ARGS)
lines[index] = body[:close] + injected + body[close:] + newline

updated = "".join(lines)

for arg in JOB_POOL_ARGS:
    if updated.count(arg) != 1:
        fail(f"job-pool argument did not resolve uniquely: {arg}")

path.write_text(updated, encoding="utf-8")
print("[PASS] Injected CMake Ninja job pools: compile=2, link=1")
