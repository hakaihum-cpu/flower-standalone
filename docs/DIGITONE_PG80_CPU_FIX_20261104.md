# Digitone PG80: audio IRQ CPU remediation (2026-11-04)

## Real device findings
Owner installed the PG80 trial from successful GitHub Actions Run 41648402050.
- EUREKA audio: severe slowdown and device freezes.
- REEL and BITCRUSHER: severe slowdown but barely usable.
The earlier successful SYX build proves compilation, not performance. PG80 has **failed real-device performance**. PG79 remains the last confirmed working baseline. CPU overload is the leading hypothesis but is not yet proven by on-device timing.

## Scope
Source-only performance changes committed on branch fix/digitone-pg80-audio-cpu-20261104, separate from PG79, PG80 trial and main. Workflow remains manually dispatched; no new SYX build or workflow run was started. Delay Plaits integration until the base DSP is stable.

## Source optimizations
- Reel and EUREKA tape soft clipping: replace real-time variable-divisor math with interpolated 257-point int16 lookup. EUREKA two static curves; Reel two buffers swapped only after UI-task curve regeneration on DRIVE changes. WOW/FLUTTER modulation depths are separately cached on UI task.
- Reel WOW/FLUTTER phase modulation: use fixed-point right shifts in place of per-sample division by 32767.
- Bitcrusher signed 4..16-bit quantization: exactly equivalent shift-and-round logic. RATE modulo replaced with branch-reset counter.
- EUREKA HAZE reduction: shift arithmetic; OG/noise divisions are compile-time constants, not a run-time denominator. FILTER alpha computed once per 32-frame block. Before loop capture is primed, skip unnecessary voice/filter/tape processing.
- Rack UI overlay redraw requests reduced from every UI tick to every fourth tick. No change to the separate PG79 EUREKA visual engine.

## Completed local numerical checks
- Exhaustive 16-bit input x 13 bit-depths: 851,968 signed Bitcrusher samples, bit-for-bit exact quantization.
- Exhaustive 16-bit input x 128 Reel DRIVE values: 8,388,608 sample tests; maximum tape lookup deviation 105 counts out of 32,768 peak (<0.33% FS). Exact sound parity is NOT claimed.
- Host C equivalence regression is added at ports/digitone_fx_rack/tests/test_rack_cpu_lut.c and now gated in the **next authorized manual build**. No CI execution occurred on this fix branch yet.

## Acceptance gates before a next SYX build
1. Three passes of code/metadata review, exact-source GCC -Wconversion -Werror and sanitizer regressions, cross compilation, Elekloader lint/patch --check, stock checksum checks, no modifications to PG79/main.
2. On-device per-32-frame callback time profiling with FM/polyphony/sequencer/visuals active. At 48kHz each callback starts every ~667us, but the existing Digitone stages also consume time. P99/max, not just averages, must fit.
3. Long-running real-device playback (>=10 min), all four FX with max/min controls, OG/LOOP/STEREO/REVERB combinations, 16 TRIG keys, MUTE, MIDI+YES, stock FM, no UI freeze or audible clipping. If EUREKA still exceeds the budget, identify hotspots before a deliberately quality-reduced ECO mode; never silently remove features.

No guarantee of eliminating hardware freezes before profiling and device test.
