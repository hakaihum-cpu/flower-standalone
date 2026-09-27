# Build and Release

## 1. CI policy
GitHub Actions are not used.
CircleCI is the Android CI/build service.

Normal pushes must not start an Android build.
A build is run only when explicitly justified and manually triggered.

Current CircleCI gate:
- pipeline parameter: `run_build=true`

## 2. Build-last policy
Do not build merely to discover whether the source is valid.
Before a build:
- static inspection must be complete,
- diff must be audited,
- target behavior must be identified,
- expected evidence from the build must be stated.

## 3. Rebuild prohibition
Do not:
- repeatedly rerun a failed job without analysis,
- regenerate an APK/ZIP when an existing artifact already proves the same commit,
- rebuild only to rename/repackage an artifact,
- use CI as an exploratory debugger when static inspection is sufficient.

## 4. Build record
Every actual build records:
- Build ID
- Date/time
- Jira issue
- Branch
- Source commit SHA
- CI configuration revision
- Result
- Artifact path/link
- Artifact checksum when available
- Target device/test result
- Golden Baseline candidate: yes/no
- Notes/root cause if failed

## 5. Artifact identity
The artifact and source commit are inseparable.
An APK is referred to by its producing commit/build, not by a convenient filename alone.

## 6. Failed build handling
On failure:
1. do not immediately rerun,
2. inspect logs,
3. classify code/config/environment/tool failure,
4. create/update Jira defect if it represents a product/process defect,
5. make the minimum correction,
6. run static review again,
7. rerun only when the new run can verify the correction.

## 7. Release candidate
A release candidate is created only after feature/fix scope is frozen.

A release candidate must have:
- exact commit SHA,
- successful required CI build,
- recorded artifact,
- completed regression checklist,
- physical-device smoke test,
- known issues list,
- matching specification,
- explicit approval.

## 8. Golden Baseline promotion
A release candidate does not automatically become `main`.
Promotion requires explicit user instruction.
After promotion, record:
- previous baseline,
- new baseline commit,
- artifact,
- release/version label,
- associated Jira issues.

## 9. Signing and secrets
No signing key, password, Play service account, token, or secret is committed to the repository.
Release signing remains separate from source control.
