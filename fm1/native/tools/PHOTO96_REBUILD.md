# MIYAKO FM-1 original-source RGB565 animation resource

This is the existing **96×96, 8×4 tile, 256-word/clip VQ** format. There
are **628 chronological frames**, split into eight banks
(81+64+100+81+81+81+70+70) and 24 fixed 27-frame dictionaries.
No new flash layout, codec, CircleCI workflow, or additional resource files
are involved.

The 628 frames have been re-encoded from the **eight exact user-provided
sheets**, with each source SHA256 preserved in
`Resources/fm1_photo_96_chunks/manifest.json`. The source images are not all
monochrome: the final sheet includes real yellow skin tones and coloured
sunsets. Each 27-frame VQ clip stores its own native 16-colour RGB565
palette. A single artificial grayscale/amber/blue palette must NOT be
substituted.

### Exact archive

- 24 × 32 bytes: clip RGB565 palettes (768 bytes)
- 24 × 256 × 16 bytes: 8×4 4-bit tile dictionaries (98,304 bytes)
- 628 × 288 bytes: 8-bit selector maps (180,864 bytes)
- Total: **279,936 bytes**, unchanged
- Stock FM-1 resource installer adds its usual 32-byte CRC header:
  **279,968 bytes**, within the existing 69-sector range
- SHA256 of the payload:
  `df517e2d8c7902bed076610a07876dee660890799ba5dec6bc12bf366783bcf4`

The 12 numbered Base64 chunks in
`Resources/fm1_photo_96_chunks/` are the build input. The existing
`pack_miyako_resource.py` verifies all their decoded bytes with SHA256.
`test_miyako_resource96.py` validates all 628 distinct decoded frames
and the 24 real palettes. This quality rebuild restored the coloured user
images; offline objective comparisons against the previous neutral-colour
rendering improved all 24 clips (0.30–2.09 dB clip-average RGB PSNR).
It does **not** imply perfect reconstruction of JPEG originals.

### Installation / CI

Use the unchanged existing CircleCI manual pipeline. The new firmware
must be installed together with its matching
`MIYAKO-FM1-NATIVE-DEV-res.bin`, using the stock FM-1 resource installer.
Earlier `res.bin` payloads, even at the same length, have different VQ
codebooks/selectors and are intentionally rejected by the runtime.

The optional offline encoder is `photo96_rebuild.py` using the eight
original user sheets under `FM1_USER_SHEETS`. It should only replace the
checked-in chunks after exact image hashes, CRC, all-frame RGB quality,
package size and native decoder checks pass.
