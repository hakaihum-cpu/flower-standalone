#!/usr/bin/env python3
"""Technical orthographic renderer for the real-geometry student_01 proxy.

This is a pipeline/alpha test only.  It does not replace the Blender render
pipeline or the approved visual references.
"""

import argparse
import importlib.util
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def load_generator(path):
    spec = importlib.util.spec_from_file_location("student01_proxy", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def premultiplied_downsample(image, size):
    arr = np.asarray(image).astype(np.float32) / 255.0
    alpha = arr[..., 3:4]
    rgb = arr[..., :3] * alpha
    premult = np.concatenate([rgb, alpha], axis=2)

    channels = []
    for i in range(4):
        channel = Image.fromarray(
            np.clip(premult[..., i] * 255.0, 0, 255).astype(np.uint8),
            "L",
        )
        channels.append(
            np.asarray(
                channel.resize(size, Image.Resampling.LANCZOS)
            ).astype(np.float32)
            / 255.0
        )

    alpha2 = channels[3][..., None]
    rgb2 = np.stack(channels[:3], axis=2)
    rgb2 = np.where(
        alpha2 > 1.0e-5,
        rgb2 / np.maximum(alpha2, 1.0e-5),
        0.0,
    )

    out = np.concatenate(
        [np.clip(rgb2, 0.0, 1.0), np.clip(alpha2, 0.0, 1.0)],
        axis=2,
    )
    return Image.fromarray((out * 255.0 + 0.5).astype(np.uint8), "RGBA")


def render_side(mesh, width=360, height=600, supersample=3):
    scale = supersample
    render_w = width * scale
    render_h = height * scale

    vertices = np.asarray(mesh.vertices)
    faces = np.asarray(mesh.faces)
    colors = np.asarray(mesh.visual.face_colors)

    xmin, xmax = vertices[:, 0].min(), vertices[:, 0].max()
    zmin, zmax = vertices[:, 2].min(), vertices[:, 2].max()

    pad_x = max(0.08, (xmax - xmin) * 0.16)
    pad_z = max(0.06, (zmax - zmin) * 0.05)
    xmin -= pad_x
    xmax += pad_x
    zmin -= pad_z
    zmax += pad_z

    sx = (render_w - 1) / (xmax - xmin)
    sz = (render_h - 1) / (zmax - zmin)

    def project(point):
        x = (point[0] - xmin) * sx
        y = (zmax - point[2]) * sz
        return float(x), float(y)

    # Side camera: X horizontal, Z vertical, Y depth.
    # Painter order is sufficient for this deliberately simple technical proxy.
    order = np.argsort(np.mean(vertices[faces][:, :, 1], axis=1))[::-1]

    image = Image.new("RGBA", (render_w, render_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image, "RGBA")

    for face_index in order:
        points = [project(vertices[index]) for index in faces[face_index]]
        color = tuple(int(value) for value in colors[face_index])
        draw.polygon(points, fill=color)

    return premultiplied_downsample(image, (width, height))


def composite(frame, background):
    background = (
        background.convert("RGB")
        .resize((720, 480), Image.Resampling.LANCZOS)
        .convert("RGBA")
    )

    actor = frame.copy()
    actor.thumbnail((150, 320), Image.Resampling.LANCZOS)
    x = 360 - actor.width // 2
    y = 430 - actor.height
    background.alpha_composite(actor, (x, y))
    return background.convert("RGB")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--generator", required=True)
    parser.add_argument("--out", default="proxy_render")
    parser.add_argument("--reference-a", default=None)
    args = parser.parse_args()

    generator = load_generator(args.generator)

    out_dir = Path(args.out)
    frames_dir = out_dir / "frames"
    frames_dir.mkdir(parents=True, exist_ok=True)

    frames = []
    for i in range(8):
        phase = 2.0 * math.pi * i / 8.0
        mesh = generator.character_pose(phase)
        frame = render_side(mesh)
        frame.save(frames_dir / f"student01_walk_{i + 1:02d}.png")
        frames.append(frame)

    frames[0].save(
        out_dir / "student01_walk_technical.gif",
        save_all=True,
        append_images=frames[1:],
        duration=125,
        loop=0,
        disposal=2,
    )

    if args.reference_a and Path(args.reference_a).exists():
        reference = Image.open(args.reference_a).convert("RGB")
        w, h = reference.size
        rooftop = reference.crop(
            (
                int(w * 0.835),
                int(h * 0.605),
                int(w * 0.995),
                int(h * 0.795),
            )
        )
        composite(frames[0], rooftop).save(
            out_dir / "student01_alpha_rooftop_check.png"
        )

    # Alpha acceptance: fully transparent pixels must contain no white matte.
    pixels = np.asarray(frames[0])
    transparent = pixels[..., 3] == 0
    transparent_rgb = pixels[..., :3][transparent]
    max_transparent_rgb = (
        int(transparent_rgb.max()) if transparent_rgb.size else 0
    )

    (out_dir / "ALPHA_CHECK.txt").write_text(
        f"frame_count=8\n"
        f"transparent_pixel_rgb_max={max_transparent_rgb}\n"
        "expected transparent_pixel_rgb_max=0 "
        "(no white matte in transparent pixels)\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
