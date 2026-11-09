#!/usr/bin/env python3
"""Edge-preserving, original-source 16-colour 4x4 VQ trial. OFFLINE ONLY.

Input: eight exact user-provided sheets from the existing photo96_sources
manifest. Output: experimental M4H9 resource *payload only*, NOT an installer.
Uses 96x96, 16-colour RGB565 per 27 frames, 96 exact 4x4 codewords, and
per-clip canonical Huffman selectors. VQ is intrinsically lossy.
"""
from __future__ import annotations
import argparse
import hashlib
import heapq
import json
import os
import struct
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from sklearn.cluster import MiniBatchKMeans
from pack_miyako_resource import read_payload, CHUNK_DIR
from photo96_sources import get, files

N, CLIPS, CLIP, SIDE, CODES, TILES = 628, 24, 27, 96, 96, 576
PAL, LENGTHS, BOOK, INDEX, BITS = 32, 800, 3104, 21536, 24052
CAPACITY = 69 * 4096 - 32
# This SHA is a reproducibility diagnostic, not a universal guarantee
# across different versions of NumPy/Pillow/scikit-learn.
# The source M4H9 codec is reused unchanged. The codebooks and selectors,
# not the visual content or FM-1 runtime, are optimized here.
REFERENCE_SHA256 = None

