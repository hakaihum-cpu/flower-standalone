# FLOWER 3D Environment

Pinned environment for AN-33 3D prototype.

## Blender
- Version: Blender 4.5.14 LTS
- Linux x64 archive: https://download.blender.org/release/Blender4.5/blender-4.5.14-linux-x64.tar.xz
- Windows x64 portable ZIP: https://download.blender.org/release/Blender4.5/blender-4.5.14-windows-x64.zip
- Reason: LTS + portable/headless operation; avoids tying the prototype to a workstation install.

## Execution model
- Authoring/rendering: Blender headless + Python.
- Runtime: unchanged Flower Actor-v3 2D PNG frame playback.
- GitHub Actions: NOT used for 3D asset iteration.
- main / Golden: untouched.

## Output contract
Each accepted pose/action is exported as transparent PNG with:
- RGBA,
- Film Transparent,
- no white matte,
- straight alpha handling,
- fixed camera,
- fixed render scale,
- fixed floor baseline,
- deterministic naming.

The first gate is student_01 only.

## Current host limitation
The current ChatGPT container does not ship with Blender.
An attempt to use the Debian package path timed out and direct archive download was unavailable in the current host session.
Therefore the repository contains deterministic setup scripts so the exact environment can be recreated on a host that can fetch the official Blender archive, without spending GitHub Actions minutes.

This limitation does NOT change the branch strategy or references. Do not fake 3D by substituting image generation and calling it a Blender render.


## Technical 3D proof completed in current host
Because Blender itself is unavailable in this host, a separate **real-geometry technical proxy** was produced with `trimesh` to validate that this branch is not relying on fake "3D-looking" images.

Repository tools:
- `tools/flower3d/student01_technical_proxy.py`
- `tools/flower3d/render_technical_proxy.py`

Persistent Library outputs:
- `/MIYAKOProject/FLOWER_3D_REFERENCE/student01_neutral_technical_proxy.glb`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/student01_walk_8pose_technical_proxy.glb`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/FLOWER_3D_student01_technical_proxy_render.zip`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/student01_alpha_rooftop_check.png`

GLB SHA-256:
- neutral: `c1ef96e3c446ad5c896c45549066cb5c5ca439e5e23159d917566d90fa115e6e`
- 8-pose walk scene: `a8d2734d21545e2600153d59571693f83a8c1d97137a0e404539d8e8c416f6f0`

Render package SHA-256:
- `d8e30699e01ff1f84ae1f24a69856f154a0ccdda3cc7ec5a87dab7110c7b0e5f`

Alpha test result:
- 8 transparent PNG walk frames were generated from actual 3D geometry.
- fully transparent pixels have RGB max = **0**.
- therefore the technical pipeline introduces **no white matte into transparent pixels**.

Important:
- this proxy is intentionally crude and is **not** a visual candidate for Flower.
- it proves geometry/export/alpha handling only.
- the approved look still comes exclusively from Reference A/B.
- the next visual-quality gate still requires the Blender/artist-refined student_01 model.


## Execution responsibility correction — 2026-09-28
The user is **not required to install Blender** and no assumption should be made that Blender already exists on the user's PC.

The Windows bootstrap is only an optional portable path for future use:
- it downloads Blender into the project-local `.flower3d-tools` directory,
- it does not require a system-wide Blender installation,
- it must not be presented as a required user action unless the user explicitly agrees to run it.

Current working rule:
- ChatGPT continues all preparation, source generation, validation logic, reference locking and branch management itself.
- Do not hand the execution burden to the user by default.
- Because the current ChatGPT host cannot execute Blender, the real Blender-rendered visual gate remains blocked unless a separate executable Blender host becomes available or the user explicitly chooses to run the portable local path.
- Do not substitute AI image generation and call it a real 3D render.
