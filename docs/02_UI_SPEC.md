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