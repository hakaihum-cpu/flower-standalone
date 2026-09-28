# FLOWER XY PERFORMANCE MVP

Updated: 2026-09-28

## Purpose

Prototype a new FLOWER direction without character animation.

The app is a touch-first:
- synth,
- arpeggiator / sequencer,
- granular processor,
- delay,
- filter,

controlled primarily from one large XY performance pad.

This branch starts from the fixed Android Golden baseline commit:

`df2378f584f155349c33fea5e7ae212a29027049`

Branch:

`feature/flower-xy-performance-mvp`

Golden/main are not modified.

## Interaction

### X axis: sequence algorithm

Eight zones:

1. SINGLE
2. UP
3. DOWN
4. UP/DOWN
5. SKIP
6. OCTAVE
7. RANDOM
8. CHAOS

Moving farther right also increases note-step subdivision from quarter-note territory toward faster divisions.

### Y axis: compound sound transformation

Moving upward simultaneously increases:
- synth filter cutoff,
- filter resonance,
- granular density,
- granular spread,
- granular wet mix,
- granular repeat/hold,
- delay feedback,
- delay wet mix.

The intent is not one-control-per-parameter. Each XY position represents a complete musical state.

### Swipe velocity

Faster gesture:
- shortens granular grain size,
- shortens delay time.

### Swipe direction

A fast leftward swipe temporarily reverses the granular playback direction.

### HOLD

HOLD latches the most recent touched XY state after finger release.
It does not start the engine by itself before the pad has been touched.

### STOP

Immediately unlatches the pad and schedules an internal note-off.

## Musical source

The MVP uses the existing FLOWER standalone sine synth voice.

There are no visible controls outside the pad. Root, scale, BPM, HOLD and STOP are assigned to physical-key input only in this MVP.

Scales:
- minor pentatonic,
- natural minor,
- major,
- dorian.

## Audio order

Internal arpeggiator MIDI
→ existing synth
→ existing FLOWER granular core
→ new performance delay
→ output

The existing FLOWER 16-second rolling audio buffer is reused for granulation.

## UI

The entire 720 × 720 screen remains the XY touch surface. There are **no on-screen buttons, knobs, combo boxes or footer controls**.

### XY contact-sheet visual (first implementation)

The first supplied contact sheet is used as a 10 × 10 visual bank.

- one sheet cell = one static visual state,
- X selects one of 10 columns,
- Y selects one of 10 rows,
- Y is inverted for image coordinates so the top of the touch surface selects the top sheet row,
- moving across a cell boundary hard-switches to the adjacent tile,
- the selected tile fills the 720 × 720 display,
- each tile is centre-cropped to square rather than stretched,
- separator lines in the supplied contact sheet are excluded with a small source inset,
- audio XY and visual XY use the same `xValue / yValue`,
- crossfade/interpolation and multiple visual banks are intentionally not part of this first implementation.

The former grid, zone labels, cursor graphics and HOLD text are not drawn over the photograph. If the embedded sheet cannot be decoded, the UI shows `VISUAL ASSET ERROR`.

The supplied JPEG is tracked as deterministic base64 parts and reconstructed by `scripts/materialize_xy_visual_asset.py` before Projucer runs in CircleCI. The reconstructed binary is SHA-256 checked before it is embedded as JUCE BinaryData.

Physical-key defaults:
- LEFT / RIGHT: root note - / + 1 semitone
- UP / DOWN: BPM - / + 2
- S: cycle scale
- 1 / 2 / 3 / 4: select minor pentatonic / natural minor / major / dorian
- H: HOLD toggle
- SPACE or ESC: STOP / panic

The key mapping is implementation-level and can later be redirected to dedicated hardware buttons or MIDI control without changing the touch UI.

The MVP uses a fixed **720 × 720** canvas as the primary design target on both Android and desktop preview.

## Current validation state

Completed:
- dedicated branch from Golden,
- UI implementation,
- internal arpeggiator implementation,
- root/scale/BPM control,
- compound filter/granular/delay mapping,
- gesture-speed mapping,
- swipe-direction reverse mapping,
- HOLD latch behavior,
- STOP behavior,
- static diff review,
- animation source/resources excluded from the MVP build,
- HOLD/touch latch separation verified,
- Android APK built and tested for audio/touch,
- crackle reduction pass tested successfully by the user,
- first 10 × 10 XY contact-sheet visual implemented in source.

Not yet performed:
- Android APK build/test of the new contact-sheet visual,
- final output-level tuning,
- final physical-button mapping,
- tuning of musical mappings after listening.

A build should be run only after static review is complete because unnecessary CI/GitHub build minutes are explicitly avoided.
