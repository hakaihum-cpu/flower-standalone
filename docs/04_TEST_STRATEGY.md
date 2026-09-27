# Test Strategy

This project follows the MIYAKO verification model: static-first, targeted verification, explicit regression coverage, and release-candidate verification.

## 1. Test layers
Use only the layers relevant to the change:
- Static source/resource inspection
- Diff audit
- Android Lint
- Unit tests
- Audio-engine tests where practical
- UI/instrumentation tests where practical
- MIDI tests
- Audio output / latency / stability checks
- Device-specific Android checks
- Manual smoke tests
- Release regression pass

## 2. Before any CI build
Confirm:
1. Jira issue and acceptance criteria exist.
2. Source branch and Golden Baseline are known.
3. Exact changed files are known.
4. No unrelated diff exists.
5. Resource/package changes are intentional.
6. No MIYAKO-only dependency leaked into standalone.
7. Existing confirmed behavior at risk is listed.

If these checks fail, do not build.

## 3. Targeted verification
A change should first be tested against the behavior it modifies.
Do not immediately run a broad full build/test sequence when a static or narrower test answers the question.

## 4. Regression matrix
For every functional change record:
- changed behavior,
- adjacent behavior at risk,
- baseline behavior that must remain unchanged,
- previous defects relevant to the same area,
- device/interface conditions needed for verification.

For Flower standalone, likely regression categories include:
- Sine synth note on/off
- ADSR
- Filter
- LFO
- Flower record
- Flower playback
- DUB
- CLEAR
- feedback
- reverse
- 4-grain playback
- POSITION/SIZE/DENSITY/SPREAD/HOLD/PITCH/MIX
- waveform/telemetry
- Actor v3 independent movement
- no synchronized whole-cast reset
- MIDI input
- Android lifecycle/resume
- audio stability

This list grows as confirmed functionality grows.

## 5. Defect verification
A defect fix requires:
1. reproduce on a known build where possible,
2. identify root cause,
3. apply minimum fix,
4. verify original reproduction no longer fails,
5. verify adjacent behavior,
6. record exact fix commit/build,
7. keep the defect open until verification is complete.

## 6. CI failure rule
A failed build is evidence, not an instruction to rebuild.
Before rerunning:
- inspect the failing step,
- capture the error,
- determine whether code/config/environment caused it,
- patch only the identified cause,
- rerun once when justified.

## 7. Release-candidate verification
Before release:
1. Freeze intended scope.
2. Verify Jira issues and specification are synchronized.
3. Diff release candidate against current Golden Baseline.
4. Run static checks.
5. Run required CI build once.
6. Verify generated APK is from the intended commit.
7. Install and smoke-test on actual Android hardware.
8. Exercise core audio/MIDI/Flower behavior.
9. Check regressions and known issues.
10. Record result and artifact.
11. Release only after explicit approval.
