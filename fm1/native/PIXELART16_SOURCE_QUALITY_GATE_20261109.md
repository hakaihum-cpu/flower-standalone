# MIYAKO FM-1 pixel-art source quality gate — 2026-11-09

## What the user actually requested

Follow the user's November 8 low-colour pixel-art reference: coherent colour areas, distinguishable eyes/mouth, recognisable faces, gestures and objects. This is not a request for realistic photos, AI redraws, arbitrary warm recolouring or achieving a byte-count alone.

- Preserve **all 628 source frames**, bank sequence and artwork; maintain 16 RGB565 colours and 96x96 minimum.
- Do not replace source geometry, downscale to 48px, or use dither/noise to fake detail.
- Keep looper, EUREKA, DREAMY, ENV/LFO, paper-airplane frames, audio FX, saved projects and boot logic unchanged.
- Resource payload limit: **282,592 B**, not including standard 32 B wrapper.
- A byte-efficient candidate is not approved unless the **actual source-image quality** demonstrably improves.

## New isolated experiment

File: fm1/native/tools/trial_pixelart_16color_edges.py

1. SHA256-verify all eight original user contact sheets from Resources/fm1_photo_96_chunks/manifest.json.
2. Quantize real source pixels per 27-frame clip to 16 RGB565 colours, no dithering and no position-based recolour.
3. Produce 4x4 source-derived VQ tile codebooks (96 words per clip, M4H9 experimental format).
4. Retain small lines/edges by reassigning and refining codebooks against a strong-edge-weighted image error; source colours and frame geometry remain fixed. No object hallucinations or procedural drawings.
5. Allow the refined codebook only if worst and mean edge errors improve against the original 96-word M4H9 baseline, while adding no more than 96 bytes of Huffman selector data per clip.
6. Read the *current branch* SHA256-verified VQ resource and compare candidate and current VQ against the SAME 628 original source frames. Report PSNR and both mean/worst edge MSE per clip.
7. Write a contact sheet showing SOURCE/CURRENT VQ/TRIAL and an isolated noninstallable .m4h9 research payload.
8. Export a global quality gate flag only when every clip improves in edge fidelity without worsening source RGB fidelity.

Offline command (the eight exact original user sheets must be accessible locally; they must not be published to the repository):

    export FM1_USER_SHEETS=/path/to/the/eight/exact/user/sheets
    python3 fm1/native/tools/trial_pixelart_16color_edges.py \
      --source-dir "$FM1_USER_SHEETS" \
      --out /tmp/miyako_pixelart_edge_trial.m4h9 \
      --report /tmp/miyako_pixelart_edge_trial.json \
      --preview /tmp/miyako_pixelart_edge_trial.png

Dependencies: Pillow, numpy, scikit-learn.

## Evidence and actual status

- Existing raw 96x96 16-colour VQ: 279,936 B.
- Lossless MHU1 repack: about 260,169 B, but **no visual improvement**.
- Prior original-source M4H9 96-word 4x4 trial: 275,443 B, mean source PSNR 21.380 dB. Comparison against current VQ on exactly the same source had not been completed.
- The new edge-aware trial and strict source-comparison code have been committed, but **not executed on all eight original source sheets** in this environment. The GitHub repository provides only their hashes and filenames, not the binary source sheets. Saved VQ data already lacks information and must not be used to claim quality improvement from original.
- No new format accepted for firmware; no target compile, CircleCI run, user-data change or actual FM-1 performance validation.

Only after a successful source-based comparison should a new image codec be linked into the existing manually built firmware. Keep the existing MHU1/MTL1/raw VQ fallback until all real validations succeed.
