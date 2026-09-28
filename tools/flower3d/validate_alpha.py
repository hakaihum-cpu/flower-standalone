#!/usr/bin/env python3
"""Validate transparent PNGs for white-matte contamination."""

import argparse
from pathlib import Path

import numpy as np
from PIL import Image


def inspect(path):
    rgba = np.asarray(Image.open(path).convert("RGBA"))
    alpha = rgba[..., 3]
    rgb = rgba[..., :3]

    fully_transparent = alpha == 0
    transparent_rgb_max = (
        int(rgb[fully_transparent].max())
        if np.any(fully_transparent)
        else 0
    )

    edge = (alpha > 0) & (alpha < 255)
    edge_count = int(np.count_nonzero(edge))
    edge_luma_mean = float(rgb[edge].mean()) if edge_count else 0.0

    return transparent_rgb_max, edge_count, edge_luma_mean


def main():
    p = argparse.ArgumentParser()
    p.add_argument("directory")
    args = p.parse_args()

    files = sorted(Path(args.directory).glob("*.png"))
    if not files:
        raise SystemExit("No PNG files found")

    failed = False
    for path in files:
        max_rgb, edge_count, edge_mean = inspect(path)
        print(
            f"{path.name}: transparent_rgb_max={max_rgb} "
            f"partial_alpha_pixels={edge_count} "
            f"partial_alpha_rgb_mean={edge_mean:.2f}"
        )
        if max_rgb != 0:
            failed = True

    if failed:
        raise SystemExit(
            "FAIL: one or more PNGs contain RGB data in fully transparent "
            "pixels; white-matte contamination is possible"
        )

    print("PASS: fully transparent pixels are matte-free")


if __name__ == "__main__":
    main()
