# Operation Rules

## Work starts from a Jira issue
No implementation task exists only in chat history.

## Branch creation
Create the work branch from the current approved source branch.
Feature work must not start by modifying `main` directly.

## Before build
Verify:
- issue scope
- exact changed files
- diff contains no unrelated changes
- configuration files were not unintentionally replaced
- existing baseline resources remain present

## Build trigger
Do not trigger a build just to see what happens.
Build only when it verifies something that static inspection cannot.

## Artifact policy
Artifacts must be associated with the exact commit that produced them.
Do not relabel an older APK as a newer baseline.

## Failure policy
On regression:
1. stop expansion of the change
2. identify first bad diff
3. restore the known baseline
4. isolate root cause
5. add a regression check before continuing
