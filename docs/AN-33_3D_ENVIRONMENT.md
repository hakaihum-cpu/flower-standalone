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
