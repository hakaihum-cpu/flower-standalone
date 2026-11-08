#!/usr/bin/env python3
"""Materialize the user's ENV/LFO/logo/start 16-colour native FM-1 assets.

The binary pack is checked into the same repository; no source downloads,
external packages, AI image generation, or runtime zlib dependencies.
Exact source input: env.png, LFO.png, logo.png, start.png supplied by user.
All 100 frames of ENV and LFO are present in their original tile order.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
import struct
import zlib
from pathlib import Path

PACK = Path(__file__).resolve().with_name("mi_ui_env_lfo_logo_start_16color.b64")
EXPECTED_SHA256 = "e521e1bccaf19001298d1f699256fd2a37065f7185722758942d2b4aaca253de"
RAW_SIZE = 46669
MOTION_FRAMES = 100
MOTION_MAP_BYTES = 256
MOTION_BOOK_BYTES = 1280
MOTION_BASE_BYTES = 2560
MOTION_OFFSET_BYTES = 200
LOGO_W, LOGO_H = 106, 28
START_W, START_H = 240, 240


def read_pack():
    try:
        encoded = PACK.read_text(encoding="ascii").strip()
        data = zlib.decompress(base64.b64decode(encoded, validate=True))
    except (OSError, ValueError, zlib.error) as exc:
        raise SystemExit("FAIL: invalid original-source UI animation pack") from exc
    if len(data) != RAW_SIZE or hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise SystemExit("FAIL: ENV/LFO/logo/start raw pack length or SHA256 mismatch")
    if data[:4] != b"MI16":
        raise SystemExit("FAIL: wrong UI media pack magic")
    elen, llen = struct.unpack_from("<II", data, 4)
    if (elen, llen) != (14426, 7627):
        raise SystemExit("FAIL: unexpected ENV/LFO motion delta sizes")
    return data, elen, llen


def u8_array(name, data):
    lines = ["    " + ",".join(f"0x{v:02x}" for v in data[i:i+24]) + ","
             for i in range(0, len(data), 24)]
    return (f"static const uint8_t {name}[{len(data)}u] = {{\n"
            + "\n".join(lines) + "\n};\n")


def u16_array(name, data):
    assert len(data) % 2 == 0
    vals = struct.unpack("<"+str(len(data)//2)+"H",data)
    lines = ["    "+",".join(f"0x{v:04x}" for v in vals[i:i+12])+"," for i in range(0,len(vals),12)]
    return (f"static const uint16_t {name}[{len(vals)}u] = {{\n"
            + "\n".join(lines) + "\n};\n")


def verify_motion(tag, books, base, offsets, deltas):
    o = struct.unpack("<100H", offsets)
    for clip in range(10):
        current = bytearray(base[clip*MOTION_MAP_BYTES:(clip+1)*MOTION_MAP_BYTES])
        for frame in range(clip*10,clip*10+10):
            if frame % 10:
                p = o[frame]
                if p + 32 > len(deltas):raise SystemExit(f"FAIL: {tag} delta offset")
                mask = deltas[p:p+32]
                n = sum(int(x).bit_count() for x in mask)
                if p + 32 + n > len(deltas):raise SystemExit(f"FAIL: {tag} delta payload")
                it = iter(deltas[p+32:p+32+n])
                for k in range(MOTION_MAP_BYTES):
                    if mask[k>>3]&(0x80>>(k&7)):
                        current[k] = next(it)
            # A 3x3 4bpp book has exactly 256 entries of 5 bytes.
            if len(current) != MOTION_MAP_BYTES or len(books) != 1280:
                raise SystemExit(f"FAIL: {tag} VQ geometry")
    return True


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--out",type=Path,required=True)
    args=ap.parse_args()
    raw,elen,llen=read_pack()
    pos=12
    outputs = [
      "/* User-supplied ENV (100), LFO (100), logo, startup source; 16 RGB565. */\n",
      "#ifndef MI_UI_MEDIA_H\n#define MI_UI_MEDIA_H\n#include <stdint.h>\n",
      "#define MI_UI_MOTION_W 48u\n#define MI_UI_MOTION_H 48u\n",
      "#define MI_UI_MOTION_COUNT 100u\n#define MI_UI_MOTION_CLIP 10u\n",
      "#define MI_UI_MOTION_MAP_BYTES 256u\n#define MI_UI_MOTION_BOOK_BYTES 1280u\n",
      "#define MI_UI_LOGO_W 106u\n#define MI_UI_LOGO_H 28u\n",
      "#define MI_UI_START_W 240u\n#define MI_UI_START_H 240u\n",
    ]
    for tag,n in (("env",elen),("lfo",llen)):
        palette=raw[pos:pos+32];pos+=32
        books=raw[pos:pos+1280];pos+=1280
        bases=raw[pos:pos+2560];pos+=2560
        offsets=raw[pos:pos+200];pos+=200
        delta=raw[pos:pos+n];pos+=n
        verify_motion(tag,books,bases,offsets,delta)
        outputs += [u16_array(f"mi_ui_{tag}_palette",palette),
                    u8_array(f"mi_ui_{tag}_books",books),
                    u8_array(f"mi_ui_{tag}_bases",bases),
                    u16_array(f"mi_ui_{tag}_offsets",offsets),
                    u8_array(f"mi_ui_{tag}_deltas",delta)]
    palette=raw[pos:pos+32];pos+=32
    pixels=raw[pos:pos+(LOGO_W*LOGO_H//2)];pos+=len(pixels)
    if len(pixels)!=(LOGO_W*LOGO_H//2):raise SystemExit("FAIL: logo pixels")
    outputs += [u16_array("mi_ui_logo_palette",palette),u8_array("mi_ui_logo_pixels",pixels)]
    palette=raw[pos:pos+32];pos+=32
    book=raw[pos:pos+512];pos+=512
    maps=raw[pos:pos+(START_W*START_H//4)];pos+=len(maps)
    if len(maps)!=START_W*START_H//4 or pos!=len(raw):
        raise SystemExit("FAIL: startup VQ geometry/pack trailing bytes")
    outputs += [u16_array("mi_ui_start_palette",palette),
                u8_array("mi_ui_start_books",book),
                u8_array("mi_ui_start_maps",maps),"#endif\n"]
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text("".join(outputs),encoding="ascii")
    print(f"PASS: 100 ENV + 100 LFO frames, 16 RGB565; logo and 240x240 start 16 RGB565; SHA256 {EXPECTED_SHA256}")

if __name__=="__main__":main()
