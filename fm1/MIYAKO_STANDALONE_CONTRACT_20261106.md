# MIYAKO → M-VAVE FM-1: standalone migration contract
Date: 2026-11-06

## Purpose — non-negotiable
Port **MIYAKO itself** to the FM-1. The deliverable is not KARMA, a KARMA customization, or a KARMA-branded firmware. The user's instruction does **not** authorize using KARMA as the runtime, application framework, UI, or sound engine.

KARMA may be **read as a technical reference only** for device-specific observations. The release must not compile, link, package, dynamically load or execute KARMA source, UI, DSP, binaries or asset data. Reuse of any third-party implementation requires a separate explicit design/licensing review; reading documentation is not permission to bundle code.

## Required migration scope
- Preserve MIYAKO's instrument and FX concept, rather than substitute an unrelated factory/KARMA feature.
- Physical-button/encoder operation on the FM-1's 240x240 screen; no touch controls.
- SINE / SAW / SQUARE / PULSE, SUB, WIDTH and CRUSH; preserve four-voice requirement unless the user changes it.
- Distinct audio FX: EUREKA, REEL, DREAMY, DELAY, REVERB and CHORUS. A new name on a menu is not an implementation. Additional originally approved MIYAKO FX require explicit scope reconciliation, not silent omission.
- EUREKA audio processing **and** 576 original-source animation frames in 16 colour; no generated or substituted media.
- **EUREKA is not the only MIYAKO animation**. Audit the original MIYAKO visual modes and user-supplied media before determining the FM-1 media manifest. Preserve the supported animations and interactions across relevant oscillator/effect modes where original source assets and behavior exist. Do not silently reduce the visualizer to EUREKA.
- VIDEO is a *view*, not an FX selector: navigating to VIDEO must preserve the currently chosen sound effect and wet level. Source-correct visuals are selected independently of the audio FX chain.
- No silent reduction in DSP, fidelity, frame count, colour, features, buffers or UI controls as a workaround for the device.
- Remove old KARMA-only synths, sequencer subsystems, menus, project format layers, and dead framework dependencies; use a separate narrowly scoped FM-1 hardware abstraction for LCD, keys/encoders, LEDs, audio I/O, flash and boot/update.
- Keep main protected and do not trigger any CI/manual build without explicit user authorization.

## Architecture and gates
Target: MIYAKO application core + independently ported MIYAKO DSP/FX + original-source video decoder + FM-1 hardware abstraction + vendor boot/update protocol. The old feature branch `feature/fm1-miyako-port-20261104` is retained unchanged for historical inspection only; its KARMA-based patcher must **not** be the new release path.

Do not claim a completed standalone firmware until each gate passes:
1. Actual compile/link source-closure audit shows **zero KARMA implementation units**.
2. Full original MIYAKO/FX scope has independent DSP and a reachable physical control for each exposed parameter.
3. Source-derived animation frames and palette decode match the source; no substitute art.
4. Measured FM-1 Flash/RAM/POOL/CPU and update-loader compatibility; projections do not count as measurements.
5. Tests on host, target compiler/linker, and real FM-1 (display, audio, controls, persistence, and no pops/freezes).

## Constraint record (not a native measurement)
The previous KARMA-based link reported ~581,564-byte application budget, 96 KiB RAM and 344,064-byte POOL. These are **preliminary engineering constraints**, not validated native MIYAKO free-space numbers. The original DREAMY implementation keeps ~2.5 seconds of stereo sample history; it cannot be copied wholesale to the FM-1 without a memory design. Do not silently replace it with a short effect and call it DREAMY.

## Progress status
This file is a binding design contract, not proof of an FM-1 binary. The branch now has:
- independent FM-1 UI, keyboard/encoder and persistent settings app entrypoints (`fm1/native/app.c`);
- an independent native **prototype** 4-voice synth plus REEL and EUREKA audio (`mi_audio.c`), not source-fidelity-complete MIYAKO DSP;
- independent original MIYAKO EFFECTS/EUREKA audio-reactive frame-index algorithms (`mi_video.c`), **not** decoded assets or rendered animations;
- CircleCI manual-only compile and FM-1 platform host simulator checks. CI has **not** been executed.

Not completed: source-fidelity DREAMY/DELAY/REVERB/CHORUS and other approved audio FX, independent multi-FX routing, EFFECTS and EUREKA media packing/rendering, accurate MIYAKO parameter behavior, target compiler/linker and memory measurement, physical device verification and a full MIYAKO FWSC. It is forbidden to present the provisional app as the completed firmware.
