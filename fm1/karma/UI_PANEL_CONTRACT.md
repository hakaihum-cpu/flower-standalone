# MIYAKO FM-1 — physical UI contract
Target: M-VAVE FM-1, 240 × 240 RGB565 TFT. NO TOUCHSCREEN.
Baseline: KARMA `db3cda79359ae914d1e58c0fc96c6ca744e7950e`.
Implementation: `mi_panel_nav.c` overlaid into KARMA `ui_input.c`,
`mi_dot_ui.c` into `ui_draw.c`.
Branch: `feature/fm1-miyako-port-20261104`.

## Non-negotiable rules
- Do **not** scale Android's 720×720 touch UI. No touch zones, taps on the LCD,
  drag gestures, virtual buttons or XY touch-pad affordances.
- Every editable sound parameter must be reachable from a **physical** button
  and a **physical** encoder. Parameter names and values stay in KARMA's KNOB
  1–4 bottom band. Do not introduce a screen-only control.
- Only 240×240 pixels are available: 0–21 header, 22–145 central graph,
  146–153 page cue, 154–211 four-encoder band, 212–239 track bar.
  Draw within bounded canvases, not an additional 240×240 framebuffer.
- The hardware's 27 keys still play notes, select steps when in SEQ, and
  use the firmware's existing held-button layers. No attempt to repurpose
  keys as a touchscreen substitute while they are being played.
- The existing hardware scanner, saved calibration, LED engine and
  MASTER volume pot remain unchanged.

## Exact physical controls
| FM-1 physical control | MIYAKO function | Native handling |
|---|---|---|
| MASTER (pot, not encoder) | Main output volume | unchanged hardware path |
| ALGORITHM encoder | Select track: 1 MIYAKO, 2 ELEMENTS, 3 PLAITS, 4 DRUMS | `panel_enc(EN_ALGO)` |
| PRESETS encoder | Browse a track's sounds from HOME/TRACKS or PRESETS screen; no accidental patch reload while editing | `panel_enc(EN_PRESET)` |
| SELECT encoder | Move the visible family's pages in either direction; tempo when page navigation does not apply | `panel_enc(EN_SELECT)`, `page_walk` |
| KNOB 1–4 | Four on-screen parameter/value columns, left to right | `panel_enc(EN_K1 + k)`, `edit_param` |
| EDIT short press | Open OSC from another family; further presses advance MIYAKO FX / VOICE pages | `mi_panel_edit_tap` |
| FX short press, track 1 | Open MIYAKO FX; again open shared FX; further presses cycle native FX (SELECT reverses) | `mi_panel_fx_tap` |
| FX short press, tracks 2–4 | Native FX pages unchanged | `open_family(FAM_FX)` |
| FX held | Native effects performance layer, not a tap | untouched `layers_input` |
| ENV / LFO | Their existing physical pages, envelope and modulation | native KARMA |
| SEL button (printed SEL, internal B_SCL) | Scale/chord/key context | native KARMA |
| GLO | Global/MIDI/mixer pages | native KARMA |
| HOME short press | Tracks/performance page; another HOME on TRACKS may show visualizer | native KARMA |
| HOME held | Hardware/settings menu | native KARMA |
| SAVE | Native presets/user save/project navigation; from TRACKS may open SONG first | native KARMA |
| ARP / SEQ / PLAY / REC | Arp, step entry, transport and recording | native KARMA |
| OCT- / OCT+ | Octave and documented layer modifiers | native KARMA |
| 27 silicone keys | Notes, sequencer input or contextual held-button shortcuts | native KARMA |

**Difference from factory firmware**: ALGORITHM is the KARMA track selector, not
the factory 32-algorithm selection control. No new FM algorithm is embedded into
MIYAKO track 1. The existing PLAITS engine on track 3 owns FM, 6OP A/B/C.
Those Plaits models are not a direct replacement for all 32 factory algorithms.

## Physical navigation: track 1 MIYAKO
- Press HOME, then turn ALGORITHM until track 1 is selected.
- Press EDIT: OSC is displayed, no remembered page surprises.
- Turn KNOB 1 WAVE (SINE/SAW/SQUARE/PULSE), KNOB 2 WIDTH (shown as PW*
  while another waveform is selected: the value is stored for future Pulse
  selection), KNOB 3 SUB, KNOB 4 CRUSH. No redundant 2-op FM is offered
  on track 1.
- For existing FM: turn ALGORITHM to PLAITS (track 3), then use the PRESETS
  encoder from the performance/preset view to choose FM BELL, FM BASS,
  or DX BANK A/B/C. EDIT controls the actual PLAITS HARM/TIMB/MRPH.
