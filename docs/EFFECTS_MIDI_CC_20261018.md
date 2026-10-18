# EFFECTS MIDI CC mapping (2026-10-18)

This document describes the app's MIDI **input** controls. Set MIDI CH in
MIDI CONTROL. All controls below use 7-bit values 0..127.

## Common (all modes)
- CC110: XY X
- CC111: XY Y
- CC112: MODE (9 modes)
- CC113: CHORD-A/B
- CC114: RUN ON/OFF (>=64 = ON)
- CC115: global WET
- CC116: BOOST
- CC117: INPUT GAIN
- CC118: DISPLAY MEDIA/DOT
- CC119: global DRY (default 0; no DRY unless explicitly raised)

## CHORD-A and CHORD-B
- CC20 COMPLEX; CC21 BAR; CC22 WIDTH; CC23 LENGTH
- CC24 HOLD; CC25 EFFECT (CHORD-B RANDOM FX probability)
- CC26 CLOCK MODE; CC27 BPM
- **CHORD-B only**: CC65 RANDOM FX SELECT:
  bucket 0 = RANDOM (CC 0..15), 1 SHORT DELAY, 2 LONG DELAY,
  3 TAPE DELAY, 4 TAPE SIM, 5 BITCRUSH, 6 CHORUS, 7 REVERB.
  Buckets are `floor(value * 8 / 128)`, fixed at 0..7.
  Random mode resumes the lottery on the next note.
- **CHORD-B only**: CC66 EFFECT probability, CC67 BPM, CC68 LENGTH.
  CC65 changes an active effect with the existing ~6ms transition.

## DREAMY
- CC60: XY X (same DSP XY axis as CC110)
- CC61: XY Y (same DSP XY axis as CC111)
- CC62: ambience/delay/reverb depth override (0..127).
  If CC62 has never been sent, the approved X*Y behaviour is unchanged.
- CC63: BPM for existing rhythmic loop duration.
- Common CC115 (WET), CC119 (DRY), and CC114 (RUN) also apply.
  DREAMY's two pitch voices have no independent existing user parameters;
  MIDI assignment does not add new synthesizer controls.

## EUREKA
- CC40..56: existing 17 EUREKA parameters
- CC110/111: EUREKA XY mapped to repeat/time/reverb

## CHORDBOT
- CC120: select one of the nine pads.
- CC121 >=64: release pad.

## CLOUDS
CC70 POSITION; 71 SIZE; 72 DENSITY; 73 TEXTURE; 74 PITCH (-12..+12 st);
75 SPREAD; 76 FEEDBACK; 77 REVERB; 78 CLOUDS INTERNAL MIX;
79 FREEZE (>=64 ON); 80 TRIGGER (0->>=64 rising edge);
81 GATE (>=64 ON). CC110/111 remain the approved performance XY controls.

Before a given individual CC has been received, its parameter continues to
follow the original XY curve. Once a CC is sent, that parameter follows its CC
until the app starts fresh. CC79/81 override the XY/touch freeze/gate for that
session. CLOUDS global output remains WET-only by default (global DRY=0).

## Vongon-inspired (unchanged)
- ULTRASHEER: CC100..109
- ROSETONES: CC13, 14, 80..87, 90
- SOLARFAZE: CC13, 14, 80..98
- POLYPHRASE: CC1..11, 13..17

## Build safety
- Source branch: `feature/effects-complete-midi-cc-20261018`
- Unchanged CircleCI: project `androidapp`,
  Config Source Branch = Checkout Source Branch,
  `manual_android_build`, `run_build=true` manually.
- No GitHub Actions. No automatic builds on push.
- Original immutable Golden baseline:
  `baseline/effects-golden-20261017-rosetones-eureka11`
