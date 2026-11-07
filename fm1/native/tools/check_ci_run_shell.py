#!/usr/bin/env python3
"""Parse-check all multiline CircleCI run scripts with bash -n.

Read-only, first-step guard. No commands are executed. Detects unbalanced
shell quotes, including late README-generation errors, before CI build work.
"""
from pathlib import Path
import re
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