def rgb565(data):
    value = np.asarray(data, dtype=np.uint16)
    return np.stack(((value >> 11) * 255 // 31,
                     ((value >> 5) & 63) * 255 // 63,
                     (value & 31) * 255 // 31), axis=-1).astype(np.uint8)

def decode_existing_vq(archive: bytes, f: int) -> np.ndarray:
    """Exact RGB palette+8x4 dictionary decoding of the CURRENT 628-frame VQ."""
    c = f // CLIP
    palette = rgb565(np.frombuffer(archive, dtype="<u2", count=16, offset=c * 32))
    raw = np.frombuffer(archive, dtype=np.uint8, count=4096, offset=768 + c * 4096).reshape(256, 16)
    codebook = np.empty((256, 32), dtype=np.uint8)
    codebook[:, ::2], codebook[:, 1::2] = raw >> 4, raw & 15
    ids = np.frombuffer(archive, dtype=np.uint8, count=288, offset=99072 + f * 288).reshape(24, 12)
    indexed = codebook[ids].reshape(24, 12, 4, 8).transpose(0, 2, 1, 3).reshape(96, 96)
    return palette[indexed]

def important_edge_weights(pictures: list[np.ndarray]) -> np.ndarray:
    """Emphasize strong visible boundaries, not high-frequency photo noise."""
    frames = []
    for rgb in pictures:
        # A subpixel lowpass before derivative avoids giving noise an advantage.
        im = Image.fromarray(rgb, "RGB").convert("L").filter(ImageFilter.GaussianBlur(radius=0.75))
        lum = np.asarray(im, dtype=np.float32)
        dx = np.zeros_like(lum)
        dy = np.zeros_like(lum)
        dx[:, 1:] = np.abs(lum[:, 1:] - lum[:, :-1])
        dy[1:, :] = np.abs(lum[1:, :] - lum[:-1, :])
        edge = np.maximum(dx, dy)
        frames.append(1.0 + 1.5 * np.clip((edge - 12.0) / 75.0, 0.0, 1.0))
    return np.stack(frames)

def source_tiles(pictures: list[np.ndarray]) -> np.ndarray:
    rgb = np.asarray(pictures, dtype=np.float32)
    return rgb.reshape(-1, 24, 4, 24, 4, 3).transpose(0, 1, 3, 2, 4, 5).reshape(-1, 16, 3)

def choose_edge_aware(targets: np.ndarray, weights: np.ndarray,
                      colors: np.ndarray, codebook: np.ndarray) -> np.ndarray:
    prototype = colors[codebook].reshape(CODES, 16, 3).astype(np.float32)
    out = np.empty(len(targets), dtype=np.uint8)
    # Keep peak memory bounded for scripts and CI host checks.
    for first in range(0, len(targets), 256):
        end = min(first + 256, len(targets))
        delta = targets[first:end, None, :, :] - prototype[None, :, :, :]
        objective = (delta * delta * weights[first:end, None, :, None]).sum(axis=(2, 3))
        out[first:end] = np.argmin(objective, axis=1)
    return out

def refine_edges(pictures: list[np.ndarray], weight_image: np.ndarray,
                 colors: np.ndarray, initial_codebook: np.ndarray) -> tuple[np.ndarray,np.ndarray]:
    """VQ pixel-art fit: preserve boundaries using 4x4 source-color patterns.

    Each update chooses among the SAME 16 RGB565 palette colors.
    No invented features, re-drawing, hallucination, or frame/geometry changes.
    """
    targets = source_tiles(pictures)
    weights = as_tiles(weight_image).reshape(-1, 16).astype(np.float32)
    palette = np.asarray(colors, dtype=np.float32)
    book = initial_codebook.copy()
    selector = choose_edge_aware(targets, weights, palette, book)
    for _ in range(3):
        new = book.copy()
        for symbol in range(CODES):
            subset = np.flatnonzero(selector == symbol)
            if len(subset) == 0:
                continue
            w = weights[subset]
            mean = (targets[subset] * w[:, :, None]).sum(axis=0) / w.sum(axis=0)[:, None]
            d = ((mean[:, None, :] - palette[None, :, :]) ** 2).sum(axis=-1)
            new[symbol] = np.argmin(d, axis=1)
        next_select = choose_edge_aware(targets, weights, palette, new)
        if np.array_equal(new, book) and np.array_equal(selector, next_select):
            break
        book, selector = new, next_select
    return book, selector

def frame_errors(source: np.ndarray, decoded: np.ndarray,
                 weights: np.ndarray) -> tuple[np.ndarray,np.ndarray]:
    diff = (source.astype(np.float32) - decoded.astype(np.float32)) ** 2
    raw = diff.mean(axis=(1, 2, 3))
    weighted = (diff * weights[..., None]).mean(axis=(1, 2, 3))
    return raw, weighted



def quantize(images):
    composite = Image.new("RGB", (SIDE * len(images), SIDE))
    for i, frame in enumerate(images):
        composite.paste(Image.fromarray(frame), (SIDE * i, 0))
    q = composite.quantize(colors=16, method=Image.Quantize.MEDIANCUT,
                           dither=Image.Dither.NONE)
    rgb = np.asarray(q.getpalette(), dtype=np.uint8).reshape(-1, 3)[:16].copy()
    rgb565 = ((rgb[:, 0].astype(np.uint16) >> 3) << 11) | (
        (rgb[:, 1].astype(np.uint16) >> 2) << 5) | (
        rgb[:, 2].astype(np.uint16) >> 3)
    pal = np.stack(((rgb565 >> 11) * 255 // 31,
                    ((rgb565 >> 5) & 63) * 255 // 63,
                    (rgb565 & 31) * 255 // 31), axis=1).astype(np.uint8)
    pi = Image.new("P", (1, 1))
    pi.putpalette(pal.reshape(-1).tolist())
    idx = np.stack([np.asarray(Image.fromarray(frame).quantize(
        palette=pi, dither=Image.Dither.NONE), dtype=np.uint8)
        for frame in images])
    if idx.max() > 15:
        raise ValueError("bad quantized colour index")
    return rgb565, pal, idx


def as_tiles(index):
    return index.reshape(-1, 24, 4, 24, 4).transpose(
        0, 1, 3, 2, 4).reshape(-1, 16)


def from_tiles(index, n):
    return index.reshape(n, 24, 24, 4, 4).transpose(
        0, 1, 3, 2, 4).reshape(n, 96, 96)


def huffman_lengths(counts):
    q = [(int(weight), i, ("s", i))
         for i, weight in enumerate(counts) if weight]
    heapq.heapify(q)
    if len(q) <= 1:
        return [1 if counts[i] else 0 for i in range(CODES)]
    serial = CODES
    while len(q) > 1:
        a, b = heapq.heappop(q), heapq.heappop(q)
        heapq.heappush(q, (a[0] + b[0], serial, ("n", a[2], b[2])))
        serial += 1
    lengths = [0] * CODES

    def visit(node, depth):
        if node[0] == "s":
            lengths[node[1]] = max(1, depth)
        else:
            visit(node[1], depth + 1)
            visit(node[2], depth + 1)
    visit(q[0][2], 0)
    if max(lengths) > 15:
        raise ValueError("Huffman tree exceeds firmware maximum 15 bits")
    return lengths


def stream_codes(maps, lengths):
    code, previous, dictionary = 0, 0, {}
    for n, symbol in sorted((n, sym) for sym, n in enumerate(lengths) if n):
        code <<= n - previous
        dictionary[symbol] = code, n
        code += 1
        previous = n
    content, offsets = bytearray(), []
    for frame in maps:
        offsets.append(len(content))
        value = used = 0
        for sym in frame:
            bits, width = dictionary[int(sym)]
            for k in range(width - 1, -1, -1):
                value = (value << 1) | ((bits >> k) & 1)
                used += 1
                if used == 8:
                    content.append(value)
                    value = used = 0
        if used:
            content.append(value << (8 - used))
    offsets.append(len(content))
    return bytes(content), offsets


def build(source_dir: Path, output: Path, report_file: Path | None, preview_path: Path | None):
    root = Path(__file__).resolve().parents[3]
    manifest = json.loads((root / "Resources/fm1_photo_96_chunks/manifest.json").read_text())
    # photo96_sources uses this environment variable at import time; set it
    # before executing this script from the shell.
    originals = []
    for bank, (name, _, _, count) in enumerate(files):
        digest = hashlib.sha256((source_dir / name).read_bytes()).hexdigest()
        if digest != manifest["files"][bank]["sha256"]:
            raise ValueError("source sheet SHA256 mismatch: " + name)
        originals.extend(np.asarray(get(bank, i).convert("RGB"), dtype=np.uint8)
                         for i in range(count))
    if len(originals) != N:
        raise ValueError("missing user frames")

    # A current-branch-approved VQ payload is required for meaningful
    # apples-to-apples comparisons. Old or mismatched resource files fail SHA.
    old_archive = read_payload(CHUNK_DIR)
    preview_candidates = {}
    records_detail = []
    all_candidates_better = True
    palettes, lengths, dictionaries, encoded, starts = (
        bytearray(), bytearray(), bytearray(), bytearray(), [])
    psnrs, records, reconstructed_hash = [], [], hashlib.sha256()
    for clip in range(CLIPS):
        pictures = originals[clip * CLIP:min(N, (clip + 1) * CLIP)]
        n = len(pictures)
        pal565, colors, indices = quantize(pictures)
        features = colors[as_tiles(indices)].reshape(-1, 48).astype(np.float32)
        model = MiniBatchKMeans(n_clusters=CODES, random_state=1007 + clip,
                                n_init=1, max_iter=50, batch_size=2048,
                                reassignment_ratio=0.003)
        model.fit(features)
        centers = model.cluster_centers_.reshape(CODES, 16, 3)
        diff = centers[:, :, None, :] - colors[None, None, :, :].astype(np.float32)
        codebook = np.argmin(np.einsum("ijkl,ijkl->ijk", diff, diff),
                             axis=2).astype(np.uint8)
        prototypes = colors[codebook].reshape(CODES, 48).astype(np.float32)
        norms = (prototypes * prototypes).sum(axis=1)
        selected = []
        for chunk in np.array_split(features, 8):
            selected.append(np.argmin(
                norms[None, :] - 2 * chunk @ prototypes.T, axis=1
            ).astype(np.uint8))
        initial_selectors = np.concatenate(selected).reshape(n, TILES)
        weights_image = important_edge_weights(pictures)
        old_baseline = np.stack([decode_existing_vq(old_archive, f)
                                 for f in range(clip * CLIP, clip * CLIP + n)])
        # Compare both the unmodified M4H9 test algorithm and the new
        # edge-aware update on the SAME original source pixels.
        initial_index = from_tiles(codebook[initial_selectors.reshape(-1)], n)
        initial_recon = colors[initial_index]
        refined_book, refined_sel = refine_edges(pictures, weights_image, colors, codebook)
        refined_idx = from_tiles(refined_book[refined_sel].reshape(-1, 16), n)
        refined_recon = colors[refined_idx]
        source = np.asarray(pictures, np.float32)
        base_raw, base_edge = frame_errors(source, old_baseline, weights_image)
        initial_raw, initial_edge = frame_errors(source, initial_recon, weights_image)
        trial_raw, trial_edge = frame_errors(source, refined_recon, weights_image)
        # Within the source-derived 4x4 candidates, keep whichever does not
        # worsen the WORST edge frame or mean edge error.
        if trial_edge.max() <= initial_edge.max() and trial_edge.mean() <= initial_edge.mean():
            codebook, selectors, image_index = refined_book, refined_sel.reshape(n, TILES), refined_idx
            reconstructed = refined_recon.astype(np.float32)
            use_refined = True
        else:
            selectors, image_index = initial_selectors, initial_index
            reconstructed = initial_recon.astype(np.float32)
            use_refined = False
        final_raw, final_edge = frame_errors(source, reconstructed, weights_image)
        improved = bool(final_edge.max() < base_edge.max() and
                        final_edge.mean() < base_edge.mean() and
                        final_raw.mean() <= base_raw.mean() and
                        final_raw.max() <= base_raw.max())
        all_candidates_better &= improved
        records_detail.append({
            "clip": clip, "frames": n, "qualified_vs_current_vq": improved,
            "edge_refinement_selected": use_refined,
            "current_vq_psnr": round(float(10*np.log10(65025/max(base_raw.mean(),1e-8))), 3),
            "trial_psnr": round(float(10*np.log10(65025/max(final_raw.mean(),1e-8))), 3),
            "current_vq_worst_edge_mse": round(float(base_edge.max()), 4),
            "trial_worst_edge_mse": round(float(final_edge.max()), 4),
            "current_vq_mean_edge_mse": round(float(base_edge.mean()), 4),
            "trial_mean_edge_mse": round(float(final_edge.mean()), 4),
        })
        for offset in range(n):
            k = clip * CLIP + offset
            if k in (0, 145, 326, 558):
                preview_candidates[k] = (originals[k], old_baseline[offset], reconstructed[offset].astype(np.uint8))
        errors = ((reconstructed - source) ** 2).mean(axis=(1, 2, 3))
        psnr = 10 * np.log10(65025 / np.maximum(errors, 1e-6))
        psnrs.extend(psnr.tolist())

        sizes = np.bincount(selectors.reshape(-1), minlength=CODES)
        table = huffman_lengths(sizes)
        part, local_offsets = stream_codes(selectors, table)
        starts.extend(BITS + len(encoded) + x for x in local_offsets[:-1])
        palettes.extend(pal565.astype("<u2").tobytes())
        lengths.extend(bytes(table))
        dictionaries.extend(((codebook[:, 0::2] << 4) |
                             codebook[:, 1::2]).tobytes())
        encoded.extend(part)
        for image in image_index:
            reconstructed_hash.update(pal565.astype("<u2").tobytes())
            reconstructed_hash.update(((image[:, 0::2] << 4) |
                                       image[:, 1::2]).tobytes())
        records.append(dict(clip=clip, frames=n, payload_bytes=len(part),
                            psnr=round(float(psnr.mean()), 3)))
    starts.append(BITS + len(encoded))
    header = struct.pack("<4sHHHHHHIIII", b"M4H9", 1, SIDE, N,
                         CLIPS, CLIP, CODES, PAL, LENGTHS, BOOK, INDEX)
    candidate = (header + palettes + lengths + dictionaries +
                 b"".join(struct.pack("<I", x) for x in starts) + encoded)
    if len(candidate) != BITS + len(encoded) or len(candidate) > CAPACITY:
        raise ValueError(f"candidate does not fit res.bin: {len(candidate)} bytes")
    # This trial creates a diagnostic (NOT an accepted/approved res.bin)
    # unless it beats the current source-matched VQ on EVERY clip.
    report = dict(size=len(candidate), resource_max=CAPACITY,
                  room=CAPACITY - len(candidate), frames=N,
                  source_sha256=hashlib.sha256(old_archive).hexdigest(),
                  every_clip_quality_gate_passed=all_candidates_better,
                  installation_approved=False,
                  edge_aware_detail=records_detail,
                  quality_gate="per clip: worst/mean edge MSE strictly improves; mean/worst RGB MSE do not worsen",
                  mean_psnr=round(float(np.mean(psnrs)), 3),
                  candidate_sha256=hashlib.sha256(candidate).hexdigest(),
                  decoded_sha256=reconstructed_hash.hexdigest(),
                  current_firmware_unchanged=True,
                  disclaimer="This 16-colour VQ data is NOT lossless original RGB",
                  clips=records)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(candidate)
    if preview_path:
        canvas = Image.new("RGB", (4 * 200, 4 * 222), (24, 24, 27))
        draw = ImageDraw.Draw(canvas)
        draw.text((8, 5), "SOURCE (96x96)", fill="white")
        draw.text((208, 5), "CURRENT VQ", fill="white")
        draw.text((408, 5), "EDGE-AWARE TRIAL", fill="white")
        for row, frame in enumerate((0, 145, 326, 558)):
            if frame not in preview_candidates:
                raise AssertionError("missing preview frame")
            source_img, old_img, new_img = preview_candidates[frame]
            for col, img in enumerate((source_img, old_img, new_img)):
                panel = Image.fromarray(np.asarray(img, dtype=np.uint8)).resize(
                    (192, 192), Image.Resampling.NEAREST)
                canvas.paste(panel, (col * 200 + 4, row * 222 + 26))
            draw.text((608, row * 222 + 31),
                      "FRAME " + str(frame) + (" PASS" if
                      records_detail[frame//CLIP]["qualified_vs_current_vq"] else " UNPROVEN"),
                      fill=(250, 245, 235))
        preview_path.parent.mkdir(parents=True, exist_ok=True)
        canvas.save(preview_path)
    if report_file:
        report_file.write_text(json.dumps(report, ensure_ascii=False, indent=2))
    print(json.dumps({k: v for k, v in report.items() if k not in ("clips", "edge_aware_detail")},
                     ensure_ascii=False, indent=2))
    if report["candidate_sha256"] != REFERENCE_SHA256:
        print("NOTE: candidate differs from reference due to library/compiler "
              "versions or encoder changes; verify all frames independently.")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--source-dir", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    p.add_argument("--report", type=Path)
    p.add_argument("--preview", type=Path)
    args = p.parse_args()
    if Path(os.environ.get("FM1_USER_SHEETS", "")) != args.source_dir:
        raise SystemExit("Set FM1_USER_SHEETS to the same --source-dir BEFORE running")
    if args.out.suffix != ".m4h9":
        raise SystemExit("OFFLINE ONLY: output must end in .m4h9, never .bin/.fwsc")
    build(args.source_dir, args.out, args.report, args.preview)
