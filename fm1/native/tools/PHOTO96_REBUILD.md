# MIYAKO FM-1 96x96 16-colour original photo resource

Rebuilt from the **eight original user-provided contact sheets**, not recolored
from the old lossy 48x48 asset. 628 frames: 81+64+100+81+81+81+70+70.
The original shapes, poses and pixel edges are retained. Colour assignments
on grayscale source are an artistic interpretation of the supplied warm
16-colour example; true historical colours are not recoverable.

Palette: 16 real RGB565 shades. 8x4 tiles, 256 codewords per 27 frames;
24 codebooks, 288 tile IDs/frame. Exact payload: 279936 B, fm1-base adds a
32-byte CRC resource header = 279968 B. The 69-sector resource range ends
before 0xDC000 saved settings. Old res.bin files (119552 B) are incompatible.

The binary chunks and original source filenames/hashes are tracked in
Resources/fm1_photo_96_chunks/manifest.json. CI verifies ALL 628 decoded
frames, uses the standard fm1-base resource builder and remains manual.

Recreate from the original eight sheets (optional offline procedure):

```sh
python -m pip install Pillow numpy scikit-learn
FM1_USER_SHEETS=/path/to/user_sheets FM1_RESOURCE_OUTPUT=/tmp/photo96 \
    python fm1/native/tools/photo96_rebuild.py
```

Verify the new payload SHA256 and VQ-decoded preview before replacing chunks.
Do not generate or reconstruct missing facial imagery.
