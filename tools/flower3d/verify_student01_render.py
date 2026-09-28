#!/usr/bin/env python3
"""Verify the student_01 Blender render package before runtime integration.

This script does not decide artistic approval. It checks mechanical constraints
that should not require subjective review: frame count/names, dimensions,
alpha, actor bounds, baseline stability and excessive vertical bob.
"""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


FRAME_NAMES = [f"student01_walk_{i:02d}.png" for i in range(1, 9)]


def opaque_bounds(rgba):
    alpha = rgba[..., 3]
    ys, xs = np.nonzero(alpha > 8)
    if len(xs) == 0:
        return None
    return [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())]


def inspect_frame(path, expected_size):
    image = Image.open(path).convert("RGBA")
    rgba = np.asarray(image)
    width, height = image.size

    alpha = rgba[..., 3]
    transparent = alpha == 0
    transparent_rgb_max = (
        int(rgba[..., :3][transparent].max())
        if np.any(transparent)
        else 0
    )

    partial = (alpha > 0) & (alpha < 255)
    partial_count = int(np.count_nonzero(partial))
    partial_rgb_mean = (
        float(rgba[..., :3][partial].mean())
        if partial_count
        else 0.0
    )

    bounds = opaque_bounds(rgba)

    return {
        "name": path.name,
        "size": [width, height],
        "size_ok": [width, height] == list(expected_size),
        "transparent_rgb_max": transparent_rgb_max,
        "partial_alpha_pixels": partial_count,
        "partial_alpha_rgb_mean": round(partial_rgb_mean, 3),
        "bounds": bounds,
    }


def make_contact_sheet(frames, output):
    cells = []
    for path in frames:
        rgba = Image.open(path).convert("RGBA")
        bg = Image.new("RGBA", rgba.size, (92, 92, 92, 255))
        bg.alpha_composite(rgba)
        cells.append(bg.convert("RGB"))

    width, height = cells[0].size
    sheet = Image.new("RGB", (width * 4, height * 2), (230, 230, 230))
    for i, cell in enumerate(cells):
        sheet.paste(cell, ((i % 4) * width, (i // 4) * height))
    sheet.save(output)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("directory")
    p.add_argument("--spec", required=True)
    p.add_argument("--report", default="student01_render_report.json")
    p.add_argument("--contact-sheet", default="student01_contact_sheet.png")
    args = p.parse_args()

    root = Path(args.directory)
    spec = json.loads(Path(args.spec).read_text(encoding="utf-8"))
    expected_size = (
        int(spec["render"]["width"]),
        int(spec["render"]["height"]),
    )

    missing = [name for name in FRAME_NAMES if not (root / name).exists()]
    if missing:
        raise SystemExit("Missing walk frames: " + ", ".join(missing))

    frame_paths = [root / name for name in FRAME_NAMES]
    frames = [inspect_frame(path, expected_size) for path in frame_paths]

    bounds = [f["bounds"] for f in frames if f["bounds"]]
    tops = [b[1] for b in bounds]
    bottoms = [b[3] for b in bounds]
    heights = [b[3] - b[1] + 1 for b in bounds]

    mean_height = float(np.mean(heights)) if heights else 0.0
    top_range = int(max(tops) - min(tops)) if tops else 0
    bottom_range = int(max(bottoms) - min(bottoms)) if bottoms else 0
    vertical_range_fraction = (
        float(top_range / mean_height)
        if mean_height > 0
        else 1.0
    )

    max_allowed = float(
        spec["render"]["vertical_bob_max_fraction_of_actor_height"]
    )

    report = {
        "asset": spec["asset"],
        "frame_count": len(frames),
        "expected_size": list(expected_size),
        "frames": frames,
        "mechanical_checks": {
            "all_sizes_ok": all(f["size_ok"] for f in frames),
            "all_have_actor_pixels": len(bounds) == 8,
            "top_range_px": top_range,
            "bottom_range_px": bottom_range,
            "mean_actor_height_px": round(mean_height, 3),
            "vertical_range_fraction": round(vertical_range_fraction, 5),
            "vertical_bob_limit_fraction": max_allowed,
            "vertical_bob_ok": vertical_range_fraction <= max_allowed,
        },
        "artistic_approval": "NOT_EVALUATED",
    }

    report_path = root / args.report
    report_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    make_contact_sheet(frame_paths, root / args.contact_sheet)

    checks = report["mechanical_checks"]
    failed = [
        name for name in (
            "all_sizes_ok",
            "all_have_actor_pixels",
            "vertical_bob_ok",
        )
        if not checks[name]
    ]

    print(json.dumps(report["mechanical_checks"], indent=2))
    if failed:
        raise SystemExit("Mechanical verification failed: " + ", ".join(failed))

    print("Mechanical verification passed. Artistic review is still required.")


if __name__ == "__main__":
    main()
