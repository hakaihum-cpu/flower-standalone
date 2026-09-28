#!/usr/bin/env python3
"""Generate a real-geometry student_01 technical proxy.

This is intentionally NOT the final Flower character art.  It exists to prove
that the 3D branch contains actual geometry and reproducible walk poses rather
than AI-generated "3D-looking" stills.

Outputs:
  student01_neutral_technical_proxy.glb
  student01_walk_8pose_technical_proxy.glb
  README.txt
"""

import argparse
import math
from pathlib import Path

import numpy as np
import trimesh


COLORS = {
    "skin": [190, 190, 190, 255],
    "shirt": [225, 225, 225, 255],
    "dark": [35, 35, 35, 255],
    "hair": [15, 15, 15, 255],
    "bag": [28, 28, 28, 255],
}


def rgba(mesh, color):
    mesh.visual.face_colors = np.tile(
        np.asarray(color, dtype=np.uint8),
        (len(mesh.faces), 1),
    )
    return mesh


def between(a, b, radius=0.045, color=COLORS["skin"]):
    a = np.asarray(a, dtype=float)
    b = np.asarray(b, dtype=float)
    return rgba(
        trimesh.creation.cylinder(
            radius=radius,
            segment=np.vstack([a, b]),
            sections=16,
        ),
        color,
    )


def sphere(center, radius, color):
    mesh = trimesh.creation.icosphere(subdivisions=2, radius=radius)
    mesh.apply_translation(center)
    return rgba(mesh, color)


def box(extents, center, color):
    mesh = trimesh.creation.box(extents=extents)
    mesh.apply_translation(center)
    return rgba(mesh, color)


def character_pose(phase=0.0, offset=(0.0, 0.0, 0.0)):
    """Return one real 3D mesh for a restrained side-walk pose.

    Proportions are only a technical approximation of Reference A/B.  This
    geometry must not be promoted to production without visual review.
    """
    ox, oy, oz = offset
    z_floor = oz

    hip = np.array([ox, oy, z_floor + 0.90])
    shoulder = np.array([ox, oy, z_floor + 1.33])
    neck = np.array([ox, oy, z_floor + 1.48])
    head_center = np.array([ox, oy, z_floor + 1.62])

    # Restrained gait: no artificial whole-body vertical bob.
    swing = 0.22 * math.sin(phase)
    knee_l = np.array([ox + 0.12 * swing, oy, z_floor + 0.52])
    knee_r = np.array([ox - 0.12 * swing, oy, z_floor + 0.52])
    ankle_l = np.array([ox + 0.28 * swing, oy, z_floor + 0.08])
    ankle_r = np.array([ox - 0.28 * swing, oy, z_floor + 0.08])
    hip_l = hip + np.array([0.0, 0.085, 0.0])
    hip_r = hip + np.array([0.0, -0.085, 0.0])

    arm = 0.18 * math.sin(phase + math.pi)
    shoulder_l = shoulder + np.array([0.0, 0.20, 0.0])
    shoulder_r = shoulder + np.array([0.0, -0.20, 0.0])
    elbow_l = shoulder + np.array([0.10 * arm, 0.24, -0.26])
    elbow_r = shoulder + np.array([-0.10 * arm, -0.24, -0.26])
    wrist_l = shoulder + np.array([0.22 * arm, 0.24, -0.50])
    wrist_r = shoulder + np.array([-0.22 * arm, -0.24, -0.50])

    parts = []

    # Uniform/body proxy.
    parts.append(box([0.26, 0.38, 0.42],
                     shoulder + np.array([0.0, 0.0, -0.18]),
                     COLORS["shirt"]))
    parts.append(box([0.28, 0.44, 0.24],
                     hip + np.array([0.0, 0.0, 0.03]),
                     COLORS["dark"]))
    parts.append(box([0.34, 0.52, 0.14],
                     hip + np.array([0.0, 0.0, -0.08]),
                     COLORS["dark"]))

    # Head/hair.
    parts.append(between(
        neck - np.array([0.0, 0.0, 0.06]),
        neck + np.array([0.0, 0.0, 0.02]),
        0.045,
        COLORS["skin"],
    ))
    parts.append(sphere(head_center, 0.115, COLORS["skin"]))
    parts.append(sphere(
        head_center + np.array([0.0, 0.015, 0.015]),
        0.125,
        COLORS["hair"],
    ))
    parts.append(box(
        [0.14, 0.22, 0.30],
        head_center + np.array([0.0, -0.005, -0.10]),
        COLORS["hair"],
    ))
    parts.append(box(
        [0.015, 0.14, 0.13],
        head_center + np.array([0.115, 0.0, -0.015]),
        COLORS["skin"],
    ))

    # Legs, socks, shoes.
    for a, b in (
        (hip_l, knee_l),
        (knee_l, ankle_l),
        (hip_r, knee_r),
        (knee_r, ankle_r),
    ):
        parts.append(between(a, b, 0.055, COLORS["skin"]))

    for ankle in (ankle_l, ankle_r):
        sock_top = ankle + np.array([0.0, 0.0, 0.17])
        parts.append(between(ankle, sock_top, 0.058, COLORS["dark"]))
        parts.append(box(
            [0.18, 0.12, 0.08],
            ankle + np.array([0.045, 0.0, -0.02]),
            COLORS["dark"],
        ))

    # Arms.
    for a, b in (
        (shoulder_l, elbow_l),
        (elbow_l, wrist_l),
        (shoulder_r, elbow_r),
        (elbow_r, wrist_r),
    ):
        parts.append(between(a, b, 0.038, COLORS["skin"]))

    # Bag: stable student_01 identity marker.
    bag_center = hip + np.array([-0.05, -0.30, 0.08])
    parts.append(box([0.18, 0.09, 0.27], bag_center, COLORS["bag"]))
    parts.append(between(
        shoulder_r,
        bag_center + np.array([0.0, 0.0, 0.11]),
        0.018,
        COLORS["bag"],
    ))

    return trimesh.util.concatenate(parts)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="flower3d_proxy")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    scene = trimesh.Scene()
    for i in range(8):
        phase = 2.0 * math.pi * i / 8.0
        mesh = character_pose(phase, offset=(i * 0.65, 0.0, 0.0))
        name = f"walk_{i + 1:02d}"
        scene.add_geometry(mesh, node_name=name, geom_name=name)

    scene.export(out_dir / "student01_walk_8pose_technical_proxy.glb")

    neutral = trimesh.Scene(character_pose(0.0))
    neutral.export(out_dir / "student01_neutral_technical_proxy.glb")

    (out_dir / "README.txt").write_text(
        "FLOWER student_01 technical proxy\n"
        "Actual 3D geometry generated with trimesh.\n"
        "Purpose: validate branch, geometry workflow, proportions, motion "
        "staging and GLB export.\n"
        "NOT a final visual production asset. Final approval requires a "
        "Blender-rendered / artist-refined student_01 matching Reference A/B.\n"
        "The eight walk poses use restrained body motion and alternating limbs.\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
