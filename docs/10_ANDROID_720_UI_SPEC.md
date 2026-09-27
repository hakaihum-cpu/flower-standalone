# Flower Standalone Android 720 x 720 UI Specification

Jira: AN-22

GoldenMaster base:
`df2378f584f155349c33fea5e7ae212a29027049`

Target:
- Android editor: exactly 720 x 720 logical pixels.
- Non-resizable on Android.
- Desktop layout remains separate and is not reflowed by this specification.
- This is a Flower Standalone requirement; it does not import MIYAKO Compact 720, RG Rotate or any MIYAKO page/control system.

## Flower page geometry

Root editor:
- 720 x 720.

Flower panel:
- x=8, y=8, width=704, height=704.

Panel content inset:
- 14 px on every side.
- usable content: 676 x 676.

Header:
- height 48.
- right-side controls remain FLOWER / REVERSE / CLEAR / SYNTH.
- touch widths before inset reduction: FLOWER 82, REVERSE 92, CLEAR 74, SYNTH 78.
- the left header area remains available for the existing FLOWER / LOOPER-GRANULAR title painting.

Main visual stack:
- 6 px gap after header.
- animation allocation: 676 x 336; component inset 2 px -> 672 x 332.
- the animation renderer keeps its fixed 16:9 stage inside that component.
- 6 px gap.
- waveform allocation: 676 x 84; component inset 2 px -> 672 x 80.
- 8 px gap.

Flower controls:
- remaining height: 188 px.
- horizontal inset: 2 px -> width 672.
- two rows, 94 px each.
- four equal columns per row, 168 px each before per-control inset.
- each knob component uses 5 px horizontal / 3 px vertical cell inset.
- row 1: POSITION / SIZE / DENSITY / SPREAD.
- row 2: HOLD / PITCH / MIX / FEEDBACK.

Android knob readability:
- label font: 11 px bold.
- label area: 16 px.
- value box: 72 x 20.
- desktop knob metrics remain unchanged.

## Synth page geometry

Synth panel:
- same x=8, y=8, width=704, height=704 as Flower panel.

Header:
- 48 px.
- CLOSE on the right, 78 px allocation before inset.

Content:
- 8 px gap after header.
- bottom keyboard allocation: 118 px; keyboard component inset 4 px horizontal / 6 px vertical.
- 8 px gap above keyboard.
- remaining control area: 494 px.
- three rows: 164 / 164 / 166 px.
- first two rows use three equal control columns.
- third row contains RESONANCE / LFO RATE / LFO DEPTH+TARGET.

## Protected behavior

AN-22 must not alter:
- GoldenMaster Android startup/audio reinitialisation,
- stb-based Android PNG decode path,
- deferred visual load guard,
- Sine/ADSR/filter/LFO DSP,
- Flower rolling 16-second capture,
- four-grain Flower DSP,
- Actor v3 simulation/variation,
- embedded visual resources,
- APVTS parameter IDs/state,
- CircleCI build topology.

## Static verification before build

Required:
- fixed 720 x 720 constructor markers remain present,
- Android square-layout markers match this document,
- desktop `resized()` branch remains present,
- no detached PNG worker path is reintroduced,
- C++ brace/preprocessor balance remains valid,
- static dependency audit is updated together with any intentional AN-22 layout change.

A CI build is not a substitute for these checks.
