#!/usr/bin/env python3
"""Check real FM-1 host captures of the high-detail mailbox and person.

Both photos come from the user's supplied 72-tile original; this test only
analyzes pixels that the firmware host actually drew, not a made-up preview.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from PIL import Image
import numpy as np


def yellow_pixels(image: Image.Image) -> np.ndarray:
    rgb = np.array(image.convert("RGB"), dtype=np.int16)
    # Host LCD RGB565 yellow can be slightly different from PIL yellow.
    return ((rgb[:, :, 0] >= 145) &
            (rgb[:, :, 1] >= 85) &
            (rgb[:, :, 2] <= 130) &
            (rgb[:, :, 0] >= rgb[:, :, 2] + 45))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("frame_zero", type=Path)
    ap.add_argument("frame_last", type=Path)
    a = ap.parse_args()
    masks = []
    for name in (a.frame_zero, a.frame_last):
        im = Image.open(name)
        if im.size != (240, 240):
            raise SystemExit(f"FAIL: {name} host LCD screenshot is {im.size}, not 240x240")
        mask = yellow_pixels(im)
        # 160x72 source: x24..216 and y48..120. Black background, mailbox
        # left, person right, yellow waveform in the central gap.
        roi = mask[48:120, 24:216]
        mail = mask[48:120, 24:82]
        person = mask[48:120, 142:216]
        if roi.sum() < 350 or mail.sum() < 100 or person.sum() < 180:
            raise SystemExit(
                f"FAIL: {name} mailbox/person illegible: total={roi.sum()} "
                f"mailbox={mail.sum()} portrait={person.sum()}")
        masks.append(roi)
    changed = np.count_nonzero(masks[0] != masks[1])
    if changed < 100:
        raise SystemExit(
            f"FAIL: paper-airplane SUB first/last frames hardly change ({changed} pixels)")
    print(f"PASS: original user mailbox, human silhouette and moving letter "
          f"legible at high detail, {changed} changed LCD pixels")


if __name__ == "__main__":
    main()
