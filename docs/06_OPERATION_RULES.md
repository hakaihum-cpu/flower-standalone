# Operation Rules

1. Separate repository/project from FLOWER and MIYAKO.
2. No source or asset writes to those repositories.
3. One issue scope per feature/fix branch.
4. Main is Golden Baseline only after explicit approval.
5. Static-first; build-last.
6. User-supplied 300-frame bank is protected visual baseline.
7. No AI/generated replacement frames.
8. No ornamental knob redesign unless explicitly requested.
9. Any repeated failure class gets a structural check, not a reminder.
10. Build ledger records branch, commit, run, artifact, checksum and device result.
11. Before any write/build, read `CURRENT_OPERATION_STATE.json`; do not rely on chat memory alone.
12. Repository/branch/CI mismatch is a hard stop before source mutation or build.
13. CircleCI must run `scripts/verify_operation_context.py --ci` before materialization, static audit, or compilation.
