# FM-1 SEL / FX original-source palette comparison (preview only)

Goal: reproduce the **visual logic** of the supplied limited-color pixel-art reference:
strong dark masses, readable contours and broad color planes, **without**
forcing unrelated original photographs into the reference's warm colors.

**No firmware/source image is modified. No res.bin/VQ output is rewritten.**
The existing approved baseline remains `fm1-master-20261112`.

1. Reconstruct the ORIGINAL JPEG frame archives with the existing scripts
   (do not use output of the existing quantizers or VQ headers):
   ```sh
   python3 scripts/materialize_effects_video_pack.py
   python3 scripts/materialize_miyako_eureka_video.py
   ```
2. Preview clip 0 of FX and EUREKA without new compression:
   ```sh
   python3 fm1/native/tools/preview_original_palette.py \
     --mode fx --source Resources/effects_video_frames.pack \
     --clip 0 --out /tmp/fm1_palette_fx_0
   python3 fm1/native/tools/preview_original_palette.py \
     --mode eureka --source Resources/miyako_eureka_frames.pack \
     --clip 0 --out /tmp/fm1_palette_eureka_0
   ```
3. For SEL, first supply the **eight original montage sheets** with the exact
   names and SHA256 from `Resources/fm1_photo_96_chunks/manifest.json`.
   The checked-in compressed `res.bin` chunks are **not originals** and are
   intentionally refused as a substitute:
   ```sh
   python3 fm1/native/tools/preview_original_palette.py \
     --mode sel --source Resources/fm1_photo_96_inputs \
     --manifest Resources/fm1_photo_96_chunks/manifest.json \
     --clip 0 --out /tmp/fm1_palette_sel_0
   ```
   For the last SEL clip use `--clip 23` (7 source frames, not 27).

## The required 3-stage FX / EUREKA comparison

The existing VQ encoders now produce a 3-column preview **without changing
compressed media or its fixed 256 x 8x4 tile dictionary**:

- **left:** the actual original user JPEG, resized to 128x78;
- **center:** the original JPEG converted to a shared 16-colour RGB565 palette,
  *before* VQ;
- **right:** the same 16-colour palette after the existing 8x4 VQ encoder,
  decoded from the codebook and selector map.

The encoders print separate original-referenced PSNR for stages 2 and 3;
this isolates extra VQ loss instead of conflating it with palette loss.

Run from the checkout only when the original CRF1 pack has been verified:

```sh
python3 fm1/native/tools/pack_effects_vq_candidate.py \
  Resources/effects_video_frames.pack /tmp/preview_only_effects.h \
  --preview /tmp/FM1_EFFECTS_3STAGE.png
python3 fm1/native/tools/pack_eureka_vq_candidate.py \
  Resources/miyako_eureka_frames.pack /tmp/preview_only_eureka.h \
  --preview /tmp/FM1_EUREKA_3STAGE.png
```

The output header is **temporary** under /tmp and is never installed or
committed. The preview includes first/middle/last of all original 36-frame
clips. This is a codec quality study, **not** an FM-1 build or proof of actual
LCD appearance. No new VQ candidate should be adopted until source-referenced
palette and VQ results, including facial/contour detail, are reviewed.

Each output directory contains `compare_pre_vq.png`, native-resolution
`8colors/` and `16colors/` PNG frames, and `quality.json` with actual
RGB565 palettes and RGB fidelity. A single palette is derived from all
**original** frames in a clip (36 for FX/EUREKA, 27 or 7 for SEL). Dithering
is OFF. Native resolution is 128x78 for FX/EUREKA and 96x96 for SEL.

**Order of decisions:** review the visual comparisons first; select a color
count and palette extraction method from the actual pictures; only after
that investigate compression **without reducing the approved image quality**.
Do not trigger FM-1 CI builds or overwrite packed assets during this review.
