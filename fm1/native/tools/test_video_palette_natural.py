#!/usr/bin/env python3
"""FM-1 non-generative, 16-colour/36-frame palette regressions.

Tests the real EFFECTS and EUREKA pre-VQ encoders. This does NOT claim
perceptual quality or on-device performance without actual user screenshots.
"""
import sys
from pathlib import Path
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mi_palette import clip_pixels as effects_palette
from pack_eureka_frames import clip_pixels as eureka_palette
from mi_reference_palette import REFERENCE_565, natural_pixels
from pack_miyako_resource import read_payload, CHUNK_DIR, SHA256
import hashlib
import re

# The active 628-frame Runway recolor uses 24 independent RGB565 palettes.
# It is decoded from MHP2, not the archived original-source VQ resource.
resource = read_payload(CHUNK_DIR)
assert SHA256 == "2426992e3f5c06870a654c49086923293e6c00247a1796f9f14048824bf1c17c"
assert hashlib.sha256(resource).hexdigest() == SHA256
app_source = (Path(__file__).resolve().parents[1] / "app.c").read_text()
header = (Path(__file__).resolve().parents[1] / "mi_resource_media.h").read_text()
assert re.search(r"static\s+uint16_t\s+mi_photo_palette\[16\];", app_source)
assert "res=plat_res(length);" in app_source
assert "res=mi_photo_source(&length);" in app_source
assert "mi_mhp2_decode(&mi_photo_decoder" in app_source
assert "mi_mhp2_apply_patch(res+MI_TILE_MHP2_CORE_BYTES" in app_source
assert "mi_photo_palette[i]=mi_photo_decoder.palette[i];" in app_source
assert "MI_TILE_RESOURCE_RGB_SOURCE 1u" in header
assert "mi_photo_grayscale_palette" not in app_source
assert "mi_photo_palette[idx]" in app_source

W, H = 128, 78
# Varying luminance, colors and high-contrast contours, no face synthesis.
frames = []
for t in range(36):
    im = Image.new("RGB", (W, H))
    pix = im.load()
    for y in range(H):
        for x in range(W):
            tone = (x * 3 + y * 2 + t) % 190 + 15
            if 36 < x < 95 and 10 < y < 67:
                pix[x, y] = (min(255, tone + 46), tone // 2 + 25,
                             tone // 3 + 20)
            else:
                pix[x, y] = (tone // 2 + 20, tone // 2 + 15,
                             min(255, tone + 8))
    frames.append(im)
before = [im.tobytes() for im in frames]
for name, run in (("EFFECTS", effects_palette), ("EUREKA", eureka_palette)):
    palette, coded = run(frames)
    source_palette, source_coded = natural_pixels(frames)
    assert (palette, coded) == (source_palette, source_coded), (
        name, "encoder altered original RGB before 16-color quantization")
    assert len(palette) == 16
    assert tuple(palette) != REFERENCE_565, (name, "old sepia palette still applied")
    assert len(coded) == 36 and all(len(b) == W * H // 2 for b in coded)
    assert all(0 <= c <= 65535 for c in palette)
    assert len(set(palette)) >= 12, (name, "too few distinct palette colors")
    all_indices = set()
    for b in coded:
        for value in b:
            all_indices.add(value >> 4)
            all_indices.add(value & 15)
    assert len(all_indices) >= 10, (name, "too few utilized sample colors")
    assert all(im.tobytes() == original for im, original in zip(frames, before))
    # The input clip stays unchanged, 16-color palette and all 36 frames kept.
    print(f"PASS {name}: 36 frames, 16 palette entries; "
          f"{len(all_indices)} indices, source unchanged")
