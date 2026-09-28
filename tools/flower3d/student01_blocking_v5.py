#!/usr/bin/env python3
"""FLOWER student_01 procedural 3D blocking v5.

This is real 3D geometry for the AN-33 prototype branch. It is intentionally a
blocking model, not final production art.  The two locked user reference sheets
remain authoritative for appearance.

The script generates:
- neutral GLB,
- 8-pose walk GLB scene,
- basic-pose GLB scene,
- manifest with hashes.

Dependencies: numpy, trimesh, Pillow.
"""
import math
import json
import hashlib
from pathlib import Path

import numpy as np
import trimesh
from PIL import Image, ImageDraw, ImageFont

COLORS = {
    "skin": np.array([176, 176, 176, 255], dtype=np.uint8),
    "shirt": np.array([224, 224, 224, 255], dtype=np.uint8),
    "dark": np.array([30, 30, 30, 255], dtype=np.uint8),
    "hair": np.array([12, 12, 12, 255], dtype=np.uint8),
    "bag": np.array([25, 25, 25, 255], dtype=np.uint8),
    "bow": np.array([35, 35, 35, 255], dtype=np.uint8),
}

P = {
    "height": 1.62,
    "hip_z": 0.88,
    "waist_z": 1.02,
    "shoulder_z": 1.29,
    "neck_z": 1.38,
    "head_center_z": 1.50,
    "head_r": (0.10, 0.085, 0.12),
    "knee_z": 0.47,
    "ankle_z": 0.09,
    "shoulder_half": 0.16,
    "hip_half": 0.06,
    "skirt_hem_z": 0.69,
}


def rgba(mesh, color):
    mesh.visual.face_colors = np.tile(color, (len(mesh.faces), 1))
    return mesh


def capsule_between(a, b, radius, color, sections=16):
    a = np.asarray(a, float)
    b = np.asarray(b, float)
    vec = b - a
    length = float(np.linalg.norm(vec))

    cyl = trimesh.creation.cylinder(
        radius=radius,
        height=length,
        sections=sections,
    )
    direction = vec / max(length, 1.0e-9)
    cyl.apply_transform(
        trimesh.geometry.align_vectors([0.0, 0.0, 1.0], direction)
    )
    cyl.apply_translation((a + b) * 0.5)

    s1 = trimesh.creation.icosphere(subdivisions=2, radius=radius)
    s2 = trimesh.creation.icosphere(subdivisions=2, radius=radius)
    s1.apply_translation(a)
    s2.apply_translation(b)

    return rgba(trimesh.util.concatenate([cyl, s1, s2]), color)


def ellipsoid(center, radii, color, subdiv=2):
    mesh = trimesh.creation.icosphere(subdivisions=subdiv, radius=1.0)
    matrix = np.eye(4)
    matrix[0, 0], matrix[1, 1], matrix[2, 2] = radii
    matrix[:3, 3] = center
    mesh.apply_transform(matrix)
    return rgba(mesh, color)


def box(center, extents, color):
    mesh = trimesh.creation.box(extents=extents)
    mesh.apply_translation(center)
    return rgba(mesh, color)


def frustum(z0, z1, rx0, ry0, rx1, ry1, color, sections=24):
    vertices = []
    for z, rx, ry in ((z0, rx0, ry0), (z1, rx1, ry1)):
        for i in range(sections):
            angle = 2.0 * math.pi * i / sections
            vertices.append([rx * math.cos(angle), ry * math.sin(angle), z])

    faces = []
    for i in range(sections):
        j = (i + 1) % sections
        faces.extend([[i, j, sections + j], [i, sections + j, sections + i]])

    vertices.append([0.0, 0.0, z0])
    c0 = len(vertices) - 1
    vertices.append([0.0, 0.0, z1])
    c1 = len(vertices) - 1

    for i in range(sections):
        j = (i + 1) % sections
        faces.append([c0, j, i])
        faces.append([c1, sections + i, sections + j])

    mesh = trimesh.Trimesh(
        vertices=np.asarray(vertices),
        faces=np.asarray(faces),
        process=False,
    )
    return rgba(mesh, color)


