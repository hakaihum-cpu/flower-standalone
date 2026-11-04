# MIYAKO FM-1 port — feature contract and progress (2026-11-04)
Repository: hakaihum-cpu/Vstplugin
Branch: feature/fm1-miyako-port-20261104
Original MIYAKO development baseline: feature/miyako-ep-sampler-16part-20261030
Target: M-VAVE FM-1 with 240 x 240 display.
Do not modify the Android baseline or main branch.

## Pinned native foundation
The first Felucca 1.9.0 / ABI 1 plugin was a proof-of-concept (`fm1/miyako.c`).
The actual firmware target is now the KARMA source because it already ports all three
requested Mutable DSP engines to FM-1. The plugin-only proof of concept is NOT built.
KARMA source: https://github.com/eatyourgreensstudio-dotcom/karma-fm1
PINNED COMMIT: db3cda79359ae914d1e58c0fc96c6ca744e7950e
KARMA base (GPL-3.0): no source is silently relabeled as full Android MIYAKO.

## Exactly retained from MIYAKO requested scope
- Sine / Square / Saw / Pulse (native MIYAKO oscillator, polyBLEP for non-sine).
- Existing Plaits FM and 6OP A/B/C are retained on track 3. No duplicate
  2-op MIYAKO FM and no new Felucca FM6 core are added, protecting Flash/RAM.
- Reel as a new short fixed-point wow/flutter tape-speed modulation and soft drive
  processor. Moving read heads use 1/256-sample linear interpolation to suppress
  discontinuous delay-tap stepping.
- EUREKA as a new short capture/repeat stutter processor. NOT Android's full ten-second EUREKA.
- Bitcrusher controlled by CRUSH.
- Delay / Reverb / Chorus remain in the KARMA FX paths.
- Real Mutable Plaits-derived DSP remains the PRISM (PLAITS) engine in KARMA.
- Real Mutable Elements-derived exciter DSP remains the ELEMENTS engine in KARMA.
- Real Mutable Clouds granular-player DSP remains the CLOUD send in KARMA.
- MIDI, panel controls, LCD parameter edit pages and sequencer are inherited from KARMA.
- MIYAKO OSC and FX graph strips are yellow DOT-style scenes, contained in 240x124 of the
  FM-1's 240x240 display, with no generated images or extra frame buffers.

## Explicitly excluded from MIYAKO
Vongon, EP-Sampler, Knowing, Tape (MIYAKO Tape FX), Twilight.
Other Mutable engines: Rings, Warps, Marbles, Tides and all except Plaits / Elements / Clouds.
Original Android video data, 720x720 UI, JUCE framework, sample banks.

## Present implementation (not yet complete)
- `fm1/karma/eng_miyako.c` implements a distinct native MIYAKO engine.
- `fm1/karma/apply_port.py` replaces KARMA's ACID engine on track 1 with MIYAKO:
  factory presets, parameter names, engine table and the per-track arena.
- Track 2 remains ELEMENTS; track 3 remains PLAITS; Clouds remains an effect bus.
- The KARMA engine index 14 is reused, so the original ACID preset trims are
  explicitly bypassed rather than incorrectly altering MIYAKO's patch levels.
- REEL and EUREKA are mutually exclusive choices in a single `MODE` parameter.
- The ring buffer holds 4096 x int16 samples, 92.9 milliseconds at 44.1 kHz.
- The previous plugin source `fm1/miyako.c` is obsolete and NOT in the KARMA build.
- 240x124 MIYAKO OSC/FX graph visuals are implemented; entire-system DOT theme is not
  complete. The existing four-knob bands and screen navigation remain unchanged.
- The inherited KARMA Tape texture choice remains at its saved enum index but is renamed "-" and
  forcibly mapped to OFF in the mixer. This removes its sound without renumbering existing projects.
- KARMA still has other inherited engines / textures; their removal and final MIYAKO-only
  navigation are not complete. The separate Reel effect remains intentionally available.
- May require reduction in other existing KARMA features to meet final flash/SRAM budget.
- Earlier host validation was performed on the superseded five-waveform version.
  The revised code has FOUR MIYAKO waveforms; the independent Plaits FM models
  are unchanged. Host tests were updated and must be rerun before any CI build.
