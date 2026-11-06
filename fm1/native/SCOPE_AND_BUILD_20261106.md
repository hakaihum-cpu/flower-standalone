# FM-1 MIYAKO independent native build scope — 2026-11-06

**LATEST USER INSTRUCTION OVERRIDES EARLIER P2 SCOPE:** "いったんEUREKAももういいですよ。形にしてください。" Therefore EUREKA audio and EUREKA animation are intentionally **DEFERRED, not silently reduced** on this branch. The previous full-scope contract remains historical; do not resume EUREKA optimization without a new instruction.

## Absolute prerequisites
- This is a MIYAKO port, not a KARMA port.
- No KARMA runtime, application, DSP or menu is packaged.
- Reuse fm1-base as the independent FM-1 hardware/boot/LCD/MIDI/flash abstraction only.
- Preserve all 144 user-origin EFFECTS animation frames (16-colour source quantisation explicitly reported).
- Ship only after real linker and package checks, without modifying main.
- Keep CircleCI default manual, run_build=true; do not start a build without user authority.

## Scope of the first writable native development image
- Hardware: physical FM-1 keys, controls, LCD, preset storage, USB/TRS MIDI.
- MIYAKO instrument: four-voice SINE, SAW, SQUARE, PULSE, SUB and CRUSH.
- FX implemented in independent native C: REEL, DELAY, REVERB, CHORUS.
- Effect mode navigation plus WET and parameters, clearly distinguishing unsupported parameters.
- User-provided EFFECTS video (all original 144 frames, audio-reactive).
- EUREKA: intentionally absent; do not compile pack or hidden screen.
- DREAMY: still pending a distinct faithful DSP; never claim a fully working DREAMY.
- Original simultaneous MIYAKO multi-effect chain: still pending; do not claim equivalence.

## Binding visual-quality decision (confirmed from earlier 2026-11-06 history)
- The user already requested the **26 dB PSNR pass/fail gate be discontinued**.
- The assistant had accepted removing this arbitrary numerical quality blocker and
  retaining exact frame count, original-source images, 16-colour format, memory size,
  data integrity, and independent C/Python CRC verification.
- PSNR **MUST be reported only**; a result such as 24.43 dB MUST NOT block
  an installable trial FM-1 development FWSC on its own.
- Do NOT silently cut frames, use generated images, suppress visual loss or waive
  technical memory/flash/package integrity checks.
- The enforced regression lives in `fm1/native/test_pipeline_contract.py`.

## Build input and output
Repository: hakaihum-cpu/flower-standalone
Branch: feature/fm1-miyako-no-eureka-20261106
CircleCI manual trigger: run_build=true
Workflow: fm1_manual
Job: fm1_native_build
Expected **only after a successful compile, link, and package**:
fm1-dist/MIYAKO-FM1-NATIVE-DEV.fwsc
Artifact includes source-video preview and SHA256SUMS.
This is a **development preview**, not a final full-featured MIYAKO port.

## Verification still needed
- CircleCI host/controller/audio/media CRC checks successfully execute, not just appear in config.
- JieLi cross-compiler generates app.bin under flash and SRAM ceilings.
- Native FWSC package verifies the expected identity and loader.
- Physical FM-1: audio, keys, display, MIDI, persistence, audio glitches, and recoverability.
- Full scope/fidelity (particularly DREAMY and multi-FX) is not declared finished.

As of creating this record, **none of those runtime checks has completed** and **no native FWSC has been produced**. The existence of CircleCI YAML is not a successful build.
