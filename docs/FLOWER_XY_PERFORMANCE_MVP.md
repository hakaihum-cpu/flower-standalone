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


## Physical-control contract (2026-09-28 combined revision)

- D-pad: moves the same XY coordinates used by touch. A single temporary pointer is shown only while a D-pad direction is held.
- D-pad release: pointer disappears, but the last XY sound remains latched.
- Screen tap: explicit performance stop. A touch drag/swipe takes over the XY surface; a simple tap stops.
- L / R: BPM down / up. Immediate 2 BPM step, then hold-repeat after 350 ms at roughly 80 ms intervals. Range 50-200 BPM.
- B: ARP ON/OFF. ARP OFF holds one mono root note while the performance gate is open.
- A: Delay ON/OFF with the existing delay smoothing retained.
- Y: Granular ON/OFF.
- X short press: looper transport EMPTY->REC->STOP->OVERDUB->STOP->OVERDUB...
- X long press (800 ms): CLEAR.
- X position no longer changes arp subdivision; arp speed is controlled by BPM only. The internal subdivision is fixed at 2 steps/beat.

## Visual framing revision

The selected source tile now fills the 720x720 canvas with no letterbox. A small top-aligned zoom is used so the dark band does not appear and the face sits closer to screen centre. The source image itself remains the corrected user-specified face-closeup contact sheet with no extra low-quality re-encode.


## Visual bank correction: full 100 cells / full face

- The supplied sheet is treated strictly as 10 columns x 10 rows = 100 cells.
- Every cell index 0-99 is reachable; there is no exclusion/skip list.
- Cell order is row-major: `index = row * 10 + column`.
- Only the 2 px separator border is removed from each cell.
- The photograph itself is not square-cropped or zoom-cropped.
- The complete tile keeps its original aspect ratio and is centred in 720x720.
- The square margin is filled by extending the tile's edge pixels instead of black letterbox bars.
- This prioritises showing the complete face/head, matching the user's reference tile.


## CONFIG screen

SELECT opens/closes a dedicated 720 x 720 CONFIG screen.

Settings:
- ROOT KEY: C / C# / D / D# / E / F / F# / G / G# / A / A# / B
- SCALE: MINOR PENT / NATURAL MINOR / MAJOR / DORIAN / RANDOM
- DEFAULT EFFECT: OFF / ON

RANDOM scale chooses a chromatic semitone (0-11 relative to ROOT) independently for each generated step.

DEFAULT EFFECT controls the startup/default state of both DELAY and GRANULAR. During performance, A and Y can still toggle DELAY and GRANULAR independently without rewriting the stored default.

CONFIG interaction:
- SELECT: open/close
- D-pad UP/DOWN: select setting
- D-pad LEFT/RIGHT: change value
- A: advance/toggle selected value
- Touch: tap left/right half of ROOT/SCALE rows to decrement/increment; tap DEFAULT EFFECT to toggle.

ROOT, SCALE and DEFAULT EFFECT are stored as APVTS parameters so the JUCE standalone state save/restore path retains them across application sessions.


## CURRENT VISUAL SOURCE CONTRACT — supersedes earlier visual experiments

The runtime now uses the exact user-supplied 1191 x 896 contact sheet directly. The previously generated square atlas and all base64/materializer intermediates have been removed.

The sheet is **not** an evenly spaced 10 x 10 grid vertically. Runtime therefore uses explicit source separator coordinates instead of arithmetic division:

- X starts: 0, 121, 241, 361, 481, 601, 721, 841, 961, 1081
- X ends: 118, 238, 358, 478, 598, 718, 838, 958, 1078, 1191
- Y starts: 0, 86, 175, 264, 355, 449, 544, 641, 733, 812
- Y ends: 84, 172, 261, 353, 447, 542, 638, 730, 809, 896

Frame 0 is therefore exactly source rectangle `x=[0,118), y=[0,84)`. Pixel inspection confirmed that this crop is identical to the user's supplied single-frame reference image.

Each selected source rectangle is drawn directly to the 720 x 720 display. There is no square pre-crop, inset, zoom, cover, letterbox compensation, edge extension, or intermediate atlas. All 100 frame indices remain reachable.