- Earlier DOT geometry test covered 22 states before FM removal. The updated
  four-waveform DOT geometry test must be rerun before firmware build.
- Local x86-64 GCC ASan/UBSan randomized audio stress: PASS (20,000 blocks,
  640,000 output samples, rapid waveform/FX parameter changes). All three
  host checks reran successfully after the fractional-delay Reel change.
- No JieLi pi32v2 target firmware build or actual-hardware audio validation has been run.

## CircleCI operational contract
- `.circleci/config.yml` exists only in the feature branch.
- The pipeline parameter `run_build` defaults to false.
- Trigger ONLY by user's explicit build approval, `run_build=true`.
- Job must verify the exact branch and pinned KARMA commit before compiling.
- In CI: `python3 fm1/check_contract.py`, patch pinned source with
  `python3 fm1/karma/apply_port.py fm1-karma`, validate with
  `python3 fm1/karma/verify_port.py fm1-karma`, then run its normal build.
- Artifacts: `MIYAKO-FM1-PREVIEW.fwsc` with SHA256SUMS. This is a preview,
  not a claim of final firmware.
- No CI executions are authorized or performed by source commits.
- Keep main protected. Report CircleCI pipeline and branch when run.
- No physical FM-1 flashing until backup, compatible flash type and recovery path confirmed.

## Next gates
1. Pass native KARMA source integration plus JieLi target linker/flash/SRAM checks.
   Host audio and 240x124 DOT geometry tests have passed locally, but full firmware has not.
2. Run deterministic smoke audio renders for MIYAKO and original Plaits/Elements/Clouds.
3. Reduce inherited nonrequired functions and implement MIYAKO dedicated DOT UI.
4. Verify the graphics and all controls on a 240x240 simulator.
5. Test on physical FM-1 only after backups and rescue procedure verification.

## 2026-11-04 additional pre-build integration work
- Removed unused ACID DSP from the native include graph, keeping the ID reserved
  for MIYAKO and preventing stale ACID preset trim from affecting MIYAKO volume.
- The previously added seventh (homebrew FM) factory preset was removed in
  favor of original KARMA Plaits FM BELL / FM BASS / DX BANK A/B/C patches.
- Restored standard KARMA per-voice headroom by removing the erroneous 2x
  gain after voice_amp; this avoids pre-FX peaks above 16-bit full scale on
  a single voice.
- EUREKA's slice DEPTH now latches at the actual next capture-cycle boundary;
  the host smoke test contains an explicit regression check for this behavior.
- Removed the actual TX_TAPE generator code while keeping its persisted enum ID
  reserved and mapping old values to OFF. Reel remains as a separate effect.
- Replaced the KARMA boot logo/animation with text-only MIYAKO yellow DOT
  splash/animation. This adds no generated artwork, image files or bitmap RAM.
- Added sanitizer-capable boot splash geometry/skip test in
  `fm1/karma/test_boot_ui.c`. All four host test programs are CI pre-build gates.
- Local independent C DSP mirror sanitizer test ran successfully for 640,000
  randomized output samples; this is NOT an embedded firmware compile and is
  not a substitute for running the exact branch CI tests.
- Exact pinned KARMA sources: 19 integration anchors verified uniquely using
  GitHub source inspection; target link still unexecuted.

## 2026-11-04 — FM architecture decision
- Compared M-VAVE stock 32-algorithm FM, Felucca's separate FM6, and KARMA's
  Plaits FM/6OP models in its exact pinned source.
- KARMA intentionally leaves the original Felucca FM6 engine slot disabled.
  Enabling it would require additional engine state/CPU/code and firmware-space
  retesting; the original factory FM algorithms are not callable as a linked
  ROM function from the current codebase.
- KARMA already compiles and exposes true Plaits FM, 6OP A/B/C models on
  track 3, with preset names FM BELL, FM BASS and DX BANK A/B/C.
- Decision: remove the redundant 2-op oscillator and its INDEX descriptor,
  factory patch, DOT graph case and redundant smoke-test assumptions. Keep
  track 1 focused on MIYAKO basic waveforms/FX and track 3 on existing Plaits FM.
- Hardware UI: ALGORITHM selects track 3; PRESETS chooses the included FM patch;
  EDIT tweaks native HARM/TIMB/MRPH. Physical encoders only; no touch panel.
