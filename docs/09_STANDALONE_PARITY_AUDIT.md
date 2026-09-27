# Flower Standalone Parity Audit

Jira: AN-23

GoldenMaster under review:
`df2378f584f155349c33fea5e7ae212a29027049`

Read-only upstream reference:
`hakaihum-cpu/Vstplugin` / `feature/flower-actor-v3`

## Confirmed by static source comparison

| Area | Standalone status | Evidence / decision |
|---|---|---|
| Android standalone launch path | Present | GoldenMaster passed CircleCI android_build and user device launch/display smoke. |
| Synth oscillator | Present | SineVoice only. |
| Polyphony | Present | 8 SineVoice instances. |
| ADSR | Present | attack/decay/sustain/release APVTS parameters drive each voice ADSR. |
| Filter | Present | StateVariableTPT low-pass with cutoff/resonance. |
| LFO | Present | sine LFO; targets OFF/PITCH/CUTOFF/RESONANCE/LEVEL. |
| Flower rolling buffer | Present | continuously captures generated synth audio, maximum 16 seconds. |
| Flower bypass capture | Present | capture continues while Flower is disabled; dry output is left unchanged. |
| Granular engine | Present | 4 grains, sin² window, linear interpolation. |
| POSITION | Present | smoothed, waveform click/drag updates parameter. |
| SIZE | Present | 8–500 ms. |
| DENSITY | Present | 1–4 active grains. |
| SPREAD | Present | randomized anchor offset. |
| HOLD | Present | grain anchor repeat count. |
| PITCH | Present | -12 to +12 semitones. |
| REVERSE | Present | reverses grain travel. |
| MIX | Present | smoothed dry/wet. |
| CLEAR | Present | clears rolling buffer/state through atomic request. |
| Waveform telemetry | Present | 256 bins plus buffer validity, base position and four grain positions. |
| Actor v3 engine | Present | standalone carries the independent-actor component, 16 Hz independent simulation and variation layer. |
| Parameter state save/restore | Present | APVTS XML state in getStateInformation/setStateInformation. |
| MIYAKO isolation | Present | standalone build/runtime has no MIYAKO repository dependency. |


## Actor v3 production visual-bank status

The runtime engine is present, but the production high-resolution eight-student walk bank is not complete.

- approved high-resolution source strip present: student_01 only,
- student_02 through student_08 are not present as approved production strips in the repository,
- the renderer intentionally enables the high-resolution bank only when all eight students have both directional banks ready,
- the opposite direction is derived by exact geometric mirror from each approved source strip,
- until all eight approved strips exist, the current runtime falls back to the legacy embedded atlas and must not mix high-resolution and legacy actors in one scene,
- completion is tracked separately in AN-24.

This is an incomplete production asset bank, not a missing Actor-v3 engine implementation.

## Superseded legacy behavior

The older `docs/FLOWER_SPEC.md` describes explicit REC/DUB capture and overdub feedback.
The current Actor-v3 processor source no longer follows that workflow.

Current baseline behavior:
- rolling capture is always active,
- visible REC/DUB controls are absent,
- `flowerRecord` / `flowerOverdub` identifiers remain in the extracted parameter surface but do not drive the active audio path,
- FEEDBACK is not used as an overdub feedback control while REC/DUB is inactive.

Do not reintroduce REC/DUB or invent new FEEDBACK semantics without a separate explicit requirement.

## Still requires target-device functional verification

Static source presence is not equivalent to physical-device verification. The following remain acceptance checks rather than known defects:
- external MIDI note input,
- on-screen keyboard note input,
- ADSR response,
- filter cutoff/resonance response,
- LFO target/rate/depth response,
- Flower enable/bypass,
- CLEAR,
- waveform position drag,
- each granular parameter,
- reverse,
- state restoration after app restart where supported by the standalone wrapper.

No CI build is required for this documentation-only audit.
