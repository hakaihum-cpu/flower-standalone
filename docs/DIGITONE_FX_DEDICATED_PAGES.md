# Digitone one-of-four audio FX — dedicated native parameter pages

Status 2026-11-03: SOURCE-ONLY PAGE IMPLEMENTATION. Not built or loaded into a Digitone. Baseline PG79 remains unchanged. The new C page renderer is deliberately not registered in any existing mod.json and does not create unusable FX menu items. This preserves stock audio and the MC78 mute fix until all audio backends and the exclusive selector are ready.

Branch: feature/digitone-fx-dedicated-pages-20261103
Files: ports/digitone_fx_pages/digitone_fx_pages.h and digitone_fx_pages.c

## Architecture

- TRACK hold opens the existing core-dn1 Mod Menu. Planned choices: GRANULAR, EUREKA, REEL, BITCRUSH, and OFF. Only one audio insert owner may be active; changing a choice silences the prior FX wet path and uses a click-safe transition. Never run parallel core_audio owners.
- Original Digitone FM, internal sequencer, stock master effects and MUTE must remain untouched.
- The MIDI+YES visual toggle and 576-frame EUREKA animation are independent of the selected audio effect.
- Native 128x64 one-bit page: tempo/title band, narrow left gutter, 2 rows x 4 actual A-H encoders, circle with position needle (continuous) or value box (option), small labels. Unlike PG79's four oversized bordered squares, never show an inert knob as editable.
- The parameter page appears only for the active audio effect; leaving it returns to stock/visual view. A page may be displayed by a future rack dispatcher; no stock page button is currently stolen.
- Per-effect independent state is required. Keep 4 dnfx_state objects in the rack. Each panel displays exactly the state consumed by its DSP, with bidirectional update on MIDI CC.
- UI uses no new images or video.

## Mapping: four distinct UIs

### GRANULAR — one page (five working parameters)
A PITA: pitch voice A, eight choices -12,-7,-5,0,+5,+7,+12,+24 semitones; stock Granular CC33.
B PITB: pitch voice B, same choices; CC34.
C SIZE: 120..360 ms, 5-ms steps; CC36.
D POS: 0..62.5 ms look-back offset; CC37.
E MIX: dry/wet 0..100%; CC60.
F-H unassigned, not clickable and not shown. No new granular DSP controls are invented.

### EUREKA AUDIO — two pages (sixteen HazeProcessor parameters)
SOUND 1/2:
A MIX, B TIME, C HAZE, D FILT, E REPT, F MOD, G VERB: normalized 0..127.
H SPED: 0.5x, 1x, 2x.
PLAY 2/2:
A LOOP: 1,2,2+; B WARB: OFF/LIGHT/HEAVY; C TRNS: transpose;
D ECHO: record-echo; E OG: deliberately degraded mode; F LOCK: hold buffer;
G GAIN: high input gain; H STEO: mono or stereo input path.
The original MIYAKO/Haze 'bypass' parameter is deliberately omitted because the rack's OFF/selection handles bypass. Audio port pending: do not expose EUREKA AUDIO menu until its DSP exists. These pages are separate from the six EUREKA VIDEO pages in PG79.

### REEL — one page (exact five MIYAKO MasterFx controls)
A DRIV: saturation; B WOW: slow pitch drift; C FLUT: fast pitch drift;
D NOIS: tape hiss; E WET: effect blend. F-H intentionally unused.
Ranges 0..127 map to source floating 0..1. Audio port pending.

### BITCRUSH — one page
A BITS: 4..16 bits; B RATE: 1..32 sample hold factor;
C MIX: dry/wet; D OUT: output compensation. E-H unused.
No unrequested reverb, distortion or random modulation.

## Shared UI contracts

- The front panel values never overwrite hidden original Digitone FM parameters.
- A-H encoder press/turn events are owned only while the active FX parameter page is visible.
- No FX page may own TRIG 1..16, FUNC+BANK MUTE, MIDI+YES visual toggle or the stock transport.
- The page renderer reads one state object but does not write audio. The rack must call dnfx_edit, publish validated parameter changes to the chosen DSP, and call dnfx_draw after PG79's video draw when the corresponding FX screen is visible.
- Native tests before an authorized build must verify all 4 mode descriptors, EUREKA two-page navigation, every range and default, pitch and grain mapping, null/unassigned controls, and that stock key passthrough remains exact.
- Additional CPU/bulk-memory/lint verification is mandatory before an authorized PG80 successor SYX. No build initiated in this work.
