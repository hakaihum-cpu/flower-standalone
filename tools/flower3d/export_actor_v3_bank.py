#!/usr/bin/env python3
"""Export a reviewed FLOWER 3D pose folder into the Actor-v3 2D asset contract.

This tool does not approve artwork. It only packages already-reviewed RGBA PNGs.
It never creates a legacy/high-resolution mixed bank.

Expected source names are the current student_01 3D authoring names:
  neutral_front.png
  walk_01.png .. walk_08.png
  pose_crouch.png
  pose_sit.png
  pose_knees_up.png
  pose_lie_side.png
  turn_diag_front.png
  turn_side.png
  turn_back.png
  walk_start_01.png / walk_start_02.png
  walk_stop_01.png / walk_stop_02.png
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


POSE_EXPORTS = {
    "stand.png": "neutral_front.png",
    "crouch.png": "pose_crouch.png",
    "sit_floor.png": "pose_sit.png",
    "sit_knees_up.png": "pose_knees_up.png",
    "fall_back.png": "pose_lie_side.png",
    "turn_right.png": "turn_diag_front.png",
    "turn_in_place.png": "turn_side.png",
    "look_back.png": "turn_back.png",
    "step_in.png": "walk_start_01.png",
    "step_out.png": "walk_stop_02.png",
}


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_rgba(path):
    image = Image.open(path).convert("RGBA")
    pixels = np.asarray(image)

    # Authoring/export rule: RGB under fully transparent pixels must be zero.
    transparent = pixels[..., 3] == 0
    if np.any(transparent):
        maximum = int(pixels[..., :3][transparent].max())
        if maximum != 0:
            raise RuntimeError(
                f"{path.name}: transparent RGB max is {maximum}, expected 0"
            )

    return image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source")
    parser.add_argument("output")
    parser.add_argument("--student", default="student_01")
    args = parser.parse_args()

    source = Path(args.source)
    output = Path(args.output)
    student_dir = output / "students" / args.student
    actor_dir = output / "actor_v3" / args.student
    student_dir.mkdir(parents=True, exist_ok=True)
    actor_dir.mkdir(parents=True, exist_ok=True)

    missing = []

    for destination_name, source_name in POSE_EXPORTS.items():
        src = source / source_name
        if not src.exists():
            missing.append(source_name)
            continue

        image = load_rgba(src)
        image.save(student_dir / destination_name)

    walk_frames = []
    for index in range(1, 9):
        src = source / f"walk_{index:02d}.png"
        if not src.exists():
            missing.append(src.name)
            continue
        walk_frames.append(load_rgba(src))

    if missing:
        raise SystemExit("Missing authoring sources: " + ", ".join(missing))

    sizes = {image.size for image in walk_frames}
    if len(sizes) != 1:
        raise SystemExit(f"Walk frame sizes differ: {sorted(sizes)}")

    frame_width, frame_height = walk_frames[0].size
    strip = Image.new(
        "RGBA",
        (frame_width * len(walk_frames), frame_height),
        (0, 0, 0, 0),
    )

    for index, image in enumerate(walk_frames):
        strip.alpha_composite(image, (index * frame_width, 0))

    # Right-facing/source bank only. The approved runtime derives the opposite
    # direction using an exact geometric mirror.
    strip_path = actor_dir / "walk_right_8.png"
    strip.save(strip_path)

    # Use the reviewed stand image as the optional Actor-v3 idle source.
    stand = load_rgba(source / "neutral_front.png")
    stand_path = actor_dir / "stand.png"
    stand.save(stand_path)

    generated = sorted(
        list(student_dir.glob("*.png"))
        + list(actor_dir.glob("*.png"))
    )

    manifest = {
        "student": args.student,
        "source_directory": str(source),
        "policy": {
            "high_res_only": True,
            "left_walk_is_runtime_exact_mirror": True,
            "legacy_mixing_allowed": False,
            "artistic_approval_implied": False,
        },
        "pose_exports": POSE_EXPORTS,
        "walk_strip": {
            "frame_count": 8,
            "frame_width": frame_width,
            "frame_height": frame_height,
            "file": str(strip_path.relative_to(output)),
        },
        "files": {
            str(path.relative_to(output)): sha256(path)
            for path in generated
        },
    }

    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    print(f"Exported {len(generated)} files to {output}")
    print("Artwork status remains REVIEW REQUIRED.")


if __name__ == "__main__":
    main()
