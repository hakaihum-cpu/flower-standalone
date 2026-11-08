# Miku-FM animation implementation review (2026-11-09)

Scope: source inspection and offline reproducible feasibility audit only. **No modification of approved MIYAKO animation, audio, UI, saved data, or build**.

## Upstream reference

- Repo: https://github.com/MichaelAPerry/MIKU-FM1
- Working FM-1 code: `native/dinghy.c`, specifically `face_draw()`, `face_set()`, `face_update()`, `dance_draw()`, `dance_update()`.
- Art preparation: `tools/sprite_pack.py`; data: `firmware/ui/singer_sprites.h`.
- *Do not copy or redistribute the upstream character artwork*: it is subject to the Piapro Character License (see upstream `LICENSING.md`). Code and character art have separate licenses.

## What the working hardware renderer actually does

- 12 face states (neutral, blink, vowels, expressions, tilts), 8 full-body dance poses.
- Sprite assets are **4bpp (16 indices) with transparent index zero**, packed two pixels/byte and RGB565 palettes.
- 90x90 faces at 2x scale, 61x71 dance frames at 3x scale; palette-to-RGB565 mapping is done during LCD scanline blit.
- Face states share a fixed head/hair/base. `face_set()` computes the union of `SINGER_FACE_DIRTY[old]` and `SINGER_FACE_DIRTY[new]` bounding boxes and redraws only that rectangle.
- `face_update()` triggers vowel/blink states on live audio/time, rate-limited to >=40ms. `dance_update()` selects 8 poses from song/voice state and redraws on state change (>=60ms).
- Raw pixel-data total stated by the generated sprite header: **65,928 bytes** (48,600 faces + 17,328 full-body). This achieves animation from *20* unique images; it does **not** demonstrate losslessly storing 628 distinct original photos in that size.
- The older standalone `firmware/ui/stage.c` draws a *different*, primitive rectangle-based prototype. **Use `native/dinghy.c` as the real firmware reference.**

## Exact MIYAKO 628-image feasibility test

Run an offline audit against the eight original user-supplied sheet files (SHA256 checked against the approved `Resources/fm1_photo_96_chunks/manifest.json`), with each original cropped/resized to 96x96 using the approved LANCZOS source transform and then quantized to RGB565 **without introducing 16-colour VQ loss**.

For each pair of consecutive frames, compare every original RGB565 pixel and calculate the *smallest exact rectangle* covering every changed pixel. Restore every next frame from the previous image plus this exact rectangle and assert byte-for-byte equality. Count only within-bank transitions; there are **620**.

| Bank | Frames | Changed RGB565 pixels between adjacent frames | Mean exact dirty-rect coverage |
| --- | ---: | ---: | ---: |
| 0 | 81 | 79.60% | 100% |
| 1 | 64 | 91.05% | 100% |
| 2 | 100 | 95.73% | 100% |
| 3 | 81 | 96.90% | 100% |
| 4 | 81 | 67.90% | 100% |
| 5 | 81 | 63.77% | 100% |
| 6 | 70 | 92.34% | 100% |
| 7 | 70 | 95.03% | 100% |
| **All** | **628** | **85.15%** | **100%** |

Every rectangle patch roundtrip restored the target 96x96 RGB565 pixels exactly. This is **only a size/structure audit**, not a 240x240 complete-image reconstruction.

## Implications and decision

- **Do not replace** the current 628-frame 27-frame-clip VQ/MTL1 path with Miku-FM's dirty-rectangle trick. The full-frame box changes would give no LCD blit savings and no assured flash saving; they are visually distinct user artworks, not 20 parameterized poses.
- **Appropriate test candidates**: image sequences with an identical base and only eyes/mouth/arms changed, including user-supplied ENV/LFO poses and the paper-airplane action. First classify exact common regions in their original images and test if stored base+patches actually reduces size. Preserve original user image pixels and timing. Do not replace their poses with inferred/generated art or create new animation states.
- Adapt the *algorithm*, not the copyrighted character or look. Preserve user-specific control mapping and audio sync.
- The existing MTL1 trial is a lossless re-pack of **already degraded 96x96 16-colour VQ**; it is not the user's required full-original fidelity. Do not call it a quality completion.
- No CircleCI run/build is authorized by this assessment. FM-1 live memory/timing still require target validation.

This review is independent of, and makes no changes to, the existing `feature/fm1-photo-shared-tile-poc-20261109` code and pipeline.
