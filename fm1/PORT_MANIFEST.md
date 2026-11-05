# MIYAKO FM-1 — authoritative P2 contract (2026-11-05)

**User-defined target:** reproduce MIYAKO's standalone instrument/FX experience
on M-VAVE FM-1 without the KARMA screen, bars, family menus, layers or page
hierarchy. Physical controls must replace touchscreen interaction. The earlier
KARMA-integrated P1 UI and all earlier descriptions of "retained Mutable
engines" are SUPERSEDED; use Git history if their investigations are needed.

## Instrument and feature scope
- Track 1 exclusive: native MIYAKO oscillator SINE, SAW, SQUARE, PULSE;
  WIDTH/PW, SUB oscillator, CRUSH bit reduction and 4-voice polyphony.
- MIYAKO FX: OFF, REEL (WOW/FLTR), EUREKA (SIZE/REPS) as exclusive
  choices; WET adjustable. EUREKA's short bounded 4096-sample ring
  is NOT falsely presented as the Android app's longer recorder.
- Native pitch/key input, amplitude envelope, LFO, arpeggiator, transport,
  sequence controls, BPM, octave, and guarded SAVE operation are surfaced in
  the exclusive full-height MIYAKO interface. All FM-1 knob interaction
  is by real quadrature encoders and mechanical buttons.
- Plaits / PRISM, Elements / PHYS and Clouds have been REMOVED from
  the native C compile inclusion graph. Their numerical engine IDs are
  RESERVED and mapped to existing GONE stubs to avoid renumbering
  persisted project formats. Cloud granular DSP has been replaced by a
  no-output shim retaining the original shared mu-law punch-in ring.
  The extra synth parts remain muted and all front-panel interaction
  is locked to MIYAKO track 1. This change has passed source-level
  checks but target compiler/linker success is still unverified.
- Also out of scope: Vongon, EP-Sampler, Knowing, Tape, Twilight, Rings,
  Warps, the Android 720x720 display and all non-FM-1 touch UI.

## Full 240x240 MIYAKO user interface
- fm1/karma/mi_full_ui.c is the SOLE normal screen renderer and
  normal hardware input handler. The upstream UI entrypoints in ui_draw.c
  and ui_input.c exit after MIYAKO-only handling; neither reaches the
  KARMA draw_head/draw_columns/draw_graph/menus/layers/screens.
- LCD uses two bounded 240x120 RGB565 canvas passes, composing 240x240.
  No KARMA widgets are mixed into MIYAKO. Boot and splash are MIYAKO.
- Mechanical buttons:
  HOME=HOME, EDIT=OSC, FX=FX, SEL=VIDEO, ENV=ENV, LFO=LFO,
  ARP=ARP, SEQ=SEQ, GLO=CONFIG, SAVE=SAVE,
  PLAY=transport, REC=record, OCT-/OCT+=keyboard octave.
- Rotary controls: ALGORITHM=waveform only (NOT other engines);
  PRESETS=6 MIYAKO presets only; SELECT=cycle 10 MIYAKO views;
  KNOB 1..4=the four values actually shown in the current view.
  SAVE opens the SAVE view; next tap arms and one more tap within
  four seconds saves the selected project slot, preventing accidental
  overwrites.
- Views: HOME, OSC, FX, VIDEO, ENV, LFO, ARP, SEQ, CONFIG, SAVE.
  HOME parameters: WAVE/MODE/WET/LEVEL.
  OSC: WAVE/WIDTH/SUB/CRUSH.
  FX and VIDEO: MODE/WET/DEPTH/RATE, dynamically labeled
  WOW/FLTR for REEL and SIZE/REPS for EUREKA.
  ENV: ATK/DEC/SUS/REL.
  LFO: RATE/WAVE/PHS/FADE.
  ARP: MODE/RATE/OCT/GATE.
  SEQ: LEN/DIV/SWG/GATE.
  CONFIG: BPM/SWING/TUNE/CLOCK.
  SAVE: project slot only.
  The PRESETS and ALGORITHM encoders do not accidentally open other DSP.

## EUREKA colour animation — agreed media format
- Original user EUREKA source: EXACTLY 16 clips x 36 frame entries
  = 576 chronological entries. No dropping clips or substituting
  placeholder waves, bars or unrelated art.
