# MIYAKO FM-1 SEL — MHP2 + source-derived detail correction feasibility

**Status: experimental research only, 2026-11-13.** This branch has **no change to `main`, the approved `fm1-master-20261112`, the CircleCI configuration, firmware media packing, or live UI/audio paths**. Neither `.mhp2` nor the `.mhp2_overlay` experiment may be renamed/flashed as `res.bin`; there is currently no integrated target firmware.

## Goals and constraints

- Improve **both** file size and fidelity to the user's eight original image sheets. Preserve **all 628** frames at **96x96**, existing source colours/clip identities and the 16 RGB565 colour limit. Do not generate replacement artwork.
- Resource payload must remain within `69 * 4096 - 32 = 282592` bytes, without touching persistent settings or moving the flash map.
- Continue using precomputed assets; the FM-1 must only do bounded VQ/Huffman decoding and optionally render small source-derived corrections. Its audio callback, loop/FX implementation and existing controls are unchanged.
- Do not promote a candidate on the basis of a better compression ratio **alone**. Compare SOURCE, current master VQ and candidate against the same eight SHA256-verified source sheets.

## New experimental codec: MHP2

- The encoder `fm1/native/tools/mhp2.py` reads the already-approved current 279936-byte VQ archive from 12 Base64 source chunks. Validates SHA256 `df517e2d8c7902bed076610a07876dee660890799ba5dec6bc12bf366783bcf4`.
- For each of 24 clips, compresses 4096 bytes of 256 opaque 8x4 VQ codewords with canonical Huffman symbols representing TWO packed 4-bit pixels per byte. It stores five-bit Huffman lengths per source byte alphabet and uses one fixed palette per clip.
- Compresses 628 independent 288-tile maps. It chooses per clip either a plain canonical Huffman map or a vertical same-as-above flag plus Huffman residual symbols, based on **actual encoded bytes**, including each frame's byte alignment. Random frame access is retained through 629 offsets.
- Binary: a 64-byte CRC32-protected header, 768-byte palettes, 7680-byte Huffman length tables, 24 mode bytes, 25+629 u32 offset entries, compressed dictionaries and maps. No decompression dependency on a separate firmware.
- Native reader `mi_mhp2_codec_poc.c`: freestanding C99, no malloc, no hosted C library includes or compressed-media encoder in firmware, 4096B dictionary cache and 288B map cache, 4608B output frame. Validates length and resource CRC during init, decodes arbitrary frames.

### Actual evidence

| Dataset | Raw VQ payload | MHP2 | Saving | Verification |
| --- | ---: | ---: | ---: | --- |
| **Current** `fm1-master-20261112`, exact GitHub archive | 279936B | **238535B** | **41401B (14.79%)** | Independent JavaScript Huffman encoder/decoder on all 24 books and 628 frames. **The committed Python/C encoder-reader pair has not been run against this current archive in one environment yet**. |
| Prior archived resource `bb5345df9b9e...`, local authenticated archive | 279936B | **229492B** | **50444B (18.02%)** | Python pack/unpack exact bytes, host C `-Werror -pedantic -fsanitize=address,undefined`, 628 forward + reverse + shuffled frames, exact palette and every pixel, CRC tamper rejection. |
| Synthetic independent archive | 279936B | 234580B | 45356B | Same full Python/C tests. |

**Physical capacity:** the current-source 238535-byte MHP2 leaves **44057B** within the 282592B resource payload limit, including possible source-derived image corrections. This is not free application flash and any target-reader growth still must satisfy the 581564B application slot.

### Source-derived correction experiment

- Input: eight original user-supplied contact sheets, each verified against the hashes in `Resources/fm1_photo_96_chunks/manifest.json`.
- For each frame, compare original RGB image with displayed VQ RGB. Choose up to 1800 individual **8x4 tile corrections** globally. Quantize correction pixels against the SAME 16-colour RGB565 per-clip palette and require strictly lower source RGB squared error plus strictly lower edge-weighted error in each corrected tile. Never alter any other pixel.
- Format: 629 little-endian u32 offsets then independently addressed 18-byte `(tile_id:uint16, original-source-derived 16-byte 4bpp tile)` patches. Pure offline encoder `evaluate_source_patch.py`; native decoder `mi_mhp2_patch_poc.c`. No overlay compression in the FM-1 firmware.
- **Measured only against the older archive** SHA256 `bb5345df9b9e...`: exactly 1800 patches across 628 frames; overlay 34916B; MHP2 + overlay **264408B** vs older 279936B (5.55% smaller). PSNR against the eight actual originals: **13.5041 -> 13.5966 dB** (gain **+0.0925 dB**). All 628 before/after frames and source-to-palette pixel data evaluated; independent C patch renderer reproduced Python expected 628×4608-byte output in forward, reverse and shuffled order. This gain is **small**: the patch trial does not yet meet the desired meaningful visual improvement.
- With the current MHP2 core of 238535B and the **same number** of 18-byte patches, the size *would be* **273451B**, still below the old raw payload. **This is a size projection only**. The source-based patch evaluation on **current** data has not been run, because the current approved VQ payload was accessed through the connected GitHub API rather than materialized into the host-test environment.

## Reproduce source-only, without CircleCI

```bash
python3 fm1/native/tools/test_mhp2.py --pair-dir /tmp/mhp2_synthetic
# Current checkout's exact SHA-approved source VQ (when its chunks are present):
python3 fm1/native/tools/test_mhp2.py --repository-source --pair-dir /tmp/mhp2_current
python3 fm1/native/tools/mhp2.py --out /tmp/sel_candidate.mhp2 --report /tmp/mhp2_report.json
cc -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
  -I fm1/native fm1/native/mi_mhp2_codec_poc.c \
  fm1/native/mi_mhp2_patch_poc.c fm1/native/test_mi_mhp2_codec_poc.c \
  -o /tmp/test_mhp2
/tmp/test_mhp2 /tmp/mhp2_current/reference.vq /tmp/mhp2_current/encoded.mhp2
# Source correction only with exact eight privately supplied user image sheets:
python3 fm1/native/tools/evaluate_source_patch.py \
  --sources /path/to/private/user-sheets --baseline /tmp/mhp2_current/reference.vq \
  --out /tmp/mhp2_source_patch --patches 1800 --emit-test-pixels
/tmp/test_mhp2 /tmp/mhp2_current/reference.vq /tmp/mhp2_current/encoded.mhp2 \
  /tmp/mhp2_source_patch/trial_patches.mhp2_overlay \
  /tmp/mhp2_source_patch/expected_patched_pixels.bin
```

## No-go criteria / remaining work

1. **Meaningful image quality gain not yet proven on the approved/current archive.** The old source comparison proves only a small +0.0925dB improvement. The previous lossy VQ degradation cannot be eliminated by an entropy-only re-pack.
2. The experimental overlay is currently **a separate research file**; pack+overlay integration into an actual CRC-protected standard `res.bin` is deliberately not present. Never flash experimental `.mhp2` files.
3. Cross-compile, target APP slot, RAM/CPU budget, rendering times during active audio/looper and physical display tests **not done**. No CircleCI build triggered. No modification to `.circleci/config.yml`, master, firmware, or persistent saved user data.
4. To claim success, source/approved-VQ/new candidate comparisons must prove source fidelity improvement at equal/better file size for the same 628 frames, including bad facial contour cases; host C must validate exact output and target resource installation must pass a separately authorized manual CircleCI job and physical hardware testing.