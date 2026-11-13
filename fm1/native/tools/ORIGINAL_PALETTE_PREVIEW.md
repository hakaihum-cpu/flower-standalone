# FM-1 SEL / FX original-source palette comparison (preview only)

Goal: reproduce the **visual logic** of the supplied limited-color pixel-art reference:
strong dark masses, readable contours and broad color planes, **without**
forcing unrelated original photographs into the reference's warm colors.

**No firmware/source image is modified. No res.bin/VQ output is rewritten.**
The existing approved baseline remains \`fm1-master-20261112\`.

1. Reconstruct the ORIGINAL JPEG frame archives with the existing scripts
   (do not use output of the existing quantizers or VQ headers):
   \`\`\`sh
   python3 scripts/materialize_effects_video_pack.py
   python3 scripts/materialize_miyako_eureka_video.py
   \`\`\`
2. Preview clip 0 of FX and EUREKA without new compression:
   \`\`\`sh
   python3 fm1/native/tools/preview_original_palette.py \
     --mode fx --source Resources/effects_video_frames.pack \
     --clip 0 --out /tmp/fm1_palette_fx_0
   python3 fm1/native/tools/preview_original_palette.py \
     --mode eureka --source Resources/miyako_eureka_frames.pack \
     --clip 0 --out /tmp/fm1_palette_eureka_0
   \`\`\`
3. For SEL, first supply the **eight original montage sheets** with the exact
   names and SHA256 from \`Resources/fm1_photo_96_chunks/manifest.json\`.
   The checked-in compressed \`res.bin\` chunks are **not originals** and are
   intentionally refused as a substitute:
   \`\`\`sh
   python3 fm1/native/tools/preview_original_palette.py \
     --mode sel --source Resources/fm1_photo_96_inputs \
     --manifest Resources/fm1_photo_96_chunks/manifest.json \
     --clip 0 --out /tmp/fm1_palette_sel_0
   \`\`\`
   For the last SEL clip use \`--clip 23\` (7 source frames, not 27).

Each output directory contains \`compare_pre_vq.png\`, native-resolution
\`8colors/\` and \`16colors/\` PNG frames, and \`quality.json\` with actual
RGB565 palettes and RGB fidelity. A single palette is derived from all
**original** frames in a clip (36 for FX/EUREKA, 27 or 7 for SEL). Dithering
is OFF. Native resolution is 128x78 for FX/EUREKA and 96x96 for SEL.

**Order of decisions:** review the visual comparisons first; select a color
count and palette extraction method from the actual pictures; only after
that investigate compression **without reducing the approved image quality**.
Do not trigger FM-1 CI builds or overwrite packed assets during this review.
