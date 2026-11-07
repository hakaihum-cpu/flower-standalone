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
from mi_reference_palette import REFERENCE_565
import re

# The embedded resource-photo renderer and both video encoders MUST use
# the SAME real palette extracted from the supplied 16-color reference.
app_source = (Path(__file__).resolve().parents[1] / "app.c").read_text()
ref_match = re.search(
    r"mi_user_reference_rgb565\[16\]\s*=\s*\{([^}]+)\}", app_source,
    re.S,
)
assert ref_match, "C renderer is missing the actual reference palette"
ref_c = tuple(int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{4})u", ref_match.group(1)))
assert ref_c == REFERENCE_565 and len(set(ref_c)) == 16, (
    "C photo palette must match Python EFFECTS/EUREKA palette exactly"
)

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
    assert len(palette) == 16
    assert tuple(palette) == REFERENCE_565
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
