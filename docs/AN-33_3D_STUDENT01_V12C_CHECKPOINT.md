# AN-33 student_01 photographic 3D visual-hull v12c

Updated: 2026-09-28
Branch: `feature/AN-33-flower-3d-prototype`

## What changed
The earlier procedural mannequin path was not visually close enough to the locked Flower references.

For student_01, the branch now also contains a **photographic visual-hull** path:
- the locked front reference silhouette and locked side/motion reference frames are segmented,
- orthogonal silhouettes are intersected into real 3D voxel occupancy,
- marching-cubes produces actual 3D meshes,
- reference grayscale is projected back onto the mesh as vertex colour,
- VTK offscreen rendering produces transparent RGBA frames,
- tiny disconnected segmentation artefacts are removed,
- no user-side Blender installation is required.

Source:
- `tools/flower3d/student01_hull_v12c.py`
- commit: `364f518aedeba2ee0b82af6334d492a7e1da0189`

## Why this exists
The goal is to preserve the exact photo-like monochrome atmosphere instead of drifting toward anime/CG mannequin rendering.

This is real 3D geometry, but it is not yet the final rigged production character. The current walk is an 8-mesh 3D morph/pose sequence reconstructed from the approved motion references.

## Outputs preserved in Library
- `FLOWER_student01_3D_sample_walk.gif`
- `FLOWER_student01_3D_sample_parallax.gif`
- `FLOWER_student01_3D_sample_review.png`
- `FLOWER_student01_3D_sample_v12c.zip`
- `student01_hull_v12c.py`

The ZIP includes:
- neutral GLB,
- 8-pose walk GLB scene,
- transparent walk PNG frames,
- walk GIF,
- slight-angle 3D parallax GIF,
- review comparison board,
- alpha report.

## Alpha
All 8 walk frames: fully-transparent RGB max = 0.
No white matte is introduced by this path.

## Important limitation
The current visual hull is strongest near the locked front/side viewpoints.
Large-angle rotation is not yet production quality because front/side silhouette intersection does not contain enough true depth information.

Therefore:
- use v12c as the first visually meaningful one-person sample,
- do not call it the final rigged model,
- do not integrate it into FLOWER runtime yet,
- next step is to convert this visually matched hull into a more stable articulated/poseable representation while keeping the reference look.

## Protected
Golden/main untouched.
No APK build.
No runtime/DSP/MIDI/Android/CI modification in this checkpoint.