def pose_joints(kind="stand", phase=0.0):
    hip = np.array([0.0, 0.0, P["hip_z"]])
    shoulder = np.array([0.0, 0.0, P["shoulder_z"]])
    joints = {
        "hip": hip,
        "shoulder": shoulder,
        "hipL": hip + [0.0, P["hip_half"], 0.0],
        "hipR": hip + [0.0, -P["hip_half"], 0.0],
        "shL": shoulder + [0.0, P["shoulder_half"], 0.0],
        "shR": shoulder + [0.0, -P["shoulder_half"], 0.0],
    }

    if kind == "walk":
        s = math.sin(phase)
        for side, sign in (("L", 1), ("R", -1)):
            hip_joint = joints["hip" + side]
            swing = 0.50 * s * sign
            joints["knee" + side] = np.array([
                0.18 * swing,
                hip_joint[1],
                P["knee_z"] + 0.035 * max(0.0, -s * sign),
            ])
            joints["ankle" + side] = np.array([
                0.32 * swing,
                hip_joint[1],
                P["ankle_z"] + 0.02 * max(0.0, -s * sign),
            ])

        arm = 0.15 * math.sin(phase + math.pi)
        for side, sign in (("L", 1), ("R", -1)):
            shoulder_joint = joints["sh" + side]
            joints["elbow" + side] = np.array([
                0.09 * arm * sign,
                shoulder_joint[1],
                1.05,
            ])
            joints["wrist" + side] = np.array([
                0.19 * arm * sign,
                shoulder_joint[1],
                0.82,
            ])

    elif kind in ("sit", "crouch"):
        dz = -0.42 if kind == "sit" else -0.30
        for key in ("hip", "shoulder", "hipL", "hipR", "shL", "shR"):
            joints[key] = joints[key] + [0.0, 0.0, dz]

        for side, y in (("L", P["hip_half"]), ("R", -P["hip_half"])):
            if kind == "sit":
                joints["knee" + side] = np.array([0.28, y, 0.47])
                joints["ankle" + side] = np.array([0.12, y, 0.10])
            else:
                joints["knee" + side] = np.array([0.20, y, 0.36])
                joints["ankle" + side] = np.array([0.04, y, 0.09])

        for side, y in (
            ("L", P["shoulder_half"]),
            ("R", -P["shoulder_half"]),
        ):
            joints["elbow" + side] = np.array([0.17, y, 0.81])
            joints["wrist" + side] = np.array([0.24, y, 0.59])

    else:
        for side, y in (("L", P["hip_half"]), ("R", -P["hip_half"])):
            joints["knee" + side] = np.array([0.0, y, P["knee_z"]])
            joints["ankle" + side] = np.array([0.0, y, P["ankle_z"]])

        for side, y, y_sign in (
            ("L", P["shoulder_half"], 1),
            ("R", -P["shoulder_half"], -1),
        ):
            joints["elbow" + side] = np.array([0.035, y, 1.05])
            joints["wrist" + side] = np.array([0.11, 0.035 * y_sign, 0.90])

    return joints