- Offline fixed 16-colour RGB565 palette per clip; indexed 4bpp
  128x78 source pixels. Fixed 8x4 tile VQ at 256 dictionary entries
  per clip. Video-only media arrays occupy 250368 bytes.
  It is lossy (VQ adds error beyond 16-colour quantization).
- Pixel rendering uses 1.5x integer mapping into the FM-1 240x124
  visualization at y=49..172; it does not stretch the source.
- The audio-reactive frame advance is updated ONCE per UI tick; each
  of the two LCD rendering passes draws the same chosen source frame.
  First real frame at rest, change on audio dynamics, random clips on
  attacks/end. No fixed-rate playback on silence.
- CircleCI's candidate encoder has an additional RGB PSNR per-frame
  >=26 dB requirement. This is an engineering gate, not proof that
  faces will be acceptably recognizable. A first-36-frame independent
  prototype measured about 25.81 dB for frame 0 and visible facial
  degradation. If the gate fails, firmware build MUST stop.
  No automatic fallback to 1bpp, fewer frames, downscaled replacement
  or cartoon art.

## Build and validation
- Repository hakaihum-cpu/Vstplugin
- Feature branch feature/fm1-miyako-port-20261104 (NEVER main)
- Upstream backend pinned at db3cda79359ae914d1e58c0fc96c6ca744e7950e
- CircleCI MANUAL workflow fm1_manual, job fm1_firmware.
  Pipeline parameter run_build=true is required; default false.
  Never trigger this pipeline on the user's behalf without permission.
- Before the target compiler: check_contract.py, native audio tests,
  native EUREKA CRC/bitmap tests, full media preparation, video-quality
  gate and verify_port.py. App linker must independently fit
  the 581564-byte app slot, including all code and ROM video.
- Expected preview artifacts only after successful pipeline:
  fm1-dist/eureka-vq-preview.png and
  fm1-dist/MIYAKO-FM1-PREVIEW.fwsc with checksum.
- NO CircleCI run or target cross-compile has been performed for this
  P2 UI revision. Full user-video 576-frame quality, full-source C
  integration, real LCD output, sound quality, key/navigation timing,
  persistence and update behavior are NOT YET proven.
  Do not call P2 a finished firmware before those checks pass.

## P2 implementation follow-up: physical LEDs and updater ID (2026-11-05)
- The FM-1's button labeled SEL is upstream's B_SCL enum, not B_SEL.
  All 14 function/octave buttons plus SELECT/ALGORITHM/PRESETS and
  KNOB 1..4 are handled directly by MIYAKO. The 27 playable key LEDs
  and function-key LEDs now use mi_full_leds, bypassing KARMA's page LEDs.
- Physical key scanning/notes remain active; hidden KARMA seq grid modes
  are reset to avoid trapped keys or invisible editing layers.
- M-UPGRADE's CurrentVersion is the actual FM-1_NNN identity, not the
  on-screen FELUCCA_VERSION. Before each authorized CircleCI target
  build, prepare_fwsc_version.py sets PRODUCT to FM-1_(926+CIRCLE_BUILD_NUM),
  starting at FM-1_927 for build #1. Existing P1 was FM-1_926.
  Before shipping FWSC, verify_fwsc_version.py checks the package's
  identity marker, application handshake and update-loader handshake all
  match the new number. test_fwsc_version.py tests #1,#2,#73 and rejects
  #74 rather than reuse an ID outside the updater's FM-1_9xx range.
- The manual CircleCI config has one workflow only; an accidentally
  duplicated tail was deleted and a guard prevents recurrence.
- These are source changes and static integration checks, NOT a completed
  P2 firmware cross-compile or proof of successful installation.

## 2026-11-05 additional preflight fixes
- Removed actual Mutable PRISM, ELEMENTS, PHYS source includes, and
  replaced CLOUD with a no-DSP shim preserving the shared punch ring.
- 38/38 pinned original KARMA replacement anchors match exactly and
  also apply sequentially, checked with a read-only independent parser.
- CircleCI version check was truncated and the SDK step was missing:
  repaired and parsed as valid YAML 2.1 with 10 named jobs steps, one
  manual workflow, correct branch filter and disabled-by-default build.
- Post-patch verification now expects GONE/GONE/MIYAKO engine slots,
  rather than the obsolete PRISM/ELEM arrangement. Do not regress it.
- None of these source/YAML checks implies the real AC79 linker has
  produced a valid 16-colour 576-frame FM-1 FWSC yet.
