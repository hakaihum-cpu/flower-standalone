# EFFECTS — MOOD MKII mode (2026-10-31)
## Scope and status
Feature branch created from approved `baseline/effects-golden-20261020-libpd-patch`. Existing mode IDs 0..9 are unchanged; MOOD MKII is mode 10. **Unbuilt, untested on Android**; no claim of circuit-level or DSP-algorithm equivalence.
Official MIDI manual: https://static1.squarespace.com/static/689b53dbdfec6d3f10536eae/t/68ccf358f11808289539219e/1758262104480/Chase-Bliss-MOODMKII_MIDIManual_JP.pdf

## Implemented
- MOOD mode 10 is an **external MIDI-only controller**. It produces no local audio, no DSP, no DRY monitor and applies no INPUT GAIN. These changes affect only MOOD mode.
- CAPTURE toggles the hardware Micro-Looper bypass on the selected MIDI OUT port using CC102 (value 127/0). The pedal records continuously when bypassed; there is no separate MIDI capture command. Button shows CAPTURE / LOOP OFF.
- The MOOD keyboard starts at C4-C5 (MIDI 60-72). OCT- / OCT+ beside LOAD TILES select C-1 to C9, allowing every standard MIDI note 0..127 across the pages. In the final page, notes above 127 are visibly disabled and do not transmit. Held notes are released before changing pages. Each playable key displays its MIDI note number; external MIDI is passed through when MIDI CONTROL is enabled.
- X/Y touch movement emits the user-selected MIDI CCs with values 0..127. Labels can be tapped to select among CC14–23, CC58, CC80–84 (existing choices retained). Help is visible beneath the controller.
- Both MOOD MIDI reception and UI-generated output use the shared MIDI CH setting in CONFIG > MIDI SETTINGS. MIDI CONTROL must be ON and MIDI OUT selected for external commands.
- The 343 user-provided JPEG frames are bundled into the APK through the existing CircleCI materializer and JUCE resource system. Images change only when the input-onset, MIDI note or XY change conditions are met; there is no fixed frame timer.
- All prior effect modes and audio DSP paths remain unchanged.

## Not implemented / incomplete
- The official MOOD MKII MIDI manual describes Synth Mode transposition and a separate octave transpose CC57, but does not specify a bounded supported Note On range. The screen OCT buttons change the actual Note On pitch, not pedal CC57; MIDI 0..127 is the standard addressable range, not a claim of verified pedal response at every pitch.
- The MOOD MIDI mode does **not** emulate the hardware signal path. No locally recorded loop is created. It cannot infer the pedal's LED color, MIDI-port connection or physical state from one-way MIDI OUT.
- MIDI CC58 (Output Type) follows the official main MIDI table; its conflict with a later CC59 caption remains subject to hardware verification.
- Android APK build, hardware/LED test, and end-to-end MIDI OUT verification have not been performed. The branch must pass the existing manual CircleCI pipeline before an APK is usable.
- The MIDI output is controlled by the existing device and enable settings; no separate connection mechanism is introduced.

## Asset format
`mood_frames.pack` header `CRF1`, little-endian uint32 count(343), 343 entries uint64 offset + uint32 JPEG length, then JPEG bytes, exactly as read by `Source/FramePack.cpp`.
Frame order: source image 1 rows 0..9, columns 0..9, then source image 2, 3, 4. Each tile is a square crop (grid border excluded) resized to 150×150 pixels; photos are user-supplied and remain black and white.

## Release gate
1. Verify each CC and actual sound effect; compare native note-on/off and UI keyboard; choose independently from incoming MIDI.
2. Verify automatic loading of 343 bundled frames, random transitions and DOT mode on Android 12/RG rotate.
3. Confirm no clicks, excessive xrun, feedback runaway, frozen capture glitch, dry/wet multiplication.
4. Check UI 720×720, menu selection all 13 entries, transparency and no overlap with EFFECTS, DRY, BOOST, CONFIG.
5. Static check and (only when approved) manual CircleCI run_build=true. Do not merge or build implicitly.
