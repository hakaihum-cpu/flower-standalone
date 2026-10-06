#!/usr/bin/env python3
import argparse
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE_PATH = ROOT / "CURRENT_OPERATION_STATE.json"

def fail(message):
    raise SystemExit(f"[FAIL] operation context: {message}")

parser = argparse.ArgumentParser()
parser.add_argument("--ci", action="store_true")
args = parser.parse_args()

if not STATE_PATH.is_file():
    fail("CURRENT_OPERATION_STATE.json is missing")

state = json.loads(STATE_PATH.read_text(encoding="utf-8"))

required = {
    "project": "EFFECTS / RealtimeChordFX",
    "repository": "hakaihum-cpu/flower-standalone",
    "active_branch": "feature/haze-emulation",
    "protected_branch": "main",
    "main_write_allowed": False,
    "merge_to_main_requires_explicit_user_instruction": True,
    "ci_provider": "CircleCI",
    "ci_workflow": "manual_android_build",
    "manual_build_parameter": "run_build=true",
    "github_actions_allowed": False,
    "failed_build_retry_policy": "root_cause_must_be_identified_before_rerun",
    "source_of_truth": "repository_state_not_chat_memory",
}
for key, expected in required.items():
    if state.get(key) != expected:
        fail(f"{key}={state.get(key)!r}, expected {expected!r}")

if state["active_branch"] == state["protected_branch"]:
    fail("active development branch resolves to protected main")

workflows = ROOT / ".github" / "workflows"
if workflows.is_dir() and any(p.is_file() for p in workflows.rglob("*")):
    fail("GitHub Actions workflow files exist although GitHub Actions are forbidden")

if args.ci:
    owner = os.environ.get("CIRCLE_PROJECT_USERNAME", "")
    repo = os.environ.get("CIRCLE_PROJECT_REPONAME", "")
    branch = os.environ.get("CIRCLE_BRANCH", "")
    actual_repo = f"{owner}/{repo}" if owner and repo else ""

    if actual_repo != state["repository"]:
        fail(f"CircleCI repository {actual_repo!r} != {state['repository']!r}")
    if branch != state["active_branch"]:
        fail(f"CircleCI branch {branch!r} != {state['active_branch']!r}")
    if branch == state["protected_branch"]:
        fail("CircleCI build attempted from protected main")

print("[PASS] operation context matches authoritative repository state")