- Press FX: MIYAKO FX is displayed. KNOB 1 = OFF / REEL / EUREKA
  (exclusive), KNOB 2 WET. The actual DSP-dependent parameter names are:
  REEL: KNOB 3 WOW (slow modulation depth), KNOB 4 FLTR (fast flutter
  modulation depth; NOT rate). EUREKA: KNOB 3 SIZE (capture-slice length),
  KNOB 4 REPS (1–4 actual repeats; underlying saved value 0–127).
  FX OFF preserves the last K3/K4 settings for subsequent activation.
- Press FX again: native shared FX begins on the first FX page.
  Turn SELECT to access FILTER, SLICER, INSERT, DLY, REV/CHO, CLOUD
  and CLOUD 2. The CLOUD page is the requested real Mutable DSP.
- Further FX short presses cycle native FX pages; SELECT goes backward.
  HOME returns to performance, EDIT from shared FX always opens OSC.
- The physical FX button LED lights for MIYAKO FX even though
  its parameter storage is `FAM_EDIT` page 2. Native FX pages light FX too.
  The EUREKA DOT graph shows 1–4 steps (same `P_E7 >> 5` mapping as
  its actual audio DSP), not the prior incorrect 1–8 bars.
- Touching the TFT does nothing: there are no touch handlers.

## Track and DSP visibility
- Track 1: MIYAKO SINE/SAW/SQUARE/PULSE, Reel/Eureka.
- Track 2: **real ELEMENTS** port from KARMA (native editable pages).
- Track 3: **real PLAITS** port from KARMA (native editable pages).
  This includes FM and 6OP A/B/C, implemented via existing Mutable DSP; no
  extra MIYAKO FM algorithm or independent Felucca FM6 is compiled.
- Shared bus FX: **real CLOUDS** port from KARMA (CLOUD/CLOUD 2 pages).
- Track 4: Native drums, preserved to avoid breaking input, sequencer and
  transport. Its UI is not a Mutable synth.
- Do not claim an independent Clouds instrument track or touch-XY control.
- IMPORTANT: the current custom Reel/Eureka `ENG_MIYAKO.post` runs ONLY on
  track 1. Existing PLAITS FM plays on track 3 and does not automatically
  feed this custom post effect. Mixing Plaits FM through Reel/Eureka requires
  a separately implemented shared-FX routing stage; do not imply it exists.

## 240×240 display
| Vertical pixels | Role | Interaction |
|---|---|---|
| 0–21 | selected track/engine, page, BPM, battery | status only |
| 22–145 | yellow DOT MIYAKO OSC waveform or FX status | no hit targets |
| 146–153 | page cue | status only |
| 154–211 | labels and values for KNOB 1–4 | physical encoders |
| 212–239 | track bar / track status | physical ALGORITHM selects track |

No horizontal scrolling, no Android dropdowns, no full-size 720×720 overlays.
The OSC/FX visualization is purposely small. All other inherited pages use
KARMA's working layout rather than a fake rescaled Android picture.

## Verification and no-build gate
- Compile and run `test_panel_nav.c` (EDIT, FX, CLOUD, HOME, other
  engines and drums), in addition to the audio, memory, DOT and boot tests.
- Confirm the native `ui_input.c` scanner routes SELECT / PRESETS /
  ALGORITHM / KNOB1–4 and physical button LEDs correctly.
- Confirm source overlay matching exactly the pinned upstream text.
- Run linker/flash/RAM and target tests **only** in user-approved
  CircleCI manual `run_build=true`; do not trigger for mere commits.
- The app is a firmware binary, not an APK. Full target build and hardware
  verification remain separate gates.

## Current verification boundaries — NO blanket "UI complete" claim
- The pinned KARMA page metadata and native EDIT/FX routing have been
  inspected. The real physical 7-encoder scanner, layer buttons, and
  selected engine IDs remain in the original KARMA source.
- The `test_panel_nav.c` harness uses a **small simulated PAGES table**
  and stub `open_family()`; its passing assertions CANNOT prove the
  full KARMA page graph, actual long-press modifier combinations, LED
  scan timing, 27-key paths or physical screen rendering.
- The knob-label mismatch (REEL RATE, EUREKA DEPTH/RATE, Pulse-only WIDTH)
  and EUREKA visual 1–8/actual 1–4 mismatch were found by code inspection
  and corrected in `eng_miyako.c` and `mi_dot_ui.c`. These changes
  are still awaiting actual compiler tests.
- Visualizer, project save/reload, track changes during held buttons,
  FX-layer latch/release, effects bypass transitions, all page reachability,
  and live panel operation have NOT been exhaustively tested.
- As a result, this stage is **partial code-level coverage**, not a signed
  full physical UI/input contract.
