# AN-33 3D cast v9 checkpoint

Updated: 2026-09-28

## Purpose
Continue forward without asking the user to judge stick-figure-level detail.

This checkpoint focuses on the **system properties** that can be validated before final character-detail work:
- eight persistent actor identities,
- visible actor-to-actor variation,
- different height/body-width profiles,
- different bag presence,
- different hair-length / tied-hair accents,
- independent animation phase,
- independent local pose state,
- no global same-pose/same-timing reset.

## v9 proof
A real-3D offscreen scene was generated with eight actor instances.

Deterministic identity variations currently include:
- per-actor height scale,
- per-actor body width scale,
- per-actor hair-length scale,
- per-actor bag/no-bag state,
- tied-hair / twin-tail identity accents on selected actors.

These are blocking variations only. Reference B remains authoritative for final student_01–student_08 appearance.

## Independent-state animation
The 2-second demonstration runs at a 16 fps display rate with effective 8 fps walk-pose cadence.

During the same scene:
- some actors walk with different phase offsets,
- one actor stays idle,
- one actor sits,
- one actor enters/exits crouch on its own schedule,
- one actor remains knees-up,
- unrelated actors continue their own motion.

This proves the 3D authoring route can preserve the Actor-v3 design goal: no all-eight synchronized action.

## Persistent Library artifacts
- `/MIYAKOProject/FLOWER_3D_REFERENCE/cast_v9_review_board.png`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/cast_independent_motion_v9.gif`
- `/MIYAKOProject/FLOWER_3D_REFERENCE/flower_cast_v9_demo.py`

## Visual status
The v9 cast is **not final art**. It is still deliberately mannequin/proxy-like.

Do not ask the user to approve facial/clothing detail from this checkpoint.
Next visual work should replace primitive body/clothing forms with a more realistic human-form mesh / surface treatment while retaining the now-proven pose and identity system.

## Protected
No Flower runtime/UI/DSP/Android/MIDI/CI change.
No APK build.
Golden/main untouched.
