# FM-1 Runway-colour 628-frame MHP2 resource | 2026-11-14

## Scope and provenance
This source-only experimental branch replaces the previous unapproved warm/sepia-tinted photo resource with the actual 8 user-provided storyboards recolored through Runway. The main branch and approved FM-1 master remain unchanged.

Resource payload: **280651 bytes** (MHP2 core 200735 bytes + exact original 16-color tile patches 79916 bytes).
Standard existing fm1-base resource wrapper: +32-byte header = **280683 bytes**.
69-sector resource capacity including header: 282624 bytes (payload cap **282592 bytes**).
Margin: **1941 bytes**. Entire 628-frame image family, 8 banks, 24 clips with 16 RGB565 colors per 27 frames.
Patch format: 629 uint32 offsets + 4300 independent 18-byte 8x4 records.

SHA-256 (resource payload): `2426992e3f5c06870a654c49086923293e6c00247a1796f9f14048824bf1c17c`
SHA-256 (MHP2 core): `d0f333129696487907e81b21086b50a912c4f33f56eb595035d47dbeb0ae27b6`
SHA-256 (patches): `4178b4bdcd9ac910c693fa9b600038447e0f8a44559831283ad7455bed089532`
SHA-256 (628 decoded 96x96 palette-index images): `387652dd94d90d51d2f636320a72449c1557bacdf759d7cb2f62999c15e6d4a0`

## Validation already performed (offline, not CI/target)
- Three independent full-cycle MHP2 decode and patch checks, all 628 frames.
- Every patched 8x4 tile matches the 16-color source pixel indices; 4300 patches, at least one per frame.
- No repeated output frames; minimum 8 palette colors used per frame.
- Mean PSNR vs 16-color source: unpatched 22.7118 dB, patched **23.3764 dB**.
- 12 numbered base64 chunks recombined into the byte-identical, SHA-verified resource payload.
- Original and Runway generation sheet correspondence needs a **visual and physical FM-1 approval gate**; PSNR is not proof of drawing fidelity.
- NO CircleCI pipeline was triggered by this source-only change; NO FWSC or res.bin was generated; NO physical testing performed.

## Existing build path, unchanged
Only use the existing CircleCI `fm1_manual` workflow on this feature branch with `run_build=true`. The original `fm1-base` resource installer must create the CRC-protected `MIYAKO-FM1-NATIVE-DEV-res.bin` from the payload, and the firmware `FWSC` must use its matching resource. Do not change settings sectors, installer, main branch or CI scheme.

## Gates before claiming release
1. `fm1/native/tools/test_miyako_resource96.py` verifies 628 exact image frames.
2. `fm1/native/tools/pack_miyako_resource.py` rejects wrong SHA, dimensions, patch-count or payload.
3. Existing CircleCI native compile and host tests must pass, **not** only Python AST/static checks.
4. Artifacts: FWSC + the matching CRC-protected resource .bin; inspect their reported sizes.
5. User's FM-1 physical test: boot, SEL/FX display, 628-bank scene handling, image color, play/loop audio without dropouts.

Prior unapproved warm-colored resource is not accepted output of this branch.
