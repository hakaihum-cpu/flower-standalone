# Original MIYAKO FX / FM-1 physical RAM feasibility audit (2026-11-06)

## Ground-truth evidence
- User's original MIYAKO source: `hakaihum-cpu/Vstplugin` at
  `feature/miyako-ep-sampler-16part-20261030`,
  `Source/MiyakoEurekaHaze.h`, `prepare(double sr)`, lines 35–57:
  `capacity = max(4096, ceil(sampleRate * 10.2))`; **two lanes**,
  each with `vector<float> buffer` and `vector<uint32_t> writeGeneration`
  of `capacity` elements. It is not a 3-second single 16-bit mono delay.
- FM-1 platform hardware linker source: `charlesvestal/fm1-base`
  commit `e07ebca2dda5fbd3bb40287d1f6083923f9ae999`
  `device/app_packed.ld` lines 22–27: entire usable packed SRAM
  region for RAM code, `.data`, `.bss`, `.pool` is `0x74000`
  (= 475,136 bytes), with stacks separate. Optional cache RAM is
  `0x7000` (= 28,672 bytes). The default non-packed regions are
  `96 KiB RAM` and `0x54000` (= 344,064 bytes) POOL.
- `device/app.ld` lines 45–50 bounds installable application XIP
  image to `0x8DFBC` (= 581,564 bytes), before platform and audio code.

## Reproducible memory calculation (44,100 samples/s)
A single original EUREKA lane: ceil(44,100 * 10.2) = **449,820 samples**.
One float buffer and generation array = (4+4) * 449,820
= **3,598,560 bytes**.
Two original lanes = **7,197,120 bytes** (6.86 MiB), without
scratch, JUCE DSP, convolution, visual decoder, voices, drivers or stacks.
Even only the original float samples without generation metadata need
2 * 449,820 * 4 = **3,598,560 bytes**.

This exceeds the complete FM-1 packed SRAM limit by **at least 15x**,
and the 0x8DFBC app flash limit by more than 12x. Removing the old
KARMA application does NOT make RAM that does not physically exist.

## Engineering consequences
- The branch's current native `mi_audio.c` uses an approximately 0.093 s
  4096-sample, 16-bit mono ring with a much simpler REEL/EUREKA processor.
  This is explicitly a *prototype* and **not** a faithful port of
  `MiyakoEurekaHaze`. The app currently cannot qualify as full MIYAKO.
- DREAMY also uses ~2.5 s history and JUCE DSP; delay, reverb, chorus,
  and concurrent FX buffers increase the true requirement.
- A faithful FM-1 port of the whole original algorithm and all simultaneous
  MIYAKO features would need a change in memory representation, audio
  fidelity, buffering/model, storage or hardware. Every such trade-off
  requires explicit approval; reducing it silently is prohibited.
- Original audio-reactive animation frame counts can be retained as
  reconstructed/quantised images independent of this audio-memory issue.
- No change here represents an approval to simplify EUREKA, DREAMY or
  the FX chain. CI may create a distinctly labelled **development** FWSC;
  it must never be passed off as the complete approved MIYAKO.

### Status
No device-validated complete FWSC. No CircleCI execution has been
triggered by this audit.
