# FM-1 MIYAKO SEL: original-only object-aware 16-colour MHP2 preview

**Status: offline research candidate, NOT FLASHABLE.** The approved master, audio/UI paths, CircleCI settings, existing `res.bin` installer, and resources are unchanged.

## Requirements honoured
- Exactly eight authenticated user-provided source sheets; no AI image generation or replacement character drawing.
- Pixel positions, original compositions and character identity preserved at **96×96**. Only colour assignment and offline lossy VQ encoding are changed.
- Single 16-entry RGB565 palette per 27-frame clip; 24 clips and all 628 frames preserved.
- Region-sensitive warm colour ramps (hair / skin / clothing / background) instead of a uniform luminosity-to-yellow tint. The input is monochrome for banks 0..5; the inferred skin segmentation is a heuristic, not reliable semantic understanding.
- Offline-only MHP2 Huffman compression of 8×4 / 256 codeword VQ; independent frame access, 629-entry correction directory; native codec implementation exists only as a separate POC from earlier branch commit.

## Reproducible data validation on all 628 frames
- **MHP2 encoded stream**: **215,925 bytes**, actual encoded binary with CRC32; all 24 dictionaries and all 628 map streams decoded byte-for-byte against source VQ.
- **Original-image-derived 8×4 tile corrections**: **3,563** records, directory plus patch payload **66,650 bytes**; all 628 corrected frames reconstructed exactly against offline reference.
- **Total packed payload**: **282,575 bytes**, maximum payload **282,592 bytes**; remaining **17 bytes** (only 17 bytes headroom, so NOT suitable for deployment until packaging/target effects are checked).
- Full res.bin CRC wrapper is **not** included in the above payload count; the existing resource slot is 69×4096=282,624 bytes, of which 32 bytes are reserved by the stock outer wrapper.
- Preview series: `SOURCE_VQ_MHP2_no_redraw_1.png` and `_2.png` show user originals vs same-geometry VQ vs corrected images (12 actual source frame IDs). The separate two-frame comparison uses master-preview pixels **previously exported from the user's earlier current-master comparison** and is not a reconstructed full-master archive; it is explicitly labelled prior export. The old local b64 cache is a **different, older archive**, not the latest master.

## Source-only commands
```
FM1_USER_SHEETS=/path/to/user/original_sources python3 real_palette_preview.py
FM1_USER_SHEETS=/path/to/user/original_sources python3 all628_trial.py
python3 encode_verified_mhp2.py
FM1_USER_SHEETS=/path/to/user/original_sources python3 write_overlay_and_correct_preview.py
```

The `all628_trial.py` source-vs-old-archive gallery is labelled **OLDER ARCHIVE**, never CURRENT MASTER. The later comparison script uses previously exported master snippets for frames 000/558 only.

## No-go conditions for promotion
- No validation of visual quality by the user yet; palette/tile appearance must not be selected automatically.
- No target C decoder end-to-end benchmark for this candidate. No embedded patch integration. No firmware build or hardware flashing.
- Remaining **17 bytes** is a tight resource bound, and the firmware-side code/stack/CPU budget must be tested separately. Any additional header/data inside the 282,592-byte payload exceeds capacity.
- Source recolouring requires user validation of facial contour and colour style, not PSNR alone.