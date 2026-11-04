# MIYAKO FM-1 — UI and physical-control coverage audit
Date: 2026-11-04
Branch: `feature/fm1-miyako-port-20261104`
Baseline: pinned KARMA `db3cda79359ae914d1e58c0fc96c6ca744e7950e`.
**Result: PARTIAL / NOT CERTIFIED — do not claim complete control coverage or zero contradictions.**

## Audited source-level routes
| Physical control | Target/route | Current source result |
|---|---|---|
| ALGORITHM encoder | track selection 1–4 | Native KARMA `panel_enc(EN_ALGO)` -> `track_select` present |
| PRESETS encoder | preset browse on HOME/BROWSE/TRACKS | Native KARMA guarded `panel_enc(EN_PRESET)` present |
| SELECT encoder | family pages / tempo fallback | Native KARMA `page_walk()` / tempo path present |
| KNOB1–4 | page-local engine params | Native `page_desc()` + `track_desc()` and editor present |
| EDIT tap | MIYAKO OSC -> FX engine page -> VOICE | `mi_panel_edit_tap()` overlay present |
| FX tap | MIYAKO FX -> shared FX/CLOUD family | `mi_panel_fx_tap()` overlay present |
| FX LED | active MIYAKO edit FX page | `cur_btn()` returns `B_FX` on engine page P_E4 |
| HOME, ENV, LFO, SEL, GLO, SAVE, ARP, SEQ, PLAY, REC, OCT-/OCT+ | retained KARMA routes and held layers | Source preserved, NOT an exhaustive behavioral/long-press test |
| MASTER analog pot | volume | Native hardware scanning not modified; physical behavior unverified |
| 27 keys | playing / sequence input | Native paths preserved; behavior unverified |

## Four confirmed inconsistencies, now corrected in the feature branch
1. Reel MODE=REEL: previous K4 label RATE was false; P_E7
   controls fast/flutter **amplitude**. New K3/K4 labels: WOW / FLTR.
2. Eureka MODE=EUREKA: previous K3 DEPTH was false; P_E6
   selects **capture slice length**. New label SIZE.
3. Eureka MODE=EUREKA: previous K4 RATE was false; P_E7
   selects **1–4 playback repetitions**. New label REPS (the
   underlying persisted parameter is still 0..127).
4. OSC P_E1 WIDTH affects PULSE only. In other modes it stores
   a future Pulse width value. New contextual label PW* outside
   Pulse, with an explanatory note in `UI_PANEL_CONTRACT.md`.
5. Separately, Eureka DOT animation incorrectly showed 1–8 bars,
   while DSP allows 1–4 repeats. Now `1 + (rate >> 5)` in both.

These corrections use the existing KARMA `engine_t.desc` callback;
physical knob scanning is unchanged. Regression assertions are included
in `test_engine.c`, `verify_port.py`, and `check_contract.py`.

## Remaining blockers before a full UI pass
- **No target compile** after dynamic-label changes. C ABI and
  integration must be checked by the native JieLi toolchain.
- **No complete real-KARMA input state-machine test**. Current
  `test_panel_nav.c` uses a simulated six-entry page table; it
  cannot prove every actual page, button hold, key combination,
  LED timing or track-switch transition.
- No actual FM-1 LCD visual inspection. Pixel clipping, legibility,
  bottom four-knob readouts, focus/zoom overlay and visualizer
  transitions must be verified using the real display.
- Missing exhaustive exercise of HOME/long-HOME, held FX, SELECT
  extremes, presets while editing, song save/reload, project loading,
  sequencer step editing, MIDI mode switching, drum context and octaves.
- No real-hardware test that all 7 encoders and 14 buttons preserve
  their native timing and layer semantics.
- Reel/Eureka are track 1 effects only. There is no global routing
  from track 3 PLAITS FM to these processors. Do not present the
  shared FX pages as proof of that routing.

## Decision
The static page wiring is plausible and major parameter-label
contradictions have been corrected, but UI **completeness and full
physical-control consistency remain UNVERIFIED**. Do not sign off
this item for a final release. No CircleCI build was triggered.
