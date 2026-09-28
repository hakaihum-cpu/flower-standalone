#!/usr/bin/env python3
"""Blender-side student_01 technical rig/render stage.

Run with:
  blender --background --python tools/flower3d/blender_student01_stage.py -- --out <dir>

This is a technical rig/render scaffold, NOT approved final character art.
"""
import argparse
import math
import os
import sys

import bpy
from mathutils import Vector


def cli_args():
    argv = sys.argv
    argv = argv[argv.index("--") + 1:] if "--" in argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--out", required=True)
    p.add_argument("--save-blend", action="store_true")
    return p.parse_args(argv)


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for block in bpy.data.meshes:
        if block.users == 0:
            bpy.data.meshes.remove(block)


def material(name, value):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (value, value, value, 1.0)
    mat.roughness = 0.85
    return mat


def add_cube(name, location, scale, mat):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    return obj


def add_uv_sphere(name, location, scale, mat):
    bpy.ops.mesh.primitive_uv_sphere_add(
        segments=24,
        ring_count=12,
        location=location,
    )
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    return obj


def add_cylinder_between(name, a, b, radius, mat):
    a = Vector(a)
    b = Vector(b)
    mid = (a + b) * 0.5
    direction = b - a
    length = direction.length
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=20,
        radius=radius,
        depth=length,
        location=mid,
    )
    obj = bpy.context.object
    obj.name = name
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = direction.to_track_quat("Z", "Y")
    obj.data.materials.append(mat)
    return obj


def make_armature():
    arm_data = bpy.data.armatures.new("student01_armature")
    arm_obj = bpy.data.objects.new("student01_armature", arm_data)
    bpy.context.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    arm_obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")

    def bone(name, head, tail, parent=None):
        b = arm_data.edit_bones.new(name)
        b.head = head
        b.tail = tail
        if parent:
            b.parent = arm_data.edit_bones[parent]
        return b

    bone("pelvis", (0, 0, 0.88), (0, 0, 1.02))
    bone("spine", (0, 0, 1.02), (0, 0, 1.36), "pelvis")
    bone("neck", (0, 0, 1.36), (0, 0, 1.50), "spine")
    bone("head", (0, 0, 1.50), (0, 0, 1.72), "neck")

    bone("thigh.L", (0, 0.09, 0.90), (0, 0.09, 0.52), "pelvis")
    bone("shin.L", (0, 0.09, 0.52), (0.02, 0.09, 0.12), "thigh.L")
    bone("foot.L", (0.02, 0.09, 0.12), (0.18, 0.09, 0.08), "shin.L")
    bone("thigh.R", (0, -0.09, 0.90), (0, -0.09, 0.52), "pelvis")
    bone("shin.R", (0, -0.09, 0.52), (0.02, -0.09, 0.12), "thigh.R")
    bone("foot.R", (0.02, -0.09, 0.12), (0.18, -0.09, 0.08), "shin.R")

    bone("upper_arm.L", (0, 0.19, 1.32), (0.02, 0.23, 1.08), "spine")
    bone("forearm.L", (0.02, 0.23, 1.08), (0.02, 0.23, 0.82), "upper_arm.L")
    bone("upper_arm.R", (0, -0.19, 1.32), (0.02, -0.23, 1.08), "spine")
    bone("forearm.R", (0.02, -0.23, 1.08), (0.02, -0.23, 0.82), "upper_arm.R")

    bpy.ops.object.mode_set(mode="POSE")
    for pb in arm_obj.pose.bones:
        pb.rotation_mode = "XYZ"
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm_obj


def parent_to_bone(obj, armature, bone_name):
    world = obj.matrix_world.copy()
    obj.parent = armature
    obj.parent_type = "BONE"
    obj.parent_bone = bone_name
    obj.matrix_world = world


