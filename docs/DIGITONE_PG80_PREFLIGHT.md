# PG80 preflight record — 2026-11-03

Baseline (unchanged): PG79 / Digitone mk1 OS 1.44, approved firmware commit d3ea8223a85d3f8f2300dc243fae6fce9fe61fcb, manual one-shot workflow #41632514533 (SUCCESS). Branch tip f2caf495e481a1cc14c23899a98bc59842c10792.

Development branch: feature/digitone-fx-dedicated-pages-20261103. **PG80 firmware NOT BUILT as of this record.** No automatic CI trigger; existing manual GitHub Actions workflow only.

## Completed preflight (host, not instrument)

1. Verified old PG79 EUREKA video and old standalone Granular SHA identical between baseline and dev branch; no stock key/VOICE/MUTE hook was rewritten. Original 576-frame pack conversion and original Granular UBSan were previously green in PG79.
2. Reviewed source, metadata and workflow in three groups; 21 checks passed (base sources, one audio owner, menu, FX DSP and UI, 2MiB bulk, callback names, original key passthrough, output path, pinned stock/core, manual-only dispatch).
3. Reconstructed the current C logic in a local isolated host harness (stripping comments and using a compact source representation for transfer, with SDK-compatible test stubs). GCC -std=c11 -O2 -Wall -Wextra -Werror -Wconversion compiled the four module C files: granular.c, rack_audio.c, rack_ui.c, digitone_fx_pages.c.
4. Native audio regression: GCC -DRACK_HOST_TEST, -Wall -Wextra -Werror -Wconversion, UBSan and ASan, PASS for Bitcrusher neutral/full-wet/4bit, Reel wet=0/wet=100, Eureka eventual processed-only output.
5. Native UI/rack integration: one linked host binary with all four C files, stub core3 exports and fake event records; PASS TRACK hold, stock TRIG 1..16 passthrough, FUNC+YES, rack's PAGE for Eureka, stock YES, NO and mono-page stock fallback, mode switching with fades.
6. Host stress: 3 modes x 5 parameter patterns x 12000 blocks x 64 values = 11,520,000 output sample words; UBSan+ASan PASS on synthetic audio and randomized/extreme values. No buffer overflows or signed-UB reported.
7. Original PG79 GitHub Action #41632514533 logs: linked OS1.44 EUREKA576 2.1 + Granular 0.5 with 422,348 bytes working RAM spare; packed OS verified in place; custom source code changed only in the new rack and independent tests.
8. Native x86 host object estimates (not ColdFire linker numbers): new rack .bss approximately 557,551 bytes including Granular's 524,416 bytes and 32,908 bytes of 4-FX DSP buffers, while Eureka's 2MiB ring is separately reserved at 0x47900000..0x47B00000. Compared with PG79 this is approximately 33KB more BSS, plus more code; linked headroom expected but only Elekloader can confirm.

## Defects found and eliminated before the next authorized build

- Fixed signed/unsigned reverb cursor conversion under strict -Wconversion.
- Fixed misleading indentation on two EUREKA `if` statements, rejected by -Werror.
- Corrected Bitcrusher transparent-path host assertion to use MIX=0, rather than quantized 16-bit/100%-wet.
- Preserved stock Q1.31 dry bits when Reel WET=0 and Bitcrusher MIX=0.
- Protected the active capture state on reselecting the same FX.
- Added EUREKA 60ms wet startup fade (without DRY bleed) and cleared stale reverb-off delay slots.
- Corrected Bitcrusher OUTPUT UI percentage (100 = unity).
- Confirmed selected owner always uses CORE_AUDIO_INSERT and avoids CORE_AUDIO_MUTE_VOICES.

## Still required: instrument-target build and physical verification

1. Dispatch the *existing* manual GitHub Actions workflow on exactly feature/digitone-fx-dedicated-pages-20261103. There is no `push` or scheduled trigger.
2. The workflow itself runs fresh exact-source GCC+UBSan/ASan regression, then pinned m68k GCC, Elekloader SDK mod builds, lint and patch --check, verified stock OS checksum, metadata, SYX byte integrity and artifact upload; if a preflight fails, no SYX is uploaded.
3. On actual Digitone verify FM/sequencer, MUTE, 16 TRIG keys, MIDI+YES video, all four FX audio and control pages, switching/pop noise, CPU and sound comparison with MIYAKO Reel/EUREKA. Passing host tests cannot replace a real-device check.

No SYX produced in this local preflight. Do not change approved PG79 or main.
