#!/usr/bin/env python3
"""Offline source-art M4H9 proof. NOT part of approved FM-1 firmware build.

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
from PIL import Image
from sklearn.cluster import MiniBatchKMeans
from photo96_sources import get, files

N, CLIPS, CLIP, SIDE, CODES, TILES = 628, 24, 27, 96, 96, 576
PAL, LENGTHS, BOOK, INDEX, BITS = 32, 800, 3104, 21536, 24052
CAPACITY = 69 * 4096 - 32
# This SHA is a reproducibility diagnostic, not a universal guarantee
# across different versions of NumPy/Pillow/scikit-learn.
REFERENCE_SHA256 = "69eb8af2574f0a14cbd4e1bc000575cae35bc8f5a94a33d18f2e7f48cb29309f"


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


def build(source_dir: Path, output: Path, report_file: Path | None):
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
        selectors = np.concatenate(selected).reshape(n, TILES)
        image_index = from_tiles(codebook[selectors.reshape(-1)], n)
        reconstructed = colors[image_index].astype(np.float32)
        source = np.asarray(pictures, np.float32)
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
    report = dict(size=len(candidate), resource_max=CAPACITY,
                  room=CAPACITY - len(candidate), frames=N,
                  mean_psnr=round(float(np.mean(psnrs)), 3),
                  candidate_sha256=hashlib.sha256(candidate).hexdigest(),
                  decoded_sha256=reconstructed_hash.hexdigest(),
                  current_firmware_unchanged=True,
                  disclaimer="This 16-colour VQ data is NOT lossless original RGB",
                  clips=records)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(candidate)
    if report_file:
        report_file.write_text(json.dumps(report, ensure_ascii=False, indent=2))
    print(json.dumps({k: v for k, v in report.items() if k != "clips"},
                     ensure_ascii=False, indent=2))
    if report["candidate_sha256"] != REFERENCE_SHA256:
        print("NOTE: candidate differs from reference due to library/compiler "
              "versions or encoder changes; verify all frames independently.")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--source-dir", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    p.add_argument("--report", type=Path)
    args = p.parse_args()
    if Path(os.environ.get("FM1_USER_SHEETS", "")) != args.source_dir:
        raise SystemExit("Set FM1_USER_SHEETS to the same --source-dir BEFORE running")
    build(args.source_dir, args.out, args.report)
