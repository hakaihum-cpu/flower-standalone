#!/usr/bin/env python3
"""Host compiler ONLY: stand-in declarations, never media and never firmware.

Must write into /tmp/fm1-base/miyako/ ONLY. The real user-supplied
original EFFECTS pack overwrites this file after successful encoding.
Fail the target firmware build if this marker persists.
"""
from pathlib import Path
import argparse

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("target",type=Path)
    args=ap.parse_args()
    path=args.target.resolve()
    if path.name != "mi_effects_vq_frames.h" or "fm1-base" not in path.parts or "miyako" not in path.parts:
        raise SystemExit("FAIL: refuse to write a compile-only stub outside FM1 test workspace")
    if path.exists():
        raise SystemExit("FAIL: do not replace actual original-source media with test stubs")
    path.write_text("""/* COMPILER PRECHECK ONLY; ZERO FRAMES ARE NOT USER MEDIA. */
#define MI_EFFECTS_MEDIA_PREFLIGHT_STUB 1
#define MI_EFFECTS_ASSET_COUNT 144u
#define MI_EFFECTS_ASSET_W 128u
#define MI_EFFECTS_ASSET_H 78u
#define MI_EFFECTS_ASSET_BYTES 4992u
#define MI_EFFECTS_ASSET_COLORS 16u
#define MI_EFFECTS_ASSET_BPP 4u
static const uint16_t mi_effects_vq_palette[4][16] = {{0}};
static const uint8_t mi_effects_vq_tilebook[4][256][16] = {{{0}}};
static const uint8_t mi_effects_vq_maps[144][320] = {{0}};
""",encoding="ascii")
    print("Compile-only C header staged. THIS HEADER MUST NEVER BE PACKAGED.")

if __name__=="__main__":
    main()
