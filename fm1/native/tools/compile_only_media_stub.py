#!/usr/bin/env python3
"""Create strictly compile-only declarations for the two source video packs.

These are NOT user pictures and MUST NOT be packaged in an installable FWSC.
The real headers replace BOTH before the final host and JieLi build.
"""
from pathlib import Path
import argparse

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("target",type=Path)
    a=ap.parse_args()
    p=a.target.resolve()
    if "fm1-base" not in p.parts or "miyako" not in p.parts:
        raise SystemExit("FAIL: fake media forbidden outside pinned MIYAKO preflight workspace")
    if p.exists():
        raise SystemExit("FAIL: never overwrite real media with compiler stubs")
    if p.name=="mi_effects_vq_frames.h":
        tag,frames,clips="EFFECTS",144,4
    elif p.name=="mi_eureka_vq_frames.h":
        tag,frames,clips="EUREKA",576,16
    else:
        raise SystemExit("FAIL: unknown generated video header")
    prefix=tag.lower()
    p.write_text(
        "/* COMPILER PRECHECK ONLY: NO USER MEDIA PRESENT. */\n"
        f"#define MI_{tag}_MEDIA_PREFLIGHT_STUB 1\n"
        f"#define MI_{tag}_ASSET_COUNT {frames}u\n"
        f"#define MI_{tag}_ASSET_W 128u\n"
        f"#define MI_{tag}_ASSET_H 78u\n"
        f"#define MI_{tag}_ASSET_BYTES 4992u\n"
        f"#define MI_{tag}_ASSET_COLORS 16u\n"
        f"#define MI_{tag}_ASSET_BPP 4u\n"
        f"static const uint16_t mi_{prefix}_vq_palette[{clips}][16] = {{0}};\n"
        f"static const uint8_t mi_{prefix}_vq_tilebook[{clips}][256][16] = {{0}};\n"
        f"static const uint8_t mi_{prefix}_vq_maps[{frames}][320] = {{0}};\n",
        encoding="ascii")
    print(f"Compiler-only {tag} types staged; not real source frames.")

if __name__=="__main__":
    main()
