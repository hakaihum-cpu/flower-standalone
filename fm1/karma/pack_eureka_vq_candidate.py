#!/usr/bin/env python3
"""Experimental FM-1 EUREKA 16-colour 8x4 vector-quantised tile codec.

This is the explicitly selected 16-colour VQ candidate build path. It is
tested and quality-gated in CI before any target firmware compilation.
All 576 source JPEG frames are used. Additional lossy tile quantisation is
reported with per-frame RGB PSNR, never concealed as exact original pixels.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import math
import os
import struct
import zlib
from pathlib import Path

os.environ.setdefault("OPENBLAS_NUM_THREADS", "1")
import numpy as np
from PIL import Image, ImageOps

from pack_eureka_frames import clip_pixels

WIDTH, HEIGHT = 128, 78
PAD_H = 80
CLIPS, FRAMES = 16, 36
TILE_W, TILE_H, TILE_PIXELS, TILE_BYTES = 8, 4, 32, 16
TILE_COUNT, DICT_SIZE = 320, 256
APP_SLOT = 0x8DFBC
MEDIA_BYTES = CLIPS * (DICT_SIZE * TILE_BYTES + FRAMES * TILE_COUNT + 32)


def rgb_palette(pal565):
    pal = np.asarray(pal565, dtype=np.uint16)
    r, g, b = pal >> 11, (pal >> 5) & 63, pal & 31
    return np.stack([((r << 3) | (r >> 2)) & 255,
                     (g << 2) | (g >> 4), (b << 3) | (b >> 2)], axis=1).astype(np.uint8)


def source_indices(fourbit):
    if len(fourbit) != FRAMES:
        raise ValueError("EUREKA: expected 36 original frames per clip")
    src = np.frombuffer(b"".join(fourbit), dtype=np.uint8)
    if src.size != FRAMES * WIDTH * HEIGHT // 2:
        raise ValueError("EUREKA: source 4bpp length mismatch")
    packed = src.reshape(FRAMES, HEIGHT, WIDTH // 2)
    result = np.zeros((FRAMES, PAD_H, WIDTH), dtype=np.uint8)
    result[:, :HEIGHT, 0::2] = packed >> 4
    result[:, :HEIGHT, 1::2] = packed & 15
    return result


def tile_extract(frames):
    if frames.shape != (FRAMES, PAD_H, WIDTH):
        raise ValueError("EUREKA: input geometry mismatch")
    return frames.reshape(FRAMES, 20, 4, 16, 8).transpose(0, 1, 3, 2, 4).reshape(-1, 32)


def tile_expand(codebook, ids):
    return codebook[ids].reshape(FRAMES, 20, 16, 4, 8).transpose(0, 1, 3, 2, 4).reshape(FRAMES, PAD_H, WIDTH)


def train_clip(original, palette565, loops=6, frame_balance=0.0):
    """Deterministic RGB-aware K-means; 256 codebook tiles per 36-frame clip."""
    tiles = tile_extract(original)
    unique = np.unique(tiles, axis=0)
    if len(unique) < 2:
        raise ValueError("EUREKA: source clip is unexpectedly static")
    codebook = unique[np.linspace(0, len(unique) - 1, DICT_SIZE, dtype=np.int32)].copy()
    codebook[0] = 0
    pal = rgb_palette(palette565)
    v = pal[tiles].reshape(-1, TILE_PIXELS * 3).astype(np.float32)
    norms = np.einsum("ij,ij->i", v, v)
    ids = np.zeros(len(tiles), dtype=np.uint8)
    for _ in range(loops):
        centroids = pal[codebook].reshape(DICT_SIZE, TILE_PIXELS * 3).astype(np.float32)
        cnorms = np.einsum("ij,ij->i", centroids, centroids)
        dist = norms[:, None] + cnorms[None, :] - 2 * (v @ centroids.T)
        ids = np.argmin(dist, axis=1).astype(np.uint8)
        means = np.empty((DICT_SIZE, TILE_PIXELS * 3), dtype=np.float32)
        means[0] = 0
        if frame_balance > 0:
            # Still a SINGLE 256-entry dictionary per clip and 8x4 tiles.
            # Bias Lloyd updates toward the frame with the highest error.
            # No frame, palette entry, tile size or bit depth is changed.
            losses = np.maximum(dist[np.arange(len(tiles)), ids], 1.0)
            frame_error = losses.reshape(FRAMES, TILE_COUNT).mean(axis=1)
            # Normalize BEFORE exponentiating: raw RGB squared errors can
            # overflow float32 for stronger frame balancing (e.g. power 6).
            relative_error = np.maximum(frame_error, 1.0)
            relative_error /= max(float(relative_error.mean()), 1.0)
            weights = relative_error.astype(np.float64) ** frame_balance
            weights = np.clip(weights / weights.mean(), 0.15, 8.0)
            tile_weights = np.repeat(weights, TILE_COUNT)
            weighted_sum = np.zeros((DICT_SIZE, v.shape[1]), dtype=np.float64)
            np.add.at(weighted_sum, ids, v * tile_weights[:, None])
            weight_totals = np.bincount(ids, weights=tile_weights,
                                        minlength=DICT_SIZE)
            means[1:] = np.where(weight_totals[1:, None] > 0,
                weighted_sum[1:] / np.maximum(weight_totals[1:, None], 1e-9),
                centroids[1:])
        else:
            for i in range(1, DICT_SIZE):
                group = v[ids == i]
                means[i] = group.mean(axis=0) if len(group) else centroids[i]
        means = means.reshape(DICT_SIZE, TILE_PIXELS, 3)
        cost = ((means[:, :, None, :] - pal[None, None, :, :]) ** 2).sum(axis=3)
        codebook[1:] = np.argmin(cost[1:], axis=2).astype(np.uint8)
        codebook[0] = 0
    # The final Lloyd update changes codebook entries after the last selector
    # assignment. Re-evaluate all 8x4 tile IDs against the ACTUAL dictionary
    # that will be stored in firmware. Otherwise an outdated selector map is
    # paired with a newer dictionary, introducing avoidable image errors.
    final_centroids = pal[codebook].reshape(DICT_SIZE, TILE_PIXELS * 3).astype(np.float32)
    final_norms = np.einsum("ij,ij->i", final_centroids, final_centroids)
    final_dist = norms[:, None] + final_norms[None, :] - 2 * (v @ final_centroids.T)
    ids = np.argmin(final_dist, axis=1).astype(np.uint8)
    ids = ids.reshape(FRAMES, TILE_COUNT)
    restored = tile_expand(codebook, ids)
    if restored.max() >= 16 or ids.size != 36 * TILE_COUNT:
        raise ValueError("EUREKA: failed tile map integrity")
    a = pal[original[:, :HEIGHT]].astype(np.float32)
    b = pal[restored[:, :HEIGHT]].astype(np.float32)
    mse = np.mean((a - b) ** 2, axis=(1, 2, 3))
    psnr = np.array([10 * math.log10(255 * 255 / max(float(e), 1e-9)) for e in mse])
    original_unique = len(set(hashlib.sha256(a.tobytes()).digest() for a in original))
    recon_unique = len(set(hashlib.sha256(a.tobytes()).digest() for a in restored))
    return codebook, ids, restored, (psnr.min(), psnr.mean(), original_unique, recon_unique)


def pack_nibbles(values):
    v = np.asarray(values, dtype=np.uint8).reshape(-1)
    if len(v) % 2 or v.max() >= 16:
        raise ValueError("EUREKA: invalid 4bpp palette index")
    packed = (v[0::2] << 4) | v[1::2]
    out = np.empty(len(v), dtype=np.uint8)
    out[0::2], out[1::2] = packed >> 4, packed & 15
    if not np.array_equal(v, out):
        raise ValueError("EUREKA: packed tile dictionary does not round-trip")
    return packed.tobytes()


def jpeg_frames(pack, clip):
    frames = []
    for f in range(clip * FRAMES, (clip + 1) * FRAMES):
        off, flags, n = struct.unpack_from("<III", pack, 8 + f * 12)
        _ = flags
        if off < 8 + 576 * 12 or n < 64 or off + n > len(pack):
            raise ValueError("EUREKA: missing source JPEG " + str(f))
        data = pack[off:off+n]
        if not (data.startswith(b"\xff\xd8") and data.endswith(b"\xff\xd9")):
            raise ValueError("EUREKA: corrupt JPEG " + str(f))
        with Image.open(io.BytesIO(data)) as im:
            im.load()
            fitted = ImageOps.contain(im.convert("RGB"), (WIDTH, HEIGHT), Image.Resampling.LANCZOS)
            canvas = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
            canvas.paste(fitted, ((WIDTH - fitted.width) // 2, (HEIGHT - fitted.height) // 2))
            frames.append(canvas)
    return frames


def write_candidate(path, palettes, books, selectors, decoded_crc32):
    if (len(palettes), len(books), len(selectors)) != (16, 16, 16):
        raise ValueError("EUREKA: all 16 clip payloads required")
    assert all(len(x) == DICT_SIZE * TILE_BYTES for x in books)
    assert all(len(x) == FRAMES * TILE_COUNT for x in selectors)
    assert MEDIA_BYTES == 250368
    if MEDIA_BYTES >= APP_SLOT:
        raise ValueError("EUREKA: dictionary media exceeds the app slot")
    def barray(data):
        return ",\n".join("    " + ", ".join(str(b) for b in data[i:i+32])
                           for i in range(0, len(data), 32))
    h = [
        "/* Lossy, PSNR-validated 576-frame 16-colour EUREKA VQ. */",
        "#define MI_EUREKA_ASSET_COUNT 576u",
        "#define MI_EUREKA_ASSET_W 128u",
        "#define MI_EUREKA_ASSET_H 78u",
        "#define MI_EUREKA_ASSET_BYTES 4992u",
        "#define MI_EUREKA_ASSET_COLORS 16u",
        "#define MI_EUREKA_ASSET_BPP 4u",
        "#define MI_EUREKA_VQ_TILE_W 8u",
        "#define MI_EUREKA_VQ_TILE_H 4u",
        "#define MI_EUREKA_VQ_TILE_COUNT 320u",
        f"#define MI_EUREKA_DECODED_CRC32 0x{decoded_crc32:08X}u",
        "static const uint16_t mi_eureka_vq_palette[16][16] = {"
    ]
    for ci, pal in enumerate(palettes):
        h.append("  {" + ", ".join(f"0x{color:04X}u" for color in pal) +
                 "}" + ("," if ci < 15 else ""))
    h.append("};")
    h.append("static const uint8_t mi_eureka_vq_tilebook[16][256][16] = {")
    for ci, book in enumerate(books):
        h.append(" {")
        for code in range(DICT_SIZE):
            t = book[code*16:(code+1)*16]
            h.append("  {" + ", ".join(str(v) for v in t) + "}" +
                     ("," if code < 255 else ""))
        h.append(" }" + ("," if ci < 15 else ""))
    h.append("};")
    h.append("static const uint8_t mi_eureka_vq_maps[576][320] = {")
    flat = b"".join(selectors)
    for f in range(576):
        h.append("  {" + ", ".join(str(v) for v in flat[f*320:(f+1)*320]) +
                 "}" + ("," if f < 575 else ""))
    h.append("};")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(h) + "\n", encoding="ascii")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("source", type=Path)
    p.add_argument("output_header", type=Path)
    p.add_argument("--preview", type=Path)
    p.add_argument("--min-psnr-db", type=float, default=26.0,
                   help="Minimum additional RGB PSNR per frame vs 16-colour source; "
                        "fail closed if image quality falls below this threshold")
    a = p.parse_args()
    source = a.source.read_bytes()
    if source[:4] != b"CRF1" or struct.unpack_from("<I", source, 4)[0] != 576:
        raise SystemExit("FAIL: not 576 original EUREKA frames")
    palettes, books, maps, worst = [], [], [], []
    decoded_crc32 = 0
    reconstructed = 0
    montage = Image.new("RGB", (256, 78 * 16)) if a.preview else None
    for clip in range(16):
        images = jpeg_frames(source, clip)
        palette, fourbit = clip_pixels(images)
        idx = source_indices(fourbit)
        # Improve the CURRENT 8x4 / 256-tile VQ dictionary only. Preserve the
        # original candidate as a guaranteed fallback if tuning is worse.
        base = train_clip(idx, palette)
        selected = "original"
        chosen = base
        if base[3][0] < a.min_psnr_db:
            for balance in (1.0, 2.0, 3.0, 4.0, 6.0):
                trial = train_clip(idx, palette, loops=18,
                                   frame_balance=balance)
                if (trial[3][0], trial[3][1]) > (chosen[3][0], chosen[3][1]):
                    chosen, selected = trial, f"frame-balanced {balance:g}"
        book, ids, recon, quality = chosen
        blob = pack_nibbles(book)
        if not np.array_equal(book, np.stack([np.frombuffer(blob, np.uint8) >> 4,
            np.frombuffer(blob, np.uint8) & 15], axis=-1).reshape(256, 32)):
            raise RuntimeError("FAIL: codebook packing corrupted original colour indices")
        # Independent C regression compares decoded 4bpp output CRC to this
        # Python-generated RGB565 palette-index frame sequence.
        for frame in recon[:, :HEIGHT]:
            pixels = pack_nibbles(frame)
            if len(pixels) != WIDTH * HEIGHT // 2:
                raise RuntimeError("FAIL: incomplete VQ source frame")
            decoded_crc32 = zlib.crc32(pixels, decoded_crc32)
            reconstructed += 1
        palettes.append(palette)
        books.append(blob)
        maps.append(ids.tobytes())
        worst.append(float(quality[0]))
        print(f"INFO clip {clip+1}/16 RGB PSNR worst={quality[0]:.2f} "
              f"mean={quality[1]:.2f} distinct={quality[3]}/36 vs input={quality[2]}/36 "
              f"dictionary={selected}")
        if montage is not None:
            pal = rgb_palette(palette)
            montage.paste(Image.fromarray(pal[idx[0, :HEIGHT]]), (0, clip * HEIGHT))
            montage.paste(Image.fromarray(pal[recon[0, :HEIGHT]]), (128, clip * HEIGHT))
    if reconstructed != 576 or len(palettes) != 16:
        raise RuntimeError(f"FAIL: VQ reconstructed {reconstructed}/576 original slots")
    write_candidate(a.output_header, palettes, books, maps, decoded_crc32)
    if a.preview:
        a.preview.parent.mkdir(parents=True, exist_ok=True)
        montage.resize((768, 78 * 16 * 3), Image.Resampling.NEAREST).save(a.preview)
    print(f"CANDIDATE: RGB565 16 colours, {reconstructed} original-frame slots; "
          f"media {MEDIA_BYTES}/{APP_SLOT} bytes; worst additional PSNR "
          f"{min(worst):.2f} dB; decoded CRC32={decoded_crc32:08X}; "
          "firmware Flash fit and physical LCD/audio are NOT yet validated")
    if min(worst) < a.min_psnr_db:
        raise SystemExit(f"FAIL: VQ quality below {a.min_psnr_db:.1f}dB threshold: "
                         f"minimum {min(worst):.2f} dB across all original 576 frames; "
                         "do not make firmware until compression quality improves")


if __name__ == "__main__":
    main()
