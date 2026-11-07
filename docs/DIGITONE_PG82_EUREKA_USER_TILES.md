# PG82 EUREKA animation tile replacement

- User supplied 7 contact sheets. Grid cell counts: 81 + 100 + 81 + 81 + 81 + 70 + 80 = **574**.
- Every cell is extracted inside its source black/white grid lines, row-major and preserving its original composition.
- The existing firmware hard-codes **576** frames in **16 scenes × 36**. Final 2 positions repeat user-supplied tiles 572 and 573 identically; no synthetic images.
- Original **#04 HYBRID** frame conversion (RGB contain into 128×64, local CLAHE, Bayer4 1bit) was applied before committing this new frame bank.
- `user_tiles/frames_lzma.b64` is a transfer-only LZMA wrapper around the **identical legacy encoded frame stream**. `prepare_user_tiles.py` decodes it, verifies all SHA256 and calls **unmodified** `prepare.py` to independently reconstruct the existing 96 keyframe seek records and 1bit preview.
- `eureka576.c`, the Digitone animation timing, MIDI controls, audio FX DSP, native menus, `mod.json` and the old PG81 sources remain unchanged.
- Old 576-frame source pack remains in Git history for rollback but is **not used** by the new build workflow. Approved PG79 and previous PG81 SYX remain untouched.
- For full original editable source crops and the pre-converted bank, use the companion user-facing ZIP: `Digitone_EUREKA_UserTiles_576_Replacement.zip` (574 individually cropped JPEGs, compiled stream, seek, reference, previews, manifest).
- This branch is **source-only, not built or device tested**. Manual firmware build requires explicit authorization. Do not claim audio/visual performance on hardware is newly verified.
