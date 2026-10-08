# E.PIANO — hybrid multisample prototype v0.1

Issue: AN-57
Branch: `feature/epiano-hybrid-v1`
Starting baseline: `feature/ep-speed-editor-integration` at `a523b3fe72e2f1ecb311221fcba8dbcd443e7bbe`.
Do not merge into `main` or any golden baseline without the owner's explicit instruction.

## What is implemented
- Separate Android app installation ID `com.analoglav.epiano`, launcher name `E.PIANO`.
- Existing EPBANK1 playback, 8-layer velocity interpolation, 3 round robins, sustain/release and 64-voice shared pool stay in their original processing path.
- Default selected instrument is SAMPLE Bank 1 / Part 9 (internal index 8, default MIDI CH9); user selects the existing *complete* EPBANK1 .bin through the same bank picker.
- Added optional DSP to SAMPLE Bank 1 only: a limited 18-pole modal resonance bank, quiet parallel preamp colouring and stereo tremolo.
- CONFIG controls: HYBRID DSP ON/OFF, RESONANCE, PREAMP, TREMOLO. Settings persist independently from the original EP-SAMPLE installation.
- MIDI: CC80 on/off; CC81 resonance; CC82 preamp; CC83 tremolo. Route to the sample-bank instrument (default CH9).
- E.PIANO CONFIG snapshots current / peak audio callback time as a fraction of audio deadline, plus AAudio XRun count. This is **not** total Android CPU usage.
- Separate CircleCI feature-branch APK artifact name: `EPiano-Android.apk`.

## Deliberate constraints
- The full 1GB+ EPBANK binary is not included in Git. Split pieces are not individual valid EPBANK1 banks.
- No source changes to other existing branches and no changes to the shared source-bank reader or voice interpolation rules.
- The modal DSP is *an experimental enhancement*, not a physically accurate component model of a particular Rhodes or Wurlitzer.
- Nothing forces CPU usage toward 80%. Target: sustained audio callback load 65–70% or less and peaks 80% or less, **subject to RG Rotate measurement**, with 0 XRuns. Higher average CPU utilisation is not a sign of quality.
- No claims about sound quality, long-run stability, or APK correctness prior to actual CircleCI success and RG Rotate tests.

## Test gate
1. Static: inspect diff against starting baseline; confirm only feature branch changed; verify manifest label, package ID, JNI declarations/definitions, header and CMake includes, CircleCI manual gate.
2. Manual CircleCI `run_build=true` for the exact final commit and only once static checks pass.
3. Confirm CircleCI success and `SOURCE_COMMIT.txt` matches requested feature-branch HEAD.
4. Install beside EP-SAMPLE (no overwriting), select the complete EPBANK1 .bin, confirm BANK READY.
5. Validate all 8 velocities, 3 RR variants and release, sustain pedal and repeated Note On/Off, 64-voice stress, MIDI routing and app restart.
6. With all DSP off, compare the rendered sound to the original; then test each DSP separately and all enabled.
7. Test RG Rotate for 30 min at 48 kHz: DSP current/peak budget, XRun, audio clicks, thermals, battery and USB audio routing.
8. Record CI number, resulting APK name and device outcomes in the Build Ledger and Jira.

## Known scope limits
- The first prototype keeps the existing EP-SAMPLE-based layout (with a CONFIG section); it is not yet a redesigned standalone piano UI.
- Actual high-density convolution, per-voice physical modelling, prefetch optimisations, pedal half-damping and 80%-of-CPU stress optimisation are **not implemented** in v0.1.
- Branch creation, source changes and static inspection do not constitute an APK build.
