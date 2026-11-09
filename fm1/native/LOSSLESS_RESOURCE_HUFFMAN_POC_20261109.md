# MIYAKO FM-1 628-frame lossless image-resource repack — 2026-11-09

## Decision
One path only: **lossless entropy coding of the current 96×96, 16-colour VQ resource**.
No additional lossy rescaling, generated pictures, removed frames, reduced palettes or changes to loop/FX behaviour.
The 628 existing VQ images remain pixel-identical. This cannot reverse the quality loss that already occurred when the originals were made into VQ images.

## Measurements against actual canonical archive
Source: 12 checked-in Base64 chunks under `Resources/fm1_photo_96_chunks/`, totaling **279,936 bytes**, containing 24 clip dictionaries and 628 selector maps. Confirmed by retrieving and decoding all 12 files via the connected GitHub source.

An independent in-session JavaScript encoder/decoder compressed every 4-bit VQ dictionary using a separate 16-symbol canonical Huffman table, and compressed each frame's 8-bit VQ indices using a clip-local 256-symbol canonical Huffman table. Raw fallback is available for a block that would not shrink. Every dictionary (24) and frame map (628) was decoded and checked byte-for-byte against the source.

| Measure | Bytes |
| --- | ---: |
| Existing raw VQ payload | 279,936 |
| Current MTL1 experimental payload | 272,521 |
| New Huffman encoded book data | 90,993 |
| New Huffman encoded frame maps | 162,490 |
| Planned MHU1 palette/header/directory overhead | 6,696 |
| **MHU1 size from measured block sizes + defined format overhead** | **260,179** |
| Reduction vs raw VQ | **19,757** (7.06%) |
| Reduction vs current MTL1 | **12,342** (4.53%) |
| Resource payload limit | 282,592 |
| Expected unused resource capacity | **22,413** |

The figure **260,179 B is a measured-block calculation**, not yet the output of the checked-in Python packer on the real archive. The Python implementation must reproduce and verify the exact archive before claiming a definitive payload size. Huffman tie-breaking can change a few padded frame bytes.

## Reproducible implementation
`fm1/native/tools/pack_photo_huffman_lossless_poc.py`:

```bash
python3 fm1/native/tools/pack_photo_huffman_lossless_poc.py \
  --out /tmp/miyako_628_lossless.mhu1 \
  --report /tmp/miyako_628_lossless_report.json
```

This uses the exact SHA256-checked existing repository archive. It includes a Python decoder, verifies all 628 palette/book/selector maps in forward, deterministic shuffled, and reverse order, reconstructs exact decoded VQ pixels and rejects corrupt header, bad offsets and truncation.

**Not an installable file**: `.mhu1` must never be renamed to `res.bin` or `.fwsc`. The existing approved `miyako_media.bin` and current native firmware remain unchanged. No CircleCI build or flashing was triggered.

## Gates before integration
1. Run the checked-in Python packer on the actual SHA-checked source archive and record resulting binary size and digest.
2. Implement a small bounded freestanding C decoder for the new frame format; prove 628 forward/shuffled/reverse frames match the VQ reference, plus corrupt-input tests.
3. Check target RAM, XIP size, audio deadlines, live LCD performance and CPU cost with the existing platform/toolchain.
4. Only if all checks pass, integrate an optional reader, preserving existing VQ/MTL1 fallback and the current flash/store/resource map. CI remains manual and needs explicit permission. No `main` changes.

A lossy direct16 source-frame experiment previously needed ~1.80MB for 628 frames; this lossless repacking is about saving space **within existing VQ quality**, not a claim that the direct16 source fits.