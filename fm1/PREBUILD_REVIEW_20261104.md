# MIYAKO FM-1 — pre-build inspection, 2026-11-04
Target: M-VAVE FM-1 / JieLi pi32v2, 240x240 non-touch LCD.
Review branch: `feature/fm1-miyako-port-20261104`.
Upstream: KARMA commit `db3cda79359ae914d1e58c0fc96c6ca744e7950e`.
**No CircleCI pipeline or JieLi target build was run during this inspection.**

## Pass 1 — source integration and user feature contract
- PASS: checked the *actual pinned upstream* source for 27 distinct replacement
  anchors; sequential simulated application found no collision or missing anchor.
- PASS: unchanged native PRISM/Plaits, ELEMENTS, and CLOUDS includes still connected.
- PASS: duplicate MIYAKO FM removed; Plaits FM / 6OP A/B/C retained.
- PASS: original ACID engine source no longer included; index 14 reserved for MIYAKO.
- PASS: Tape texture generator branch removed and persisted TX_TAPE values mapped
  to OFF, keeping Reel a separate choice. Vongon, EP-Sampler, Knowing and
  Twilight are not built as MIYAKO modules.
- PASS: checked actual `felucca.c` include order: engines and post callback precede
  voice rendering and FX mixer hook. EUREKA note-off tail remains connected.

## Pass 2 — source syntax, buffer and panel safety
- PASS: lexical parser inspected 10 C source/test files for balanced braces,
  brackets, parentheses, properly terminated comments and string literals.
  This is **not** a C compiler type check.
- PASS: tested numeric upper bounds with Python for all parameter extrema:
  EUREKA 256..3304 input samples fit 1024 x int16 storage at 4:1 sampling.
  Repeats max 4, total tail max 16520 samples (about 375 ms at 44.1kHz).
  32-bit interpolation intermediate at most 16,711,170.
  32-bit wet multiply bound 2,147,287,044 < INT32_MAX 2,147,483,647.
- PASS: measured equivalent host `old sizeof(miyako_slot_t)=8228` bytes and
  confirmed the C static bound rejects structures above 8448 bytes.
  The actual JieLi compiler layout **not measured**.
- PASS: the patched physical EDIT/FX button routes, SELECT/PRESETS/ALGORITHM
  encoder calls, KNOB 1..4 edit paths and no-touch design are present.
- NEEDS EXECUTION: actual host C compiler and sanitizer tests are committed as
  **five** CI prereqs, but were not rerun in this inspection.
- NEEDS EXECUTION: JieLi source compilation, 240x240 hardware rendering,
  actual physical key scanning, CPU usage, absence of audio pops and
  Mutable Plaits/Elements/Clouds audio tests.

## Pass 3 — CircleCI, toolchain, SDK and artifact gates
- FOUND / FIXED: the previous container `cimg/base:2024.08` could resolve to
  Ubuntu 24.04, which lacks `libtinfo5`. Set explicit
  `cimg/base:2024.08-22.04`, and fail fast if architecture or OS differs.
  This explicit image tag is consistent with CircleCI's naming rules;
  actual registry pull is still unverified.
- FOUND / FIXED: three exact SDK paths are now checked against the
  `tools/fm1pkg_make.py::sdk_file` implementation, and their SHA-256 sums
  are REQUIRED to match the known `tools/build.py::SDK_SHA256` entries.
  This is stronger than upstream's warning-only behavior.
- PASS: local `bash -n` of the new SDK-check command and generation of exactly
  three SHA records; no external SDK files were downloaded locally.
- PASS: `run_build` defaults to false, workflow is branch restricted and
  Android workflows/main are untouched.
- PASS: native build job checks three outputs:
  `build/felucca.bin`, `build/loader/ota.bin`, `build/felucca.fwsc`.
  It requires app length at most `0x8DFBC` = 581,564 bytes and stores
  the package plus a SHA256 manifest as artifacts.
- NEEDS EXECUTION: CircleCI YAML validation by CircleCI service, container
  registry availability, JieLi package server, Gitee SDK clone,
  actual native firmware link and real flash/RAM/pool sizes.

## Stop/go decision
**STATIC PRE-BUILD REVIEW: PASS with target-build gates pending.**
Do not describe this as target firmware compilation or a functional FM-1 image.
The cross-build must fail closed if any sanitizer, patch, package dependency,
ROM region, heap/stack/pool budget, firmware slot or integrity assertion fails.

Next action requires explicit user approval for the manual
`run_build=true` CircleCI pipeline. Until then no .fwsc has been produced.

## Pool headroom correction (2026-11-04)
Previous target linker measured .pool 341632 B of 344064 (only 2432 B free,
8192 required). Optimized track-1 arena is bounded by Juno's 2456 B,
freeing a measured-host equivalent 5772 B. The reverb's second allpass
(441 int16, 882 B) is moved from .pool to .bss without changing DSP.
Projected POOL free: 2432 + 5772 + 882 = **9086 B**, over the original
8192 B safety floor. Projected RAM .data+.bss: 96728 + 882 = **97610 B**,
below 98304. These are projections from the last actual link and must be
validated by the JieLi target build; no linker thresholds were lowered.
Local host ASan/UBSan DSP/FX/UI tests and 100000-sample split-reverb
bit-equivalence test pass. EUREKA retains 256..3304-sample timing using
4:1 stored points and interpolated replay.
