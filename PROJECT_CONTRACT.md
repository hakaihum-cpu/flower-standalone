# Flower Standalone Development Contract

Status: ACTIVE
Project: flower-standalone
Repository: hakaihum-cpu/flower-standalone
Jira project: AN

This project follows the same development, defect, verification, release and specification-management discipline established for MIYAKO, while remaining a completely separate product and repository.

## 1. Golden Baseline
- `main` represents only a user-approved, confirmed Golden Baseline.
- A successful build does not make a revision a Golden Baseline.
- A visually plausible result does not make a revision a Golden Baseline.
- Golden Baseline changes only after explicit user instruction to merge.
- The exact baseline commit and corresponding artifact must be recorded.

## 2. No implicit merge
No feature, fix, documentation update, generated artifact, CI result, or release candidate is merged to `main` without explicit user approval.

## 3. Branch isolation
- Every implementation/fix uses a dedicated issue-linked branch.
- Work stays on that branch until explicitly merged.
- Unrelated features are not combined into one change.
- MIYAKO branches and files are never modified by AN-* work.

## 4. Minimum-diff rule
- Modify only files required by the Jira issue.
- No opportunistic cleanup, refactoring, reformatting, regeneration, or dependency updates.
- No reconstruction of already-working features from prompts when patching the existing implementation is possible.
- Anything outside scope is treated as protected.

## 5. Specification is authoritative
Confirmed requirements must live in Jira and/or versioned `docs/`.
Chat history alone is not the specification of record.
When a requirement changes:
1. identify the previous requirement,
2. record the requested change,
3. record affected behavior/files/tests,
4. update the specification before or with implementation,
5. preserve traceability to the Jira issue.

Unknown requirements remain explicitly unknown. Do not invent them.

## 6. Static-first / build-last
Required order:
1. requirement/issue confirmation,
2. identify Golden Baseline and source branch,
3. static source/resource inspection,
4. minimum patch,
5. diff audit,
6. dependency/regression impact audit,
7. build only when it provides evidence static inspection cannot,
8. targeted test,
9. regression verification,
10. artifact and build record,
11. baseline/merge decision.

A build must never be used as a substitute for understanding the diff.

## 7. CI cost control
- GitHub Actions are not used for this project.
- CircleCI remains manually gated.
- Normal source changes must not trigger Android builds.
- Do not rebuild repeatedly until green.
- On failure, inspect the failing step/log and change only the identified cause before rerunning.

## 8. Defect management
Every reproducible defect discovered during development or verification must be traceable to Jira.
For each defect record:
- observed behavior,
- expected behavior,
- exact build/commit,
- reproduction conditions,
- affected area,
- severity/impact,
- fix branch/commit,
- verification result,
- regression check.

A defect is not closed merely because a new APK was produced.

## 9. Regression responsibility
A change is incomplete until:
- affected confirmed behavior is identified,
- unchanged protected behavior is checked,
- known previous regressions relevant to the area are checked,
- any newly discovered regression becomes a Jira issue.

## 10. Artifact integrity
Every artifact must map to the exact commit that produced it.
Do not:
- relabel old APKs as new builds,
- rebuild the same code unnecessarily,
- generate replacement ZIP/APK packages without need,
- treat an unverified local/package artifact as equivalent to a recorded CI artifact.

## 11. Failure response
When a regression or process failure occurs:
1. stop expanding the change,
2. identify the first bad diff/build,
3. compare against the Golden Baseline,
4. restore the known-good state if necessary,
5. isolate root cause,
6. add a structural prevention/check,
7. only then continue.

"Be more careful" is not considered a sufficient prevention measure.

## 12. Release decision
Release readiness requires:
- requirements/specification updated,
- all intended issues resolved or explicitly deferred,
- static audit passed,
- targeted tests passed,
- regression checklist passed,
- release candidate built from the exact intended commit,
- artifact recorded,
- smoke test passed on target Android device(s),
- known issues documented,
- explicit user approval.

## 13. Project isolation from MIYAKO
MIYAKO may be read only as the initial Flower source reference.
No automatic sync exists in either direction.
No shared branch, build state, Golden Baseline, release state, or Jira issue is implied between the projects.
