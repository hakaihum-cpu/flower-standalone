# Operation Rules

## 1. Work starts from Jira
No implementation, defect fix, or specification change exists only in chat history.
Each actionable change maps to Jira.

## 2. Specification handling
- Confirmed requirements are written to Jira and/or versioned docs.
- Undecided items stay marked undecided.
- Implementation must not silently redefine a requirement.
- If implementation reveals a contradiction, stop and update/clarify the specification before proceeding.
- Screenshots/assets that become approved visual baselines must be identified/versioned explicitly.

## 3. Branch discipline
- One issue/scope -> one dedicated branch.
- No routine direct development on `main`.
- Do not merge until explicitly instructed.
- Do not mix MIYAKO changes into this repository.
- Do not use this repository to alter MIYAKO.

## 4. Before editing
State internally and verify:
- source branch,
- target issue,
- files allowed to change,
- areas that must not change,
- current Golden Baseline.

## 5. Before build
Verify:
- issue scope,
- exact changed files,
- no unrelated changes,
- configuration/resources were not unintentionally replaced,
- baseline resources remain present,
- no known working feature was reconstructed unnecessarily,
- no MIYAKO-only dependency remains.

## 6. Build trigger
Do not trigger a build just to see what happens.
Build only when static inspection cannot provide the needed evidence.

## 7. Defect lifecycle
For a defect:
1. create/update Jira ticket,
2. capture reproduction and affected build,
3. identify root cause,
4. patch minimally,
5. statically inspect,
6. run targeted verification,
7. run regression check,
8. record result,
9. close only when verified.

## 8. Regression handling
If an existing function regresses:
- stop expanding the feature,
- compare to Golden Baseline,
- isolate the first bad change,
- restore or patch from the known-good state,
- add a repeatable regression check,
- continue only after stability returns.

## 9. Artifact policy
Artifacts must be associated with their exact source commit.
Do not rename/repackage old artifacts as if newly built.
Do not generate duplicate deliverables without purpose.

## 10. Release operation
Before release:
- freeze scope,
- reconcile Jira/spec/docs,
- review full diff from Golden Baseline,
- resolve or document all defects,
- run one justified release build,
- test the actual built APK on hardware,
- record artifact/checksum/build ID,
- require explicit approval for baseline promotion/release.

## 11. Structural prevention
When the same class of failure occurs more than once, add a structural guard:
- automated/static check,
- repository rule,
- explicit checklist,
- test,
- build gate,
- or state/baseline lock.

A reminder to "be careful" is not an acceptable long-term control.
