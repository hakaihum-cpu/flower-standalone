# Realtime Chord FX Development Contract

Status: ACTIVE (local bootstrap; dedicated remote repository not yet attached)

This project is a completely separate product from FLOWER and MIYAKO. It adopts the FLOWER operational discipline only; source, assets, issues, builds and release state are isolated.

## Authoritative operation state
- Before any repository write or CI build, read `CURRENT_OPERATION_STATE.json`.
- Repository state is authoritative; chat memory is never sufficient by itself.
- If repository, active branch, protected-branch policy, or CI provider does not match that file, stop before modifying source or starting a build.
- `scripts/verify_operation_context.py --ci` enforces the repository/branch/CI contract before CircleCI performs materialization or compilation.

## Golden Baseline
- `main` is reserved for a user-approved Golden Baseline once a dedicated repository exists.
- Build success never promotes a revision automatically.
- Promotion requires explicit user instruction to merge.

## Branch isolation
- Implementation uses an issue-linked feature/fix branch.
- No changes are made to FLOWER or MIYAKO repositories for this project.
- Unrelated work is not combined.

## Static-first / build-last
Required order: requirements -> baseline -> static inspection -> minimum patch -> diff audit -> regression/dependency audit -> justified build -> targeted test -> device test -> artifact record -> merge decision.

## Cost control
- GitHub Actions are not used.
- CircleCI is manually gated with `run_build=true`.
- Failed builds are analysed before any rerun.

## Asset contract
The 300 user-supplied classroom images are the authoritative visual bank. They are stored byte-for-byte inside `Resources/classroom_frames.pack`; generated substitute imagery is prohibited.

## Product contract
- This is an audio effect, not a synthesizer.
- Input timbre is transformed by pitch shifting to create generated chord voices.
- Main parameters: COMPLEX, BAR, WIDTH, LENGTH.
- Main image area toggles REC / STOP+CLEAR.
- CONFIG contains audio input selection/detection, MIDI CH and CLOCK. Internal clock includes the necessary BPM value.

## Artifact integrity
Every APK must map to exact branch, commit, CircleCI run and SHA-256. An APK is not MASTER until physical-device verification and explicit approval.