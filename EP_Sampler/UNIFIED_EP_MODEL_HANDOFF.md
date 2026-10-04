# EP-SAMPLE + Physical Modeling unified candidate

Date: 2026-10-04

## Baselines
- Physical-model Golden baseline: `golden/violin-multitimbral-2026-10-04`
- Physical baseline SHA: `9e629ec30f486701933198d0097160f89ebaa041`
- EP-SAMPLE standalone MASTER for sampler behavior reference: `main`
- EP-SAMPLE MASTER SHA at integration start: `5eec5d2f2455399f9990a602c6a3c398e8c8c047`

## Integration branch
- `feature/EP-physical-unified`
- Do not merge to main/golden without explicit user instruction.

## Implemented in this candidate
- Existing physical-model instruments remain parts 0..7:
  1. VIOLIN
  2. FLUTE
  3. SAXOPHONE
  4. FELT PIANO
  5. PIANICA / ACCORDION
  6. XYLOPHONE
  7. WOOD BASS
  8. DRUMS
- EP-SAMPLE added as independent ninth engine / part 8.
- Default external MIDI routing expanded to CH1..CH9; EP-SAMPLE defaults to CH9.
- Existing multitimbral same-channel layering behavior is preserved.
- EPBANK1 mmap loader restored using existing SampleBank implementation.
- EP playback behavior restored: 8 velocity layers, 3 RR, sustain/release samples,
  velocity interpolation, aftertouch response, +/-2 semitone pitch bend.
- EP standalone master-level relationship is compensated inside the EP part so
  the physical-model shared 1.55x master does not make EP unexpectedly quieter.
- Shared global FX remain Dreamy / Tape / Space / Boost.
- Per-part boost/distortion storage and DSP include EP part 8.
- Momentary XY behavior: DRUMS keeps Stutter; EP-SAMPLE uses Delay like non-drum parts.
- EP bank file is selected with ACTION_OPEN_DOCUMENT and its persisted URI is reloaded.
- EP mode uses the existing piano reference image; model modes keep their video layers.
- Integration app identity is separate:
  - applicationId `com.example.epmodel`
  - label `EP + MODEL`
  so it can coexist with the standalone EP and physical-model builds.

## Deliberately not ported in the first integration pass
- The standalone EP-SAMPLE RECORDER drawer from commit
  `5eec5d2f2455399f9990a602c6a3c398e8c8c047`.
  Reason: first validate the engine boundary, MIDI routing, bank loading, output level,
  and existing physical-model regressions before adding another realtime audio/UI path.

## Static checks completed
- Existing main unchanged.
- Existing physical Golden branch unchanged.
- CMake includes SampleBank.cpp.
- JNI/Java bank-loading methods remain connected.
- 9-part audio loop and part-8 routing checked.
- Remaining modelParts_[part-1] accesses are guarded from EP part 8.
- Java/C++ raw brace balance checked.
- CircleCI preflight-required files and ABI/signing settings remain present.
- No EP bank binary is intentionally added to the APK.

## Build status
CircleCI is configured with pipeline parameter `run_build` defaulting to false.
No authenticated CircleCI trigger is available from the current tool connection, so
this branch has not yet been compiled by CircleCI in this pass.

Next validation pipeline:
- branch: `feature/EP-physical-unified`
- pipeline parameter: `run_build=true`

First device test priorities:
1. Install alongside standalone EP and VIOLIN/physical app.
2. Confirm all 8 physical-model instruments still sound as the Golden baseline.
3. Select EP-SAMPLE, load the existing EPBANK1 .bin, verify note-on/off, velocity,
   3RR, sustain, pitch bend, aftertouch, and output level.
4. Confirm CH9 default routing and same-channel layering only when intentionally configured.
5. Touch XY on EP-SAMPLE and confirm momentary Delay is clean/no zipper noise.
6. Check XRuns with AUTO and the existing BURST settings.
