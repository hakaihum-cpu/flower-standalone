# FM-1 MIYAKO SEL: MHP2 DETAIL across all 628 frames (source-only experiment)

Date: 2026-11-13. Approved baseline: `fm1-master-20261112`, SHA of its VQ resource payload `df517e2d8c7902bed076610a07876dee660890799ba5dec6bc12bf366783bcf4`.

## User-selected appearance

This is the **middle / MHP2 DETAIL** candidate, *not* either of the later yellow/soft recolouring candidates. Preserve all 24 original-source RGB565 16-entry clip palettes byte-exact, all eight banks, all 628 chronological 96x96 images. Prioritize source-derived detail reconstruction (8x4 tile patches), with no generated imagery and no changed image order.

## Proof on current, not legacy, source data

1. Matched all eight privately supplied image sheets to the exact SHA256 values in `Resources/fm1_photo_96_chunks/manifest.json`.
2. Regenerated the **current master** 279,936-byte VQ archive using the existing `photo96_rebuild.py` algorithm, then verified the resulting SHA256 exactly matched the approved resource `df517e2d8c7902bed076610a07876dee660890799ba5dec6bc12bf366783bcf4` (not the older `bb5345df...` archive).
3. Encoded and independently decoded all 24 dictionaries and 628 frame maps using the compatible MHP2 v1 canonical-Huffman codec, byte-exact to the current master archive. Its actual size is 238,535 bytes.
4. Selected a capacity-bounded 2,200 source-derived replacement tiles. Frame 0 and frame 558 keep the eight best 8x4 patches each from the user-selected MHP2 DETAIL preview. Every other frame gets at least one source-improving tile, then global strongest gains are added up to eight patches per frame.
5. Replayed the actual 2,516-byte 629-entry offset directory plus 39,600 bytes of tile records backwards through **all 628 frames** and checked each reconstructed pixel index against the expected candidate. Each tile was required to improve BOTH original-source RGB squared error and edge-weighted error. Source-palette colours and per-frame identities are not changed.

## Exact resource payload size

| Part | Bytes |
| --- | ---: |
| Current master raw VQ | 279,936 |
| Current MHP2 core | 238,535 |
| Patch directory, 629 u32 offsets | 2,516 |
| Source-derived tile patches, 2,200 * 18 B | 39,600 |
| **New MHP2 + patches, total payload** | **280,651** |
| **Existing FM-1 resource payload maximum** | **282,592** |
| **Remaining** | **1,941** |

The resource maximum excludes the standard 32-byte CRC resource installer wrapper. The preview fits the **payload** window without changing the flash layout or saved settings. The MHP2 core and overlay are still **separate experimental artifacts**; there is **no integrated, installable `res.bin`** yet. A target decoder and package must pass cross-compile, link, audio/LCD timing, and real FM-1 checks before adoption.

## Quality measurement (RGB compared against the exact 628 original images)

| Metric | Current approved VQ | MHP2 + source detail |
| --- | ---: | ---: |
| Overall RGB PSNR | 20.337 dB | 20.657 dB |
| Per-frame average improvement | — | +0.366 dB |
| Minimum per-frame improvement | — | +0.031 dB |
| Maximum per-frame improvement | — | +1.432 dB |

All 628 frames improved in RGB MSE; no frame was allowed to get worse. This numeric result is **not a user acceptance/visual judgement**. User selects visual appearance from direct current-vs-candidate comparison PNGs; do not automatically replace the approved master.

## Reproduce (offline, no CircleCI build)

```bash
python3 fm1/native/tools/evaluate_mhp2_detail_all628.py \
  --sources /private/path/to/original-eight-sheets \
  --baseline /tmp/approved_current_279936_bytes.bin \
  --out /tmp/mhp2_detail_all628 \
  --patches 2200 --reserve 1700 --emit-test-pixels
```

Outputs include a full 628-frame actual patch binary, exact capacity/quality JSON, source/current/candidate comparison PNGs, and expected packed 4bpp pixel buffer for running the existing separate C host test. The script requires the **exact approved VQ SHA**, rejects over-capacity patch counts, and does not modify firmware or connected CI files.

### Scope and build restrictions

Research branch only; no main/master branch mutation, no CircleCI execution, no firmware flashing or installable FWSC/res.bin generation. The existing `mi_mhp2_codec_poc.c` and `mi_mhp2_patch_poc.c` remain unlinked experimental native reader units. Host Python full-image verification is not the same as a target build or physical test.