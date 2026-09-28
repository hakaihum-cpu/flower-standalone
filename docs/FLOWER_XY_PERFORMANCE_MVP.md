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

Controls exposed outside the pad:
- root note,
- scale,
- BPM,
- HOLD,
- STOP.

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

Animation is intentionally absent.

The screen contains:
- FLOWER header,
- one large touch XY pad,
- eight vertical arp-algorithm zones,
- simple effect-depth indication,
- root / scale / BPM controls,
- HOLD and STOP.

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
- static diff review.

Not yet performed:
- Android APK build,
- device audio/touch test,
- tuning of musical mappings after listening.

A build should be run only after static review is complete because unnecessary CI/GitHub build minutes are explicitly avoided.
