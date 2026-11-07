# MIYAKO FM-1 — pre-build audit, 2026-11-07

## Pipeline specification (DO NOT trigger without explicit authorization)

- Repository: `hakaihum-cpu/flower-standalone`
- Branch: `feature/fm1-miyako-recorder-eureka-fx-20261107`
- Checked source revision: `bf3abfa0c83aa2342fae59d34be6056191a7a156`
- CircleCI workflow: `fm1_manual`
- CircleCI job: `fm1_native_build`
- Trigger pipeline parameter: `run_build=true` (default `false`)
- Output: `fm1/MIYAKO-FM1-NATIVE-DEV.fwsc`
- The pipeline was **not triggered**. Therefore there is **no pipeline/run number**.

## Decision: HOLD — preliminary source checks passed, whole firmware pre-build proof absent

The CircleCI configuration has not been supplemented with redundant runtime tests.
One **deterministic failure before any C tests** was fixed on the feature branch:
`fm1/native/test_pipeline_contract.py` still demanded
`mi_wave_name(state.wave)`, while the now-correct OSC1/OSC2 UI uses
`mi_wave_name(show_wave)`. Fix: `98e7a999dcd757e6917b089f5109ed2827f10ae0`.
The CI header incorrectly said EUREKA was deferred; the descriptive comment
was corrected in `bf3abfa0c83aa2342fae59d34be6056191a7a156`.
No build logic or job was added.

### Audit A — state, controls and source references

- Checked app.c, mi_control, mi_audio, mi_fx_dsp, recorder, video,
  saved-state compatibility, header references, and CI contract.
- Source-local includes resolve. Deliberate exceptions:
  `plat.h` is supplied by pinned upstream `fm1-base`;
  `mi_effects_vq_frames.h` and `mi_eureka_vq_frames.h` are generated
  later by the pipeline and temporarily replaced by compile-only headers
  during upstream compiler prechecks.
- OSC1/SUB/CRUSH code stays in the audio path. OSC2 has an independent
  waveform/phase/detune/octave/mix and two physical OSC edit pages.
- DREAMY/EUREKA extra parameter page K1..K4 connects to actual DSP
  pitch/reverse/texture/feedback fields; independent FX A/B switches retained.
- Data migration checks the actual stored-object length before accepting an
  old prefix, because pinned fm1-base `st_load` can truncate to the caller's
  maximum. The host C ABI test confirms 68-byte old store and 104-byte new
  store; FM-1 target ABI still needs confirmation.
- Tile atlas: six bank counts sum to **488**, with contiguous source offsets;
  the encoded C string is **13,144 bytes**. The 9,856-byte decoded 32x32
  monochrome index/dictionary is range-checked; **48x48/4-gray enhancement is
  NOT committed**.
- Existing EFFECTS 144 + EUREKA 576 VQ frame banks remain configured.
- Requirement status: (①) DREAMY original visuals incomplete; (②) old
  EFFECTS/EUREKA square-aspect conversion incomplete; (⑦) original full VFX
  incomplete; ⑤/⑥ require device tests.

### Audit B — offline arithmetic and simulated audio

- The independent local C reproduction compiled and ran under
  GCC/Clang sanitizers, verifying saved-state layout, 128 pitch settings,
  DREAMY 16,362 bounded read coordinates, and a six-buffer pitched
  waveform reproduction.
- This is **NOT** the full repository source compiled together. It is
  evidence for arithmetic and memory-range assumptions only.
- Unit tests embedded in CircleCI were inspected for stale source contracts;
  they were NOT executed by CircleCI during this audit.

### Audit C — toolchain, target build, media and artifact

- Target toolchain: pinned JieLi `jieli-linux-toolchains-20250324.1`,
  SHA-256 `f686586bcfb45e0f0bb27fd2b39c7a7f313cb4f0e88a66a14da621ffa8225958`.
  Installer downloads to a file, verifies SHA-256, tests xz/tar format, then
  installs a known `pi32v2/bin/clang`. The fallback endpoint is also pinned
  by SHA-256, not trusted just because it responds.
- The primary and backup servers were **not reachable/verified from the
  audit runtime**. #267 failed because upstream endpoint timed out.
- Local audit host has `clang` for x86-64 only; no JieLi target compiler,
  pinned `fm1-base` checkout, or AC79 SDK. Consequently, **no original
  source target compile/link/FWSC execution** was possible here.
- CI is configured to use `X0X_FONTSET=terminus`, pinned Pillow/numpy,
  pinned `fm1-base` revision, pinned AC79 SDK tag, MMIO policy,
  compiler-only media stubs, full VQ conversion, C decoder CRC, host
  sound/video checks, and `verify_fwsc.py`. These are pipeline steps,
  not claims that they were executed in this audit.
- Flash budget **lower bound**: 144 EFFECTS + 576 EUREKA frames = 312,960
  bytes, plus 13,144-byte base64 contact sheet => 326,104 bytes of
  embedded media; app slot 581,564 bytes. The nominal remaining
  **255,460 bytes** must still hold the binary/code/metadata and related
  assets; true target link size is **unverified**.

## Exit criteria before claiming successful full pre-build audit

1. Real pinned JieLi compiler and AC79 SDK available independently of
   CircleCI, with checked bytes and matching pinned versions.
2. Unmodified **actual** GitHub source at the exact checked SHA compiled
   as a whole (host and target), with native tests and platform policy run.
3. Actual media VQ and binary linked within original FM-1 flash/RAM maps,
   FWSC checker passed; no fake media stubs in the final package.
4. UI tests cover OSC1/OSC2 and both FX pages; DSP and loop audio evaluated,
   without claiming source-perfect Dreamy/Eureka emulation.
5. Only then authorize an intentional `run_build=true` CircleCI run.

**Do not interpret the static checks or reconstructed DSP tests as a
successful full target build. No firmware is produced by this audit.**
