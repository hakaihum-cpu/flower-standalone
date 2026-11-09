# MIYAKO direct 16-colour pixel-art image conversion — 2026-11-09

## User-approved direction

- The aim is **visually clean pixel art**, not full-colour photo reproduction.
- Use the *original user images*, directly reduce each frame to **16 RGB565 colours**, without dithering, vector quantisation (VQ) tile substitution, generated art, or changing frame identity.
- Keep the complete **628 normal-animation frames** and bank order.
- Maintain the existing ENV, LFO, Recorder, audio FX/EUREKA and settings. Do **not** introduce additional visual modes or a new build workflow.
- No firmware build, res.bin flashing or branch merge was executed in this experiment.

## Reproduced on the complete 628 original frames

Local script: `fm1/native/tools/pack_source_pixelart16.py`. Uses the 8 exact SHA256-validated user image sheets referenced in `Resources/fm1_photo_96_chunks/manifest.json` and the existing `photo96_sources.py` crop at 96x96.

Per 27-frame group: quantise to a shared 16-colour palette, snap to RGB565, then pack each pixel in 4 bits without a VQ dictionary. All source-quantised frames are checked against decompression bit-for-bit. The output is an **offline .m16p research file**, not an installable resource. No native decoder is added or linked.

| 628-frame method | Bytes of image content (excluding res.bin header) | Fits existing region? |
| --- | ---: | --- |
| Current 96x96/16-colour VQ | 279,936 | Yes; image shape can be degraded by the VQ tile dictionary |
| 96x96/16-colour **direct**, DEFLATE per 27-frame clip | **1,799,060** | **No**; exceeds by 1,516,468 B |
| 96x96/16-colour direct, LZMA per 27-frame clip, *not implemented on target* | about 1,581,000 plus metadata | No |
| 48x48/8-colour direct, median 5x5, DEFLATE | about 251,416 | Fits **but visually rejected**: eyes, mouth and narrow lines disappear |

Resource limit remains 69 sectors of 4,096 bytes minus 32-byte RES1 header = **282,592 bytes**.

**The direct16 reference is the visual goal, not a flashable candidate.** Fitting its 628 images requires a future approach that preserves salient shapes without the large raster payload; current 48px over-smoothed tests are explicitly **not accepted**. This is a measured size blocker, not a conclusion that direct colour reduction is visually wrong.

### How to reproduce without touching firmware

Provide the existing exact eight image sheets in a private directory, then run:

```bash
python3 fm1/native/tools/pack_source_pixelart16.py \
  --source-dir /path/to/approved-eight-sheets \
  --output /tmp/fm1-direct16-reference.m16p \
  --report /tmp/fm1-direct16-report.json \
  --preview /tmp/fm1-direct16-side-by-side.png
```

The test output is deliberately **not** named `res.bin` or `.fwsc`. A .m16p file must never be installed as a firmware resource: it does not fit the FM-1 memory map.

SHA256 of the measured direct16 output: `4655d45bb5247951387d77ecdfe42fecea067a00afdfd96c07e90a3da537b58c`.

## Forward decision gates

1. Continue to use the visually clean original-source direct16 result as comparison baseline. Judge legibility of eyes, mouth and silhouettes first, numerical PSNR second.
2. Do not automatically replace or modify the original 628-frame approved resource until an equally legible candidate fits under **282,592 bytes** and all frames decode.
3. Do not alter `APP_RES_BASE`, `APP_RES_SECTORS`, saved user data or XDATA regions to make an oversized candidate appear to fit.
4. The standard branch, manual `run_build=true` CircleCI configuration and working audio/control code remain untouched.

Miku-FM's few-pose/dirty-rectangle optimisations are not needed for this normal-animation problem. No standalone ENV/LFO workaround is introduced.
