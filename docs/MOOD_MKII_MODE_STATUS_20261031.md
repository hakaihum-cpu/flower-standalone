# EFFECTS — MOOD MKII mode (2026-10-31)
## Scope and status
Feature branch created from approved `baseline/effects-golden-20261020-libpd-patch`. Existing mode IDs 0..9 are unchanged; MOOD MKII is mode 10. **Unbuilt, untested on Android**; no claim of circuit-level or DSP-algorithm equivalence.
Official MIDI manual: https://static1.squarespace.com/static/689b53dbdfec6d3f10536eae/t/68ccf358f11808289539219e/1758262104480/Chase-Bliss-MOODMKII_MIDIManual_JP.pdf

## Implemented
- Always-listening stereo input ring, CAPTURE swaps two preallocated stereo rings in O(1) time to hold recent audio (loop time follows CC14 and CC16).
- Mono note performance on captured audio using 12-TET resampling; touchscreen C4–C5 or external MIDI notes; note velocity, attack/decay/sustain/release envelope. Wet processing remains stereo.
- X/Y assignable CC selection: CC14 TIME, 15 MIX, 16 LENGTH, 17 WET MODIFY, 18 CLOCK, 19 LOOP MODIFY, 20 RAMP, 21 WET MODE, 22 ROUTING, 23 LOOP MODE, 58 OUTPUT, 80-84 envelope/portamento. By default X=14, Y=19. External controller CC input enters the engine, not MIDI output.
- Two channels: micro-loop output plus simplified wet reverb/delay/slip-type processing. No guaranteed equivalence to actual hardware.
- Keyboard overlay with translucent white/black keys, touch-drag note reassignment. CAPTURE, CC selectors, LOAD TILES.
- Visual bank uses only user-provided pictures. The four uploaded 1536² grids are split as 9×9, 9×9, 9×9 and 10×10 sheets = **343 actual individual JPEG frames**; no AI-generated images. Pack in `CRF1` layout from existing FramePack. Load from in-app **LOAD TILES** button; persisted at app private `EFFECTS/Mood/mood_frames.pack`. Animation plays individual 1:1 frames and responds to MIDI note, input onset, XY motion and timed random selection.

## Not implemented / incomplete
- 343-frame pack is separately provided as a downloadable artifact, **not yet committed as a bundled binary resource**. The app UI requires a first-time LOAD TILES import until this asset is embedded in release. The code falls back to a `BinaryData::getNamedResource("mood_frames_pack")` resource if the pack is bundled later.
- This is **not a MOOD MKII audio algorithm reproduction**: TAPE/STRETCH/ENV and WET DELAY/REVERB/SLIP are approximations, with basic reverse/stall, freeze (CC105), overdub (CC106) and no DSP parity verification. Original stereo width, hidden settings, presets, precise sync, ramp waveforms and the full CC map remain unimplemented.
- Basic ADSR attack/decay/sustain/release is wired to captured-note level, but its curves and timing are approximations. CC84 portamento and CC20 ramp are selectable yet **not wired to audio**; the controls must not be described as functional.
- Manual inconsistency: official JP MIDI document labels Output Type CC58 (page 9-10) but later captions mention CC59; separate third-party MIDI list reports CC87. Current UI maps 58 per official main table. Hardware verification needed.
- No APK build, no hardware smoke test, no latency/stereo/CPU audio measurements. The generated media pack is available separately and requires first-time user import, so this branch is not yet a fully self-contained distributable app.
- Previous failures to inspect before build: CircleCI config v2.1 unclosed `<<`, JUCE resource dependencies, config scroll UI, MIDI port and effect mode range, audio callback allocations, older CHORD-A/Haze golden regressions.

## Asset format
`mood_frames.pack` header `CRF1`, little-endian uint32 count(343), 343 entries uint64 offset + uint32 JPEG length, then JPEG bytes, exactly as read by `Source/FramePack.cpp`.
Frame order: source image 1 rows 0..9, columns 0..9, then source image 2, 3, 4. Each tile is a square crop (grid border excluded) resized to 150×150 pixels; photos are user-supplied and remain black and white.

## Release gate
1. Verify each CC and actual sound effect; compare native note-on/off and UI keyboard; choose independently from incoming MIDI.
2. Verify loading 343 frames, random transitions and DOT mode, import persistence on Android 12/RG rotate.
3. Confirm no clicks, excessive xrun, feedback runaway, frozen capture glitch, dry/wet multiplication.
4. Check UI 720×720, menu selection all 13 entries, transparency and no overlap with EFFECTS, DRY, BOOST, CONFIG.
5. Static check and (only when approved) manual CircleCI run_build=true. Do not merge or build implicitly.
