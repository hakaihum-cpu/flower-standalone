#!/usr/bin/env python3
"""628-frame DIRECT 16-colour RGB565 pixel-art reference encoder.

EXPERIMENT ONLY: output .m16p is NOT an installable FM-1 res.bin.
No VQ dictionaries or painterly reconstruction. Preserves all 96x96
quantized pixels and original frame chronology. Never modifies firmware.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
N, CLIP, CLIPS, SIDE = 628, 27, 24, 96
FRAME_BYTES = SIDE * SIDE // 2
RES_MAX = 69 * 4096 - 32
HDR = struct.Struct('<4sHHHHIII')
ENTRY = struct.Struct('<III')
PAL, DIRECTORY, BODY = HDR.size, HDR.size + CLIPS * 32, HDR.size + CLIPS * 32 + CLIPS * ENTRY.size
assert BODY == 1080


def to565(rgb):
    return ((rgb[0] >> 3) << 11) | ((rgb[1] >> 2) << 5) | (rgb[2] >> 3)


def from565(c):
    return ((c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63,
            (c & 31) * 255 // 31)


def verify_sheets(folder, files, manifest):
    if len(files) != len(manifest["files"]):
        raise ValueError("source bank count mismatch")
    for (name, _, _, count), row in zip(files, manifest["files"]):
        if name != row["name"] or count != row["frames"]:
            raise ValueError("original source image identity/count mismatch")
        sha = hashlib.sha256((folder / name).read_bytes()).hexdigest()
        if sha != row["sha256"]:
            raise ValueError("source sheet differs from approved archive: " + name)


def build(folder: Path, output: Path, report: Path, preview: Path | None):
    sys.path.insert(0, str(ROOT / "fm1/native/tools"))
    import photo96_sources as src
    src.base = folder
    manifest = json.loads((ROOT / "Resources/fm1_photo_96_chunks/manifest.json").read_text())
    verify_sheets(folder, src.files, manifest)
    frames = [src.get(b, i).convert("RGB")
              for b, (_, _, _, count) in enumerate(src.files) for i in range(count)]
    if len(frames) != N or any(im.size != (SIDE, SIDE) for im in frames):
        raise ValueError("source frames are not 628 x 96x96")

    palettes, compressed, checksums, sizes, samples = [], [], [], [], []
    for cl in range(CLIPS):
        start = cl * CLIP
        batch = frames[start:min(start + CLIP, N)]
        atlas = Image.new("RGB", (len(batch) * SIDE, SIDE))
        for i, image in enumerate(batch):
            atlas.paste(image, (i * SIDE, 0))
        q = atlas.quantize(colors=16, method=Image.Quantize.MEDIANCUT,
                           dither=Image.Dither.NONE)
        qraw = q.getpalette()
        pal565 = [to565(qraw[i * 3:i * 3 + 3]) for i in range(16)]
        lut = Image.new("P", (1, 1))
        lut.putpalette([v for color in map(from565, pal565) for v in color])
        palettes.append(struct.pack("<16H", *pal565))
        items = []
        for i, image in enumerate(batch):
            indexed = image.quantize(palette=lut, dither=Image.Dither.NONE)
            pixels = indexed.tobytes()
            if len(pixels) != SIDE * SIDE or max(pixels) > 15:
                raise ValueError("bad source pixel index")
            packed = bytes((pixels[k] << 4) | pixels[k + 1]
                           for k in range(0, len(pixels), 2))
            assert len(packed) == FRAME_BYTES
            items.append(packed)
            if i == 0 and cl in (0, 5, 12, 19, 22):
                samples.append((image, indexed.convert("RGB")))
        raw = b"".join(items)
        comp = zlib.compress(raw, 9)
        if zlib.decompress(comp) != raw:
            raise AssertionError("direct16 lossless roundtrip failed")
        compressed.append(comp)
        checksums.append(zlib.crc32(raw) & 0xFFFFFFFF)
        sizes.append(len(raw))

    cursor = BODY
    entries = []
    for blob, crc in zip(compressed, checksums):
        entries.append((cursor, len(blob), crc))
        cursor += len(blob)
    result = HDR.pack(b"M16P", 1, SIDE, N, CLIPS,
                      PAL, DIRECTORY, BODY)
    result += b"".join(palettes)
    result += b"".join(ENTRY.pack(*e) for e in entries)
    result += b"".join(compressed)
    assert len(result) == cursor
    for cl, (at, length, crc) in enumerate(entries):
        value = zlib.decompress(result[at:at + length])
        assert len(value) == sizes[cl]
        assert (zlib.crc32(value) & 0xFFFFFFFF) == crc

    info = {
        "frames": N, "source_banks": 8, "side": SIDE,
        "max_colours_per_clip": 16, "no_vq": True,
        "no_dither": True, "image_bytes": len(result),
        "res_bin_payload_max": RES_MAX,
        "bytes_over_capacity": max(0, len(result) - RES_MAX),
        "fits_res_bin": len(result) <= RES_MAX,
        "sha256": hashlib.sha256(result).hexdigest(),
        "decoded_equal_to_quantized_pixels": True,
        "original_full_colour_lossless": False,
        "firmware_modified": False,
        "warning": "Raw experiment, NOT installable as res.bin or fwsc",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(result)
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(info, ensure_ascii=False, indent=2))
    if preview:
        sheet = Image.new("RGB", (240 * len(samples), 480), "#222")
        for n, (original, indexed) in enumerate(samples):
            sheet.paste(original.resize((240, 240), Image.Resampling.NEAREST),
                        (n * 240, 0))
            sheet.paste(indexed.resize((240, 240), Image.Resampling.NEAREST),
                        (n * 240, 240))
        preview.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(preview)
    print(json.dumps(info, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--source-dir", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path,
                   help="Experimental .m16p only, never a firmware resource")
    p.add_argument("--report", required=True, type=Path)
    p.add_argument("--preview", type=Path)
    args = p.parse_args()
    build(args.source_dir, args.output, args.report, args.preview)
