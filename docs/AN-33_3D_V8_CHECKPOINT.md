# AN-33 3D student_01 v8 checkpoint

Updated: 2026-09-28

## Status
The current ChatGPT host can now generate and offscreen-render **real 3D geometry** without requiring Blender on the user's PC.

The current visual proxy is still not final production character art, but it is beyond the earlier stick-figure technical blocker and now has:
- rounded human-form torso/head/limbs,
- long dark hair mass and side locks,
- blouse, dark skirt, bow and shoulder bag,
- clean RGBA output,
- actual 3D view changes,
- actual 3D walk-cycle geometry,
- expanded pose bank.

## v8 generated pose bank
Produced and archived:
- neutral front / side / 3/4 / back,
- 8-frame side walk,
- sit,
- crouch,
- knees-up sit,
- lying pose,
- turn: front / diagonal-front / side / diagonal-back / back,
- walk start 01 / 02,
- walk stop 01 / 02.

## Persistent Library artifacts
- `/MIYAKOProject/FLOWER_3D_REFERENCE/student01_visual_v8_neutral.glb`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/student01_visual_v8_walk8.glb`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/student01_visual_v8_review_board.png`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/student01_visual_v8_walk.gif`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/FLOWER_student01_visual_v8_posebank.zip`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/student01_visual_v8.py`

ZIP SHA-256:
`4b3f29ee75d80c91beec7ae658634447b943aa2f7353b2ac01597317b78b64ed`

GLB SHA-256:
- neutral: `59423a9f8284d0960ee71fd88fcf04aebd2eb168dea362776f817c26f395a2ca`
- walk8: `4bfc70ebf189ab829f2ac79288034e004af5f2c263be66d1b557214ddbba6383`

## Alpha result
All eight walk frames were revalidated after visual post-processing:
- fully transparent pixel RGB max = **0**
- white-matte contamination is not introduced by the current VTK path.

## Current interpretation
The v8 proxy is suitable for continuing animation/pose-system development without asking the user to judge a stick figure.

It is **not yet** suitable for final runtime replacement. Remaining visual work is primarily:
- face/hair realism,
- blouse and skirt construction,
- hands/feet,
- bag construction,
- texture/photographic treatment,
- reference-fit of body silhouette.

## Source tracking
The v8 generator is preserved in the persistent Library while the branch keeps the stable v3/v5 procedural source and renderer tools. Do not promote any v8 render to production merely because the mechanical/alpha checks pass.

## Protected
No runtime / Android / DSP / MIDI / UI / CI change has been made for this v8 checkpoint.
No APK build was run.
