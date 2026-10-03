# EP VIOLIN — Physical Modeling MVP

Branch: `feature/EP-violin-physical-model`

Base: `feature/EP-sampler-mvp` at `3c3858a15a112733b0e0bcd034739fb3f0a0416e`

## Scope

This branch keeps the proven EP Sampler Android/AAudio/MIDI shell but replaces the sample-playback signal path with a four-string bowed physical model.

No sample bank is required for sound generation.

### Instrument model

- Four fixed physical strings: G3 / D4 / A4 / E5.
- Maximum polyphony: four strings.
- Notes are allocated to physically reachable strings; higher open strings and shorter finger distances are preferred.
- Legato on an occupied string changes effective pitch without clearing the string state.
- Each string uses a 24-mode damped modal resonator bank.
- Bow excitation uses a stateful velocity-dependent stick/slip friction approximation.
- Bow position changes modal excitation weights.
- Pitch bend changes effective finger pitch (±2 semitones).
- Vibrato modulates effective string length/fundamental rather than post-processing audio.
- All four strings feed one shared 14-mode body resonator.
- NaN/non-finite protection and bounded modal states are present in the realtime path.

### MIDI

- Note: finger pitch / string allocation
- Velocity: initial bow force
- CC1: vibrato depth
- CC7: master volume
- CC10: bow pressure
- CC11: bow speed
- CC64: sustain / bow hold
- CC74: bow position
- Poly Pressure: per-note bow pressure expression
- Channel Pressure: global bow pressure expression
- Pitch Bend: ±2 semitones
- CC120 / CC123: release all bows

Existing optional EP effects remain available after the physical model:
BOOST, SPACE, TAPE, DREAMY.

## Android identity

The Gradle applicationId is `com.example.epviolin` so the physical-model experiment can coexist with the EP Sampler APK. Java/JNI source packages remain `com.example.epsampler`; the launcher activity is therefore fully qualified in AndroidManifest.xml.

## Build

The existing CircleCI EP_Sampler path is intentionally retained. No GitHub Actions workflow was added and no main/Golden branch was modified.

The existing manual CircleCI gate remains `run_build=true`.

At source-construction time this branch is **not yet CI/APK verified**. Treat it as an experimental source branch until the existing CircleCI job succeeds and the APK is checked on-device.

## First device checks

1. Open G3, D4, A4, E5 individually.
2. Play all four together and check for underruns or runaway output.
3. Sweep CC10 (bow pressure) and CC11 (bow speed); there should be regions of clean, rough, and weak/non-speaking behavior rather than simple volume changes.
4. Sweep CC74 and listen for harmonic-color change.
5. Test legato on one string.
6. Hold for 10 minutes and check for NaN/runaway/continuous clipping.
7. Verify Note Off, CC123, MIDI disconnect behavior and restart.
