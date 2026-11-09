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
| **MHU1 size from measured block sizes + defined format overhead** | **260,169** |
| Reduction vs raw VQ | **19,767** (7.06%) |
| Reduction vs current MTL1 | **12,352** (4.53%) |
| Resource payload limit | 282,592 |
| Expected unused resource capacity | **22,423** |

The **260,169 B** measurement is from an independent JavaScript encoder run directly over all 12 checked-in source VQ chunks, with byte-by-byte decode checks on all 24 clip books and all 628 maps, plus an assembled binary layout and CRC32 (`EBF25270`). It supersedes the previous block-size estimate of 260,179 B (10-byte coding tie-break difference). The checked-in Python packer and C reader still require a joint run against the complete original archive before declaring this a target-ready resource.

## Reproducible implementation
`fm1/native/tools/pack_photo_huffman_lossless_poc.py`:

```bash
python3 fm1/native/tools/pack_photo_huffman_lossless_poc.py \
  --out /tmp/miyako_628_lossless.mhu1 \
  --report /tmp/miyako_628_lossless_report.json
```

This uses the exact SHA256-checked existing repository archive. It includes a Python decoder, verifies all 628 palette/book/selector maps in forward, deterministic shuffled, and reverse order, reconstructs exact decoded VQ pixels and rejects corrupt header, bad offsets and truncation.

**Not an installable file**: `.mhu1` must never be renamed to `res.bin` or `.fwsc`. The existing approved `miyako_media.bin` and current native firmware remain unchanged. No CircleCI build or flashing was triggered.

## Host decoder implementation and tests

The following are committed in the **experimental** branch and the runtime reader has now been wired into `app.c` and registered in this branch's `fm1.toml`. The approved master and main are unchanged:

- `fm1/native/mi_hu_codec_poc.h` — reader API and exact size contracts
- `fm1/native/mi_hu_codec_poc.c` — bounded, freestanding canonical-Huffman decoder and CRC32/resource validator
- `fm1/native/test_mi_hu_codec_poc.c` — independent C palette/dictionary/map/pixel test for 628 forward, shuffled, reverse
- `fm1/native/tools/test_photo_huffman_host.py` — host-only real-archive Python/C runner

An independent local C implementation was compiled on Linux x86_64 using `cc -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined`; a deterministic **synthetic** 279,936-byte 628-frame test archive encoded by a Python MHU1 prototype to 230,955 bytes, and all C assertions passed (628 forward/shuffled/reverse, byte-perfect VQ pixels and corrupted-input rejection). This validates the algorithm and cross-language binary format **on synthetic data**, not all current MIYAKO artwork.

The real-data JS pack/decode proof above is separate. The committed Python and C code need their own real-archive validation run and target compiler/link/audio load checks. No CircleCI firmware build was launched and no main, approved firmware, installed res.bin, branch merge, or device files were modified. The experimental branch now has optional MHU1 runtime support, separate C reader compilation, and a manual CI job configured to test, pack, and produce the matching standard res.bin after explicit build authorization.

## Integration progress and remaining gates (2026-11-09)

- DONE: The experimental branch's `app.c` accepts MHU1 payloads alongside the existing CRC-protected MTL1 and raw VQ paths. It reuses the existing 4 KiB `mi_photo_tilebook`, 288-byte map and 4,608-byte pixel buffer. Switching from the other formats invalidates the dictionary cache.
- DONE: This branch's `fm1.toml` includes the separate `mi_hu_codec_poc.c` compile unit. No change to audio, recorder or UI mapping logic.
- DONE: `pack_miyako_resource.py --lossless-huffman-poc` creates the MHU1 payload after SHA256 verification and 628-frame exact source/pixel round-trip. Standard fm1-base resource tooling wraps that payload with its own 32-byte CRC header. An `.mhu1` file by itself must never be flashed as a `res.bin`.
- DONE: Existing CircleCI manual pipeline config is adjusted for this experimental branch only. It runs Python/C host unit tests with sanitizers **before** expensive media/build work and checks exact size **260,169 B payload / 260,201 B installed resource**. Its branch filter and `run_build=true` requirement are preserved.
- DONE: Static read-back audit of the updated GitHub files checks the code path, existing format fallback, compile list, packing gate, branch guard, resource size checks and allowed CircleCI template variables.
- PENDING: The committed Python/C host test must run together on the **current** 12 SHA256-verified GitHub source chunks. A local reference archive from an older branch (SHA256 `bb5345df...`) was verified with an independently compiled GCC host decoder, but it is different from the current GitHub source (SHA256 `df517e2d...`). Do not substitute the former proof for the latter.
- PENDING: Target pi32v2 compiler/link policy and LCD/audio timing checks. The current branch has not executed CircleCI. Physical FM-1 testing is also pending.
- PENDING: No changes may be merged to main or adopted as approved until target compilation and audio/LCD regression tests pass.

The next approved manual CI run, if authorized, needs to expose the pipeline number, branch name, exact `.fwsc` and `-res.bin` artifacts and test logs. No run has been requested or started here.

A lossy direct16 source-frame experiment previously needed ~1.80MB for 628 frames; this lossless repacking is about saving space **within existing VQ quality**, not a claim that the direct16 source fits.