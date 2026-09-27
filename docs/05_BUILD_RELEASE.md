# Build and Release

## Cost control
CI builds are manual by default.
Normal pushes do not execute the Android build job.

To run the CircleCI workflow:
- Trigger a pipeline for the repository.
- Set parameter `run_build` to `true`.

## Build command
`./ci/gradle-bootstrap.sh --no-daemon lint testDebugUnitTest assembleDebug`

## Debug artifact
`app/build/outputs/apk/debug/app-debug.apk`

## Build record
Record at minimum:
- Build ID
- Date/time
- Jira issue
- Branch
- Commit SHA
- Result
- Artifact
- Whether candidate/Golden Baseline
- Notes

## Release signing
No signing key, password, Play service account, or other secret is committed to the repository.
Release signing is intentionally not configured until distribution requirements are decided.
