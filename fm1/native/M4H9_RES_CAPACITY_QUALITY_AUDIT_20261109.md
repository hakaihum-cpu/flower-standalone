# MIYAKO FM-1 `res.bin` optimization: measured result (2026-11-09)

**Status: isolated, offline experimental codec. No production firmware change, no CI build or flashing.**
No ART/SRC replacement; ENV, LFO, Recorder, FX/EUREKA, saved projects, firmware flash layout and playback semantics remain unchanged.

## Fixed resource constraints (pinned fm1-base)

- `APP_RES_BASE = 0x97000`, `APP_RES_SECTORS = 69`; last byte 0xDBFFF.
- 69 * 4096 = 282,624 bytes including fm1-base's 32-byte RES1 header.
- Payload maximum = **282,592 bytes**. The resource installer appends a CRC32/length-checked 32-byte header.
- Preserve settings starting `0xDC000`, existing `APP_STORE_MAP`, other XDATA regions. Do not expand the resource window without a separate persistence/protection review.

## Actual source-image tests

Each trial includes all **628** original user-supplied image frames, verified against the 8 archive source hashes in `Resources/fm1_photo_96_chunks/manifest.json`. All are downsampled to 96x96, quantized to 16 RGB565 colours per 27-frame clip, and VQ-coded as 4x4 tiles.

| Dictionary per 27 frames | Payload | Available after payload | Mean RGB PSNR vs 96x96 original | Disposition |
| --- | ---: | ---: | ---: | --- |
| 64 tiles, canonical Huffman | 241,599 B | 40,993 B | 20.854 dB | Smaller, but lower fidelity |
| **96 tiles, canonical Huffman** | **275,443 B** | **7,149 B** | **21.380 dB** | Fits, experimental |
| 128 tiles, canonical Huffman | 302,552 B | -19,960 B | 21.728 dB | Exceeds resource; reject |

- 96-word source-derived resource SHA256: `69eb8af2574f0a14cbd4e1bc000575cae35bc8f5a94a33d18f2e7f48cb29309f`
- 96-word decoded palettes + all original-sized packed indices SHA256: `aa6bc774f33d571c77265dc583f20d223ec3ce7422d605611028cf8eeadaf249`
- C host decoder's 628-frame palette+pixel CRC32: `CB90E9DA`.
- Independent Python and C decoders validated **628 forward + 628 shuffled + 628 reverse** frames, and rejected a corrupt header/out-of-range frame index. Host C compiled with ASAN/UBSAN and `-Werror`.
- C freestanding decoder: `fm1/native/mi_m4h9_codec_poc.c`, `mi_m4h9_codec_poc.h`.
- C host test: `fm1/native/test_mi_m4h9_codec_poc.c`.
- Offline encoder from user-source sheets: `fm1/native/tools/m4h9_source_poc.py`. It needs the eight original user-provided sheets available locally under `FM1_USER_SHEETS` plus Pillow, numpy and scikit-learn; they are not checked into this repository.

## Important boundary

**The 96-word candidate has NOT been shown superior to the current GitHub VQ release.** The PSNR values above compare the three new candidates to exactly the same original source images; the older locally available VQ archive is not a valid substitute for the current archive. Before switching the running UI to M4H9, compare the *actual current* VQ payload against exactly the same 628 RGB reference frames and measure quality per bank and worst frame. If no improvement is proven, retain the approved VQ/MTL1 reader.

M4H9 uses lossy **16-colour, 96x96** images. A decoder's exact output check only ensures codec correctness; it does **not** mean the original full-colour source was reproduced pixel-for-pixel. Do not call M4H9 a complete visual quality fix.

## Next bounded proof

1. Current-archive vs M4H9 RGB PSNR and visual inspection of all 8 banks.
2. If improved, test target-code RAM/POOL space and 96-colour-codebook CPU cost while audio runs.
3. Only then integrate resource generation into the existing manual CircleCI flow, with prebuild source, resource, CRC and host quality gates.
4. Do not run CI automatically; preserve golden baseline, firmware settings and restore mechanisms.

The uploaded local M4H9 binary is a **raw experimental payload**, not an installable `res.bin` and not a standalone .fwsc.
