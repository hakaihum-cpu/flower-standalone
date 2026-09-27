# androidapp Development Contract

Status: INITIAL BASELINE
Project: androidapp
Repository: hakaihum-cpu/flower-standalone
Jira project: AN
Purpose: Android application development

## 1. Golden Baseline
`main` represents only a user-approved, confirmed state.
A successful build alone does not make a revision a Golden Baseline.

## 2. No implicit merge
No feature, fix, documentation update, generated artifact, or CI result is merged to `main`
without an explicit merge instruction.

## 3. Minimum-diff rule
Each change must be limited to the stated Jira issue and PR scope.
Unrelated cleanup/refactoring is prohibited unless separately approved.

## 4. Build-last rule
Order of work:
1. Requirement/issue
2. Static inspection
3. Minimal patch
4. Diff inspection
5. Build only if it adds evidence
6. Test
7. Artifact recording
8. Merge decision

## 5. No automatic CI spending
The CircleCI workflow is disabled by default.
A build requires the explicit pipeline parameter `run_build=true`.

## 6. Traceability
Every functional change must have:
Jira issue -> branch -> commit/PR -> build (if any) -> artifact -> baseline decision.

## 7. Regression responsibility
A change is incomplete until its affected confirmed behavior has been checked.

## 8. Unknown product requirements
No product feature, UI design, data model, permissions, network behavior, monetization,
analytics, or store-distribution policy is assumed until it is explicitly specified.