- Do not claim Plaits 6OP banks provide the original FM-1's 32 algorithms.
- No hidden compatibility: custom MIYAKO Reel/Eureka processors are currently
  per-engine effects on track 1, not a global bus available to Plaits FM on
  track 3. This routing remains unimplemented.
- Source updated, but target firmware has not been compiled or flashed.

## 2026-11-04 — Real-time EUREKA note-off regression
- Inspected the actual KARMA audio call graph: `felucca.c` includes
  `engines.c`, `voice.c`, then `fx.c`; `voice.c::track_render()` calls
  `engine_t.post` before `fx.c::mix_part_run()` decides whether an
  otherwise-silent track may be skipped.
- Discovered a runtime correctness gap: KARMA's default 16-block (or INSERT
  64-block) tail does not cover the longest short-EUREKA capture/repeat cycles.
- Added `miyako_slot_t.tail_left` and `tail_block` to keep a bounded replay
  alive for at most one complete cycle after the input falls silent. The
  final processed block remains eligible for mixing before the engine idles.
- Added a branch-local `miyako_tail_active(track_t*)` hook. The FX mixer checks
  this before bypassing a silent synth part. Only track 1 running the real
  MIYAKO engine may return busy. Existing Plaits, Elements, Clouds, drums,
  stereo routing, and KARMA's second core are otherwise unchanged.
- Extended the host regression in `test_engine.c`: capture start, note-off,
  exact replay countdown, the last processed block, next-block idle, and
  non-MIYAKO track isolation. Post-patch verification asserts the precise
  FX mixer hook exists.
- Removed literal backslash-n characters accidentally embedded into a C
  comment during the earlier FM removal. The previous preflight overlooked
  this invalid C; it now explicitly rejects any such source text.
- These are source commits, **not** a successful target compiler link or a
  claim that all original 10.2-second Android EUREKA behavior is reproduced.
- Estimated standalone MIYAKO ring-buffer state grows by one 32-bit
  countdown and a tail-byte flag; the linker must still validate actual
  union sizing and free `.pool` capacity against KARMA's limits.
- Important: applying MIYAKO Reel/EUREKA to Plaits FM on track 3 remains
  **unimplemented**. KARMA may render track 3 on CPU1, so sharing the
  track 1 effect arena across cores is unsafe. No speculative cross-core
  pointer or overlapping arena has been added.

## 2026-11-04 — Pre-link completion gates and measured host layout
- The exact pinned KARMA source was checked as a whole against all
  **27 sequential replacement operations**; no mismatches. A separate
  pre-mutation check confirmed those 27 old anchors each occur once in
  the unmodified upstream, so no partial source mutation is needed if
  a future pinned source fails contract checks.
- `fm1/karma/apply_port.py` now parses its own AST to test **every**
  source replacement before copying or editing any target file. The
  giant boot-animation replacement is included in this atomic preflight.
- Actual KARMA code was simulated with those 27 replacements; an
  additional 12 integration checks for actual Plaits/Elements/Clouds
  DSP includes, physical controls, EUREKA's idle-tail hook, Tape
  removal, boot/DOT UI, and versioning all passed.
- The current engine uses a 4096 x int16 EUREKA buffer. A local
  C ABI layout check of the equivalent structures reports
  `sizeof(juno_slot_t)=2456`, `sizeof(miyako_slot_t)=8228` on
  the x86-64 host (rough union size growth **5772 bytes**).
  Those are **not** native JieLi target measurements; final slot size,
  `.pool` headroom, and absolute app-image size require the real
  pi32v2 linker.
- The native engine now fails compilation if the buffer loses its
  power-of-two invariant or the `miyako_slot_t` state grows above
  8448 bytes. `tools/build.py` independently checks application
  Flash slot size, RAM region and at least 8192 bytes of pool headroom.
- `verify_port.py` additionally checks the real physical
  SELECT, PRESETS, ALGORITHM, KNOB1–4 handlers and the FX/EDIT remapping.
- CircleCI uses an isolated Python virtual environment with pinned
  Pillow 10.4.0 to avoid Linux externally-managed Python package errors.
- The firmware cross-build and physical FM-1 audio/UI validation
  have **not** been run in this stage. The job remains gated behind
  explicit `run_build=true` permission and restricted to this branch.