def build_character(kind="stand", phase=0.0):
    joints = pose_joints(kind, phase)
    hip = joints["hip"]
    shoulder = joints["shoulder"]
    parts = []

    parts.append(
        frustum(
            hip[2] + 0.09,
            shoulder[2] + 0.03,
            0.12,
            0.15,
            0.135,
            0.17,
            COLORS["shirt"],
        )
    )
    parts.append(
        frustum(
            P["skirt_hem_z"] + (hip[2] - P["hip_z"]),
            hip[2] + 0.10,
            0.22,
            0.20,
            0.12,
            0.15,
            COLORS["dark"],
        )
    )

    parts.append(
        box(
            [0.12, 0.0, shoulder[2] - 0.04],
            [0.03, 0.12, 0.08],
            COLORS["bow"],
        )
    )

    neck = np.array([0.0, 0.0, shoulder[2] + 0.11])
    head_center = np.array([0.02, 0.0, shoulder[2] + 0.30])

    parts.append(
        capsule_between(
            [0.0, 0.0, shoulder[2] + 0.02],
            neck,
            0.045,
            COLORS["skin"],
        )
    )
    parts.append(ellipsoid(head_center, P["head_r"], COLORS["skin"]))
    parts.append(
        ellipsoid(
            head_center + [-0.015, 0.0, 0.03],
            [0.105, 0.095, 0.13],
            COLORS["hair"],
        )
    )
    parts.append(
        box(
            [-0.055, 0.0, head_center[2] - 0.17],
            [0.11, 0.20, 0.34],
            COLORS["hair"],
        )
    )
    parts.append(
        ellipsoid(
            head_center + [0.075, 0.0, -0.005],
            [0.035, 0.065, 0.08],
            COLORS["skin"],
            subdiv=1,
        )
    )

    for side in ("L", "R"):
        parts.append(
            capsule_between(
                joints["hip" + side],
                joints["knee" + side],
                0.045,
                COLORS["skin"],
            )
        )
        parts.append(
            capsule_between(
                joints["knee" + side],
                joints["ankle" + side],
                0.042,
                COLORS["skin"],
            )
        )
        parts.append(
            capsule_between(
                joints["sh" + side],
                joints["elbow" + side],
                0.034,
                COLORS["skin"],
            )
        )
        parts.append(
            capsule_between(
                joints["elbow" + side],
                joints["wrist" + side],
                0.031,
                COLORS["skin"],
            )
        )

        ankle = joints["ankle" + side]
        parts.append(
            capsule_between(
                ankle,
                ankle + [0.0, 0.0, 0.16],
                0.047,
                COLORS["dark"],
            )
        )
        parts.append(
            box(
                ankle + [0.065, 0.0, -0.025],
                [0.24, 0.09, 0.07],
                COLORS["dark"],
            )
        )

    bag_center = hip + [-0.12, -0.23, 0.03]
    parts.append(
        box(
            bag_center,
            [0.30, 0.065, 0.25],
            COLORS["bag"],
        )
    )
    parts.append(
        capsule_between(
            joints["shR"],
            bag_center + [0.0, 0.0, 0.11],
            0.013,
            COLORS["bag"],
        )
    )

    return trimesh.util.concatenate(parts)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    out = Path("build/flower3d/blocking_v5")
    out.mkdir(parents=True, exist_ok=True)

    neutral = build_character("stand")
    neutral_glb = out / "student01_blocking_v5_neutral.glb"
    trimesh.Scene(neutral).export(neutral_glb)

    walk_scene = trimesh.Scene()
    for index in range(8):
        mesh = build_character("walk", 2.0 * math.pi * index / 8.0)
        mesh.apply_translation([index * 0.65, 0.0, 0.0])
        name = f"walk_{index + 1:02d}"
        walk_scene.add_geometry(mesh, node_name=name, geom_name=name)

    walk_glb = out / "student01_blocking_v5_walk8.glb"
    walk_scene.export(walk_glb)

    pose_scene = trimesh.Scene()
    for index, kind in enumerate(("stand", "sit", "crouch")):
        mesh = build_character(kind)
        mesh.apply_translation([index * 0.9, 0.0, 0.0])
        pose_scene.add_geometry(mesh, node_name=kind, geom_name=kind)

    pose_glb = out / "student01_blocking_v5_pose_set.glb"
    pose_scene.export(pose_glb)

    manifest = {
        "status": "technical blocking only; not visual approval",
        "reference_basis": ["1000005621.png", "1000005616.png"],
        "body_height_m": P["height"],
        "outputs": {
            path.name: sha256(path)
            for path in (neutral_glb, walk_glb, pose_glb)
        },
    }

    (out / "manifest.json").write_text(
        json.dumps(manifest, indent=2),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