def build_proxy(armature):
    skin = material("skin", 0.74)
    shirt = material("shirt", 0.88)
    dark = material("uniform_dark", 0.07)
    hair = material("hair", 0.025)

    parts = [
        (add_cube("blouse", (0, 0, 1.19), (0.15, 0.21, 0.20), shirt), "spine"),
        (add_cube("skirt", (0, 0, 0.92), (0.20, 0.27, 0.16), dark), "pelvis"),
        (add_uv_sphere("head_mesh", (0, 0, 1.62), (0.115, 0.105, 0.135), skin), "head"),
        (add_cube("hair_back", (-0.035, 0, 1.56), (0.07, 0.13, 0.19), hair), "head"),
    ]

    limb_specs = [
        ("thigh_mesh.L", (0, .09, .88), (0, .09, .55), .06, skin, "thigh.L"),
        ("shin_mesh.L", (0, .09, .53), (.02, .09, .16), .052, skin, "shin.L"),
        ("thigh_mesh.R", (0, -.09, .88), (0, -.09, .55), .06, skin, "thigh.R"),
        ("shin_mesh.R", (0, -.09, .53), (.02, -.09, .16), .052, skin, "shin.R"),
        ("upper_arm_mesh.L", (0, .19, 1.30), (.02, .23, 1.09), .04, skin, "upper_arm.L"),
        ("forearm_mesh.L", (.02, .23, 1.07), (.02, .23, .84), .035, skin, "forearm.L"),
        ("upper_arm_mesh.R", (0, -.19, 1.30), (.02, -.23, 1.09), .04, skin, "upper_arm.R"),
        ("forearm_mesh.R", (.02, -.23, 1.07), (.02, -.23, .84), .035, skin, "forearm.R"),
    ]

    for name, a, b, radius, mat, bone in limb_specs:
        parts.append((add_cylinder_between(name, a, b, radius, mat), bone))

    parts += [
        (add_cube("shoe.L", (.09, .09, .09), (.11, .065, .045), dark), "foot.L"),
        (add_cube("shoe.R", (.09, -.09, .09), (.11, .065, .045), dark), "foot.R"),
        (add_cube("bag", (-.08, -.31, .93), (.10, .05, .16), dark), "pelvis"),
    ]

    for obj, bone in parts:
        parent_to_bone(obj, armature, bone)


def animate_walk(armature):
    scene = bpy.context.scene
    scene.render.fps = 16
    phases = [2.0 * math.pi * i / 8.0 for i in range(8)]
    frame_numbers = [1 + i * 2 for i in range(8)]

    for frame, phase in zip(frame_numbers, phases):
        scene.frame_set(frame)
        swing = 0.26 * math.sin(phase)
        bend_l = 0.18 + 0.22 * max(0.0, -math.sin(phase))
        bend_r = 0.18 + 0.22 * max(0.0, math.sin(phase))
        arm = 0.20 * math.sin(phase + math.pi)

        poses = armature.pose.bones
        poses["thigh.L"].rotation_euler.y = swing
        poses["thigh.R"].rotation_euler.y = -swing
        poses["shin.L"].rotation_euler.y = bend_l
        poses["shin.R"].rotation_euler.y = bend_r
        poses["upper_arm.L"].rotation_euler.y = arm
        poses["upper_arm.R"].rotation_euler.y = -arm
        poses["forearm.L"].rotation_euler.y = -0.08
        poses["forearm.R"].rotation_euler.y = -0.08

        for name in (
            "thigh.L",
            "thigh.R",
            "shin.L",
            "shin.R",
            "upper_arm.L",
            "upper_arm.R",
            "forearm.L",
            "forearm.R",
        ):
            poses[name].keyframe_insert(data_path="rotation_euler", frame=frame)

    scene.frame_start = 1
    scene.frame_end = 16


def look_at(obj, target):
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def setup_render(out_dir):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 384
    scene.render.resolution_y = 640
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.image_settings.compression = 25

    bpy.ops.object.camera_add(location=(3.7, -7.0, 1.25))
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 1.95
    look_at(camera, (0.0, 0.0, 0.92))
    scene.camera = camera

    bpy.ops.object.light_add(type="AREA", location=(-2.0, -4.0, 4.5))
    key = bpy.context.object
    key.data.energy = 650.0
    key.data.shape = "RECTANGLE"
    key.data.size = 4.0
    look_at(key, (0.0, 0.0, 1.0))

    bpy.ops.object.light_add(type="AREA", location=(2.0, 1.5, 2.7))
    fill = bpy.context.object
    fill.data.energy = 240.0
    fill.data.size = 3.0
    look_at(fill, (0.0, 0.0, 1.0))

    os.makedirs(out_dir, exist_ok=True)


def render_eight_frames(out_dir):
    scene = bpy.context.scene
    for i, frame in enumerate(range(1, 16, 2), start=1):
        scene.frame_set(frame)
        scene.render.filepath = os.path.join(
            out_dir,
            f"student01_walk_{i:02d}.png",
        )
        bpy.ops.render.render(write_still=True)


def main():
    args = cli_args()
    out_dir = os.path.abspath(args.out)

    clear_scene()
    armature = make_armature()
    build_proxy(armature)
    animate_walk(armature)
    setup_render(out_dir)
    render_eight_frames(out_dir)

    if args.save_blend:
        bpy.ops.wm.save_as_mainfile(
            filepath=os.path.join(
                out_dir,
                "student01_technical_stage.blend",
            )
        )


if __name__ == "__main__":
    main()
