# MIYAKO source-of-truth audit for the FM-1 migration — 2026-11-06

## Verified first-party MIYAKO source (READ-ONLY)
Repository: hakaihum-cpu/Vstplugin
Branch: feature/miyako-ep-sampler-16part-20261030

- `Source/VisualizerComponent.cpp` blob `3acee750a2c2b4c1c53583cce2256cc0292fe3bb`:
  separate `loadEffectsVideoFramePack` and `loadEurekaVideoFramePack`;
  EUREKA pack explicitly requires 576 frames, while the general EFFECTS pack
  has its own audio-reactive display state, cooldown, source media and frame selection.
  **Preserving only EUREKA animation is not the MIYAKO port.**
- `Source/PluginProcessor.cpp` blob `da31980930aedd5876719d47bfbf77a23480ac88`:
  DELAY / REVERB / CHORUS / GRANULAR parameters and DSP states, DREAMY DSP,
  EUREKA audio, master FX, and other processing paths exist.
- `Source/PluginEditor.cpp` blob `02f37dc9f20993f46ca7b394ac9135c768dd892f`:
  distinct FX page controls and ON/OFF switches for DELAY, REVERB,
  GRANULAR and CHORUS; DREAMY page, TAPE page and other MIYAKO modes.

## Requirements for porting
1. **MIYAKO is source-of-truth**, not the old FM-1 proof of concept.
2. Audit complete MIYAKO FX routing, including independent ON/OFF states,
   and preserve the original chain/parallel behavior. The old FM-1 exclusive
   OFF/REEL/EUREKA switch is *not* a substitute for MIYAKO's multiple FX.
3. Both **EFFECTS** visual pack and **EUREKA** pack must be inventoried
   and preserved using original user-provided materials. Also audit all
   additional approved MIYAKO visual modes; do not assume these two exhaust
   its visual resources.
4. `SEL` / VIDEO navigation must **not enable EUREKA or overwrite WET**.
   The independent native control implementation now enforces that.
5. Port DSP semantics and physical knob mappings before exposing any FX as
   supported; names in the UI do not prove DSP exists.
6. Compile/release only when the FM-1 hardware HAL, startup, audio input/output,
   scheduler, display, controls, storage, and complete media pack are independent.
7. Do not include KARMA executable implementation in the output; references
   for hardware research do not constitute authority to ship that framework.

## Actual current development state
- Implemented in `fm1/native`: MIYAKO control state/navigation and host-test scaffold.
- NOT IMPLEMENTED: independent FM-1 board HAL, DSP equivalence for original MIYAKO FX,
  original EFFECTS video pack decoding, complete visual coverage, native boot/update,
  a fully linked FWSC and physical hardware verification.
- CircleCI for this branch is **native preflight only**, not a firmware generation pipeline.
- No pipeline run authorized by creating this document.
