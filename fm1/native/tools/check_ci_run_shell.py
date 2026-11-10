#!/usr/bin/env python3
"""Parse-check CircleCI shell and precompile every first-stage host C test.

Read-only: no build artifacts are produced. This prevents another #301 where
contract tests passed but an unused-variable warning stopped the C compiler.
All eleven first-stage C commands are syntax-compiled with CircleCI's exact
-Werror flags before any host test binaries or media processing.
"""
from pathlib import Path
import re
import shlex
import subprocess
import sys


def commands(source: str):
    """Extract literal YAML command: | block scalars without external deps."""
    lines = source.splitlines()
    i = 0
    while i < len(lines):
        match = re.fullmatch(r"( *)command: *\|[-+]? *", lines[i])
        if not match:
            i += 1
            continue
        start_line = i + 1
        key_indent = len(match.group(1))
        i += 1
        block = []
        while i < len(lines):
            line = lines[i]
            if line.strip() and len(line) - len(line.lstrip(" ")) <= key_indent:
                break
            block.append(line)
            i += 1
        indents = [len(s) - len(s.lstrip(" ")) for s in block if s.strip()]
        if not indents:
            raise ValueError(f"empty run command at config line {start_line}")
        indent = min(indents)
        if indent <= key_indent:
            raise ValueError(f"invalid YAML indent at config line {start_line}")
        body = "\n".join((s[indent:] if s.strip() else "") for s in block) + "\n"
        yield start_line, body



def precompile_first_stage(script: str, root: Path) -> bool:
    """Compile all first-stage translation units without linking or artifacts."""
    lines = script.splitlines()
    i = 0
    compiles = []
    while i < len(lines):
        line = lines[i].strip()
        if not line.startswith("cc -std="):
            i += 1
            continue
        fragments = []
        while True:
            fragment = lines[i].strip()
            continuation = fragment.endswith("\\")
            fragments.append(fragment[:-1].rstrip() if continuation else fragment)
            if not continuation:
                break
            i += 1
            if i >= len(lines):
                raise ValueError("unterminated C compile command")
        argv = shlex.split(" ".join(fragments))
        if argv[0] != "cc" or "-o" not in argv:
            raise ValueError(f"unsafe or incomplete C compile command: {argv}")
        output = argv.index("-o")
        if output + 1 >= len(argv):
            raise ValueError("C compile command missing -o target")
        # Remove only -o and its filename. Preserve ALL original -Werror,
        # sanitizers and target source units for the syntax-only preflight.
        argv = argv[:output] + argv[output + 2:] + ["-fsyntax-only"]
        compiles.append(argv)
        i += 1
    if len(compiles) != 11:
        raise ValueError(f"expected 11 first-stage C test builds; found {len(compiles)}")
    for index, argv in enumerate(compiles, 1):
        result = subprocess.run(argv, cwd=root, capture_output=True, text=True, check=False)
        if result.returncode:
            print(f"FAIL: first-stage C test {index}/11 syntax preflight", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            return False
    print("PASS: all 11 host C test compile commands syntax-checked with original -Werror flags")
    return True


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    config = root / ".circleci/config.yml"
    source = config.read_text(encoding="utf-8")
    scripts = list(commands(source))
    run_steps = len(re.findall(r"(?m)^      - run:[ \t]*$", source))
    if not scripts or run_steps != len(scripts):
        print(f"FAIL: found {len(scripts)} command blocks, {run_steps} run steps", file=sys.stderr)
        return 1
    for line, script in scripts:
        result = subprocess.run(
            ["bash", "-n"], input=script, text=True, capture_output=True, check=False
        )
        if result.returncode:
            print(
                f"FAIL: .circleci/config.yml command at line {line}\n{result.stderr}",
                file=sys.stderr,
            )
            return 1
    print(f"PASS: bash -n for all {len(scripts)} CircleCI run commands")
    if not precompile_first_stage(scripts[0][1], root):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
