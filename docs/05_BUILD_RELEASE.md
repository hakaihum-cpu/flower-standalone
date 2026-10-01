# Build and Release

Same operational model as FLOWER:
- GitHub Actions unused.
- CircleCI Android build manually gated by `run_build=true`.
- Static audit and diff audit precede a build.
- Failed run is analysed before rerun.
- Artifact is bound to exact commit and SHA-256.
- Physical target-device smoke is required.
- Successful build is not MASTER/Golden Baseline.
- Promotion/merge requires explicit user instruction.