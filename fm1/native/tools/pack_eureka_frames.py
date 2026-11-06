#!/usr/bin/env python3
"""Pack ALL 576 user-supplied EUREKA video frames for the COLOUR FM-1 TFT.

Input is the original, SHA-verified CRF1 pack created by
scripts/materialize_miyako_eureka_video.py. This never generates new
artwork, selects a subset of clips, or substitutes waveform graphics.
The 128x78 viewport is a display resize only; clip/frame order is retained.
Each 36-frame clip uses a fixed 16-colour RGB565 palette to retain source colour
and avoid palette flicker. Packed 4bpp pixels are XOR-delta/RLE compressed.
The output is 16-colour quantised, NOT original 24-bit colour.
Target flash acceptance remains the firmware linker's responsibility.
If it does not fit, fail the build; never reduce frames or substitute bars.
"""
from __future__ import annotations

import argparse
import hashlib
import io
from pathlib import Path
import struct
import zlib
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Resources" / "miyako_eureka_frames.pack"
COUNT = 16 * 36
CLIP_LENGTH = 36
WIDTH, HEIGHT = 128, 78
COLORS = 16
FRAME_BYTES = WIDTH * HEIGHT // 2
# App-area ceiling from the pinned FM-1/KARMA flash partition.
# Every byte of media is included in that app image alongside actual DSP/UI code.
# This check rejects impossible media before a costly target compilation.
APP_SLOT_BYTES = 0x8DFBC
PALETTE_BYTES = (COUNT // CLIP_LENGTH) * COLORS * 2
OFFSET_BYTES = (COUNT + 1) * 4


def image_rgb(jpg: bytes) -> Image.Image:
    """Reconstruct the full user frame, proportionally scaled and letterboxed."""
    with Image.open(io.BytesIO(jpg)) as im:
        im.load()
        fitted = ImageOps.contain(im.convert("RGB"), (WIDTH, HEIGHT),
                                  Image.Resampling.LANCZOS)
        canvas = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
        canvas.paste(fitted, ((WIDTH - fitted.width) // 2,
                              (HEIGHT - fitted.height) // 2))
        return canvas


def clip_pixels(frames: list[Image.Image]) -> tuple[list[int], list[bytes]]:
    """Shared per-clip palette: colour, face midtones and stable brightness.

    Clip-wide luma mapping prevents frame-to-frame auto-contrast flicker.
    16-colour median-cut palette is FIXED throughout the 36-frame clip.
    No dithering: noisy pixel changes destroy 576-frame delta compression.
    """
    if len(frames) != CLIP_LENGTH:
        raise ValueError(f"expected {CLIP_LENGTH} original frames per clip")
    hist = [0] * 256
    ycc = []
    for im in frames:
        y, cb, cr = im.convert("YCbCr").split()
        for value, count in enumerate(y.histogram()):
            hist[value] += count
        ycc.append((y, cb, cr))

    def percentile_cut(count: int, *, reverse: bool) -> int:
        total = 0
        indices = range(255, -1, -1) if reverse else range(256)
        for value in indices:
            total += hist[value]
            if total > count:
                return value
        return 0

    cutoff = sum(hist) // 100
    lo = percentile_cut(cutoff, reverse=False)
    hi = percentile_cut(cutoff, reverse=True)
    if hi <= lo:
        raise ValueError("source clip has no luminance range")
    # Gentle lift of dark skin/hair tones without destroying colour.
    lut = [max(0, min(255, round(255 * (
        max(0.0, min(1.0, (v - lo) / (hi - lo))) ** 0.75))))
        for v in range(256)]
    fixed = [Image.merge("YCbCr", (y.point(lut), cb, cr)).convert("RGB")
             for y, cb, cr in ycc]

    montage = Image.new("RGB", (WIDTH, HEIGHT * CLIP_LENGTH))
    for i, im in enumerate(fixed):
        montage.paste(im, (0, i * HEIGHT))
    colors = montage.quantize(colors=COLORS - 1,
                              method=Image.Quantize.MEDIANCUT,
                              dither=Image.Dither.NONE).getpalette()
    rgb = [(0, 0, 0)] + [tuple(colors[i * 3:i * 3 + 3])
                         for i in range(COLORS - 1)]
    if len(rgb) != COLORS:
        raise ValueError("incomplete RGB palette")
    # Exactly sixteen palette entries; the FM-1 draws each as RGB565.
    pal = Image.new("P", (1, 1))
    pal.putpalette([c for color in rgb for c in color])
    palette565 = [((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                  for r, g, b in rgb]
    pixels = []
    for im in fixed:
        indices = im.quantize(palette=pal, dither=Image.Dither.NONE).tobytes()
        if len(indices) != WIDTH * HEIGHT or max(indices) >= COLORS:
            raise ValueError("Pillow produced out-of-range palette indices")
        pixels.append(bytes((indices[i] << 4) | indices[i + 1]
                            for i in range(0, len(indices), 2)))
    return palette565, pixels


def encode_delta(delta: bytes) -> bytes:
    """Three bounded packet types: zero run, equal-byte run, literal."""
    out = bytearray()
    i, length = 0, len(delta)
    while i < length:
        if delta[i] == 0:
            j = i + 1
            while j < length and j - i < 64 and delta[j] == 0:
                j += 1
            out.append(j - i - 1)  # 0x00..0x3f
            i = j
            continue
        j = i + 1
        while j < length and j - i < 64 and delta[j] == delta[i]:
            j += 1
        if j - i >= 3:
            out.extend((0x40 | (j - i - 1), delta[i]))
            i = j
            continue
        start = i
        i += 1
        while i < length and i - start < 128:
            if delta[i] == 0:
                break
            k = i + 1
            while k < length and k - i < 3 and delta[k] == delta[i]:
                k += 1
            if k - i >= 3:
                break
            i += 1
        out.append(0x80 | (i - start - 1))
        out.extend(delta[start:i])
    return bytes(out)


def decode_delta(dst: bytearray, data: bytes) -> None:
    i = j = 0
    while i < len(data):
        op = data[i]
        i += 1
        n = (op & 0x3f) + 1 if op < 0x80 else (op & 0x7f) + 1
        if op < 0x40:
            j += n
        elif op < 0x80:
            val = data[i]
            i += 1
            for k in range(n):
                dst[j + k] ^= val
            j += n
        else:
            for k in range(n):
                dst[j + k] ^= data[i + k]
            i += n
            j += n
        if j > len(dst):
            raise ValueError("EUREKA RLE overrun")
    if j != len(dst):
        raise ValueError(f"EUREKA RLE underfill: {j} != {len(dst)}")


def write_header(path: Path, blocks: bytes, offsets: list[int],
                 palettes: list[list[int]], decoded_crc32: int) -> None:
    def c_array(items, per_line=16):
        return ",\n".join("    " + ", ".join(str(v) for v in items[i:i + per_line])
                           for i in range(0, len(items), per_line))
    if len(offsets) != COUNT + 1 or len(palettes) != COUNT // CLIP_LENGTH:
        raise SystemExit("FAIL: colour EUREKA index/palette count is incomplete")
    payload_bytes = len(blocks) + OFFSET_BYTES + PALETTE_BYTES
    print(f"INFO: EUREKA RGB565/16 colour payload {payload_bytes} bytes "
          f"(compressed frames {len(blocks)}, offsets {OFFSET_BYTES}, "
          f"palettes {PALETTE_BYTES}); app slot {APP_SLOT_BYTES} bytes "
          "INCLUDING all firmware code")
    if payload_bytes >= APP_SLOT_BYTES:
        raise SystemExit(
            "FAIL: 16-colour EUREKA video alone exceeds the 581564-byte FM-1 "
            f"app slot by {payload_bytes - APP_SLOT_BYTES} bytes, BEFORE firmware "
            "code. No frame removal, 2-colour fallback, lower resolution, "
            "or substitute animation allowed. Requires a new proven storage/"
            "codec design; aborting before target build.")
    digest = hashlib.sha256(blocks).hexdigest()
    header = (
        "/* Generated from ORIGINAL user-provided MIYAKO EUREKA videos.\n"
        " * 16 clips x 36 frames; no replacement art or omitted frames.\n"
        " * Full-colour FM-1 LCD, source converted to per-clip 16-colour RGB565 palettes.\n"
        " * sha256 of RLE stream: " + digest + "\n"
        " * Generated by fm1/karma/pack_eureka_frames.py; do not hand-edit.\n"
        " */\n"
        "#define MI_EUREKA_ASSET_COUNT 576u\n"
        "#define MI_EUREKA_ASSET_W 128u\n"
        "#define MI_EUREKA_ASSET_H 78u\n"
        "#define MI_EUREKA_ASSET_BYTES 4992u\n"
        "#define MI_EUREKA_ASSET_COLORS 16u\n"
        "#define MI_EUREKA_ASSET_BPP 4u\n"
        f"#define MI_EUREKA_DECODED_CRC32 0x{decoded_crc32:08X}u\n"
        "static const uint16_t mi_eureka_palette[16][16] = {\n"
        + ",\n".join("    {" + ", ".join(f"0x{v:04X}u" for v in palette) + "}"
                     for palette in palettes) + "\n};\n"
        "static const uint32_t mi_eureka_off[577] = {\n"
        + c_array(offsets, 8) + "\n};\n"
        "static const uint8_t mi_eureka_data[] = {\n"
        + c_array(list(blocks)) + "\n};\n"
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(header, encoding="ascii")
    print(f"PASS: EUREKA COLOR RGB565/16 palette, 576/576 frames, 16 clips, RLE={len(blocks)} bytes"
          f" + offsets={len(offsets)*4} bytes, sha256={digest}, decoded_crc32={decoded_crc32:08X}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_header", type=Path)
    a = parser.parse_args()
    src = SOURCE.read_bytes()
    if src[:4] != b"CRF1" or int.from_bytes(src[4:8], "little") != COUNT:
        raise SystemExit("FAIL: source is not approved 576-frame CRF1 EUREKA pack")
    offsets, blob, palettes = [0], bytearray(), []
    decoded_crc32 = 0
    unique = set()
    for clip in range(COUNT // CLIP_LENGTH):
        source_frames = []
        for i in range(CLIP_LENGTH):
            f = clip * CLIP_LENGTH + i
            offset, flags, size = struct.unpack_from("<III", src, 8 + f * 12)
            _ = flags
            if size < 64 or offset < 8 + COUNT * 12 or offset + size > len(src):
                raise SystemExit(f"FAIL: invalid EUREKA frame boundary {f}")
            jpg = src[offset:offset + size]
            if not (jpg.startswith(b"\xff\xd8") and jpg.endswith(b"\xff\xd9")):
                raise SystemExit(f"FAIL: invalid user JPEG at frame {f}")
            source_frames.append(image_rgb(jpg))
        palette, clip_frames = clip_pixels(source_frames)
        palettes.append(palette)
        last = bytearray(FRAME_BYTES)
        for i, pixels in enumerate(clip_frames):
            f = clip * CLIP_LENGTH + i
            decoded_crc32 = zlib.crc32(pixels, decoded_crc32)
            unique.add(hashlib.sha256(pixels).digest())
            delta = bytes(a ^ b for a, b in zip(pixels, last))
            compressed = encode_delta(delta)
            test = bytearray(last)
            decode_delta(test, compressed)
            if test != pixels:
                raise SystemExit(f"FAIL: round-trip mismatch at frame {f}")
            blob.extend(compressed)
            offsets.append(len(blob))
            last[:] = pixels
        print(f"INFO: EUREKA colour clip {clip + 1}/16,"
              f" ROM stream={len(blob)} bytes; 36/36 original frames")
    if len(palettes) != 16 or len(offsets) != 577 or len(unique) < 100:
        raise SystemExit("FAIL: missing or duplicate original EUREKA frames")
    write_header(a.output_header.resolve(), bytes(blob), offsets,
                 palettes, decoded_crc32)


if __name__ == "__main__":
    main()
