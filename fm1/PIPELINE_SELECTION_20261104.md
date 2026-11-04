# MIYAKO FM-1 — CircleCI pipeline selection (NOT triggered)
Date: 2026-11-04
Repository: hakaihum-cpu/flower-standalone

## Exact pipeline selection
- Config path: `.circleci/config.yml`
- Config source branch: `feature/fm1-miyako-port-20261104`
- Checkout source branch: `feature/fm1-miyako-port-20261104` (only if the GitHub App integration asks separately)
- Workflow declared by repository: `fm1_manual`
- Job: `fm1_firmware`
- Pipeline parameter: `run_build` (type: boolean)
- Current safe default: `false`
- To actually build upon explicit approval: `run_build=true`
- Pinned KARMA commit: `db3cda79359ae914d1e58c0fc96c6ca744e7950e`
- Expected artifact after successful job: `fm1/MIYAKO-FM1-PREVIEW.fwsc`, plus `SHA256SUMS`

## Execution boundary
This file records the selection only. No CircleCI trigger or build has
occurred. A numeric CircleCI pipeline number/ID is assigned only after the
provider actually creates a pipeline; do not invent a number.

The CircleCI project's pipeline **definition name and ID** are provider-side
settings not available from GitHub repository source alone. If the CircleCI
project has multiple definitions, select the one whose configuration source
points to this repository and `.circleci/config.yml`. The workflow/job
names above are exact and come from the committed config, not assumptions
about its CircleCI account configuration.

## Operator action once a build is authorized
In CircleCI > Projects > Vstplugin > Pipelines > Trigger Pipeline:
1. Select the correct pipeline definition for this GitHub repo.
2. Set config source branch to `feature/fm1-miyako-port-20261104`.
3. Set checkout branch to the same if offered.
4. Select parameter `run_build`, type boolean, value true.
5. **Only after approval**, click Run Pipeline.

No build is authorized by committing this selection file.

## Existing CircleCI pipeline: FM-1 branch
- Pipeline: `androidapp-build` (the existing CircleCI integration for `flower-standalone`)
- Config Source Branch: `feature/fm1-miyako-port-20261104`
- Checkout Source Branch: `feature/fm1-miyako-port-20261104`
- Boolean parameter: `run_build=true` only upon user build authorization.
- Workflow: `fm1_manual`; job: `fm1_firmware`.
- No GitHub Actions and no new CircleCI pipeline definition.
