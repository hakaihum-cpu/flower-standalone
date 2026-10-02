# UI Specification

Design space: 720 x 720 logical square, scaled to actual display bounds.

## Main screen
- Fullscreen current supplied frame.
- No decorative knobs.
- Bottom readability gradient only.
- Thin bars/text for COMPLEX, BAR, WIDTH, LENGTH.
- Status text: detected note and current chord.
- Small `CONFIG` text at top-right.
- Running only: 3px red border inset 8px and `● REC` at top-left.

## Touch
- Main image/background: REC toggle.
- Bottom four parameter cells: drag horizontally.
- CONFIG label: opens config.
- Parameter interaction never toggles REC.

## CONFIG screen
Same current supplied frame behind a dark translucent layer.
- Audio input names as plain text/radio marks.
- iRig detection text plus the Android physical input product name when available.
- Thin input meter.
- MIDI CH text row.
- CLOCK text row.
- BPM row only in Internal mode.
- CLOSE text; no ornamental panel/knob styling.

## MIDI controller mode
When MIDI CONTROL is ON:
- The main image field is the XY surface.
- Top-left becomes the dedicated REC toggle so XY touches never toggle REC.
- A small X/Y numeric readout is shown; no decorative panel is added.
- CONFIG -> MIDI SETTINGS opens a second text-only configuration page.
- Rows: ENABLE, MIDI OUT, MIDI CH, X MODE, X CC, Y MODE, Y CC, KEY, SCALE, PRESET, LOAD, SAVE.
- X/Y modes: CC / NOTE / CLOCK.
- Coordinate-to-frame mapping is 20 columns x 15 rows over the exact supplied 01..300 frames.


## Motion REC controls
- Physical L1: record / stop-and-play.
- Physical R1: clear.
- MIDI SETTINGS includes MOTION BARS 1..16.
- Main screen shows MOTION REC while recording and MOTION PLAY while looping.
- Holding L1/R1 must not retrigger repeatedly; actions occur on physical press transitions.


## Chord MIDI output controls
- MIDI SETTINGS adds CHORD OUT and CHORD CH.
- CHORD OUT default is OFF.
- CHORD CH is 1..16 and independent from the XY MIDI CH.
- MIDI OUT remains the common destination device for XY/Motion and generated-chord MIDI.
- No new panel or decorative control is introduced; the existing text-row style is retained.
