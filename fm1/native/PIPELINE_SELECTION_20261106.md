# MIYAKO FM-1 — existing CircleCI pipeline selection (verified 2026-11-11)

## Actual source of truth

- GitHub repository / both configuration and checkout source: `hakaihum-cpu/flower-standalone`
- Requested Git branch: `feature/fm1-three-track-rec-drum-routing-video-slicer-20261110`
- Configuration path on that branch: `.circleci/config.yml`
- Existing CircleCI project used for these builds: `androidapp`
- Manual parameter: `run_build` = boolean `true` (the default is `false`)
- **Workflow inside YAML:** `fm1_manual`
- **Job inside YAML:** `fm1_native_build`
- Output of a successful job: `fm1/MIYAKO-FM1-NATIVE-DEV.fwsc` and the matching `-res.bin`

## Important: the three CircleCI names are not interchangeable

In CircleCI's *Trigger Pipeline* UI, the selectable **pipeline definition**
is configured in the CircleCI project. It is NOT the YAML workflow
`fm1_manual` or the job `fm1_native_build`.

Use the existing project/definition that served the prior FM-1 build
(#300), **after checking that its config-source repository and checkout-source
repository are both `hakaihum-cpu/flower-standalone`**. For a GitHub App
integration, set BOTH the config branch and checkout branch to the exact feature
branch above. If the existing definition points to another repository, this
cannot be repaired simply by changing the YAML on `flower-standalone`.
Do not trigger another repository's definition as a workaround.

The feature branch above exists in `flower-standalone`, not in `Vstplugin`.
For GitHub OAuth projects the configuration and checkout come from the same
repository. Neither case requires a new CircleCI workflow or pipeline definition
if the existing one is configured for this repository.

## Build safety

The first job checks the exact branch and checkout-origin repository before
running source regressions, upstream FM-1 compile/link, media packing, firmware
validation and artifact upload. The generated firmware product identity depends
on `pipeline.number`, not on the job number or the literal workflow name.

Successful baseline: prior #300 / source commit
`e1e312a2a45cf482a8fe2a6cb6478257957a60e7`, protected as branch
`fm1-master-circleci-300-20261110`. Do not alter `main` or the baseline.

**Status 2026-11-11:** GitHub config and branch were checked. No CircleCI
project-settings API access or real current pipeline run was available for this
inspection. A complete target cross-compile/link and a successful .fwsc for
this feature branch have NOT been observed. Never claim build success based
only on this document or source tests. No build was triggered by this review.
