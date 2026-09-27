# Flower Actor v3 Asset Status

Jira: AN-23 / AN-24

GoldenMaster:
`df2378f584f155349c33fea5e7ae212a29027049`

Read-only restart source:
- MIYAKO `feature/flower-actor-v3`
- `docs/FLOWER_PAUSE_CHECKPOINT_2026-09-26.md`

## Approved runtime rules preserved

- Up to 8 actors.
- Actor decisions and movement are independent.
- Actor simulation runs at 16 Hz.
- The approved 8-frame walk source advances every two actor ticks, producing the accepted 8 fps pose cadence.
- No artificial vertical bob.
- Facing direction follows travel direction.
- Opposite travel direction is derived by exact geometric mirror.
- High-resolution and legacy actor banks are never mixed in one runtime scene.
- Former Level-4 degradation is retired.
- Production compositing must not downsample to 256x144 and re-enlarge with NEAREST.
- Actor-local Variation remains independent and occasional.

## Repository production assets

Approved high-resolution walk source currently committed:
- `Resources/flower_actor_v3_walk_student01.png`

Approved embedded compatibility bank:
- `Resources/flower_embedded_atlas.png`

Approved student_02 through student_08 high-resolution walk strips:
- not present in the repository.

The current renderer intentionally gates high-resolution rendering through
`hasCompleteHighResActorCore()`. It requires both directional banks for all
8 students. Since only student_01 has an approved source strip, the GoldenMaster
correctly remains on the legacy embedded atlas at runtime.

## Saved QA/reference material inspected

The saved Library contains:
- student_01 walk/direction QA/reference sheets,
- eight-actor motion QA GIFs,
- an eight-student identity/reference sheet,
- older low-resolution / Level-4-style pose sheets.

These are useful visual references but are not equivalent to seven approved
production 8-frame source strips. The exact accepted
`MIYAKO_Flower_8Actor_Motion_QA_v3_DEFRINGE.mp4` and clean production
student_02-through-student_08 strips were not found in the inspected saved
material.

Do not extract production strips from a composited QA GIF: doing so would lose
source resolution/alpha quality and would violate the approved no-degrading /
defringing rules.

## Next production step

AN-24 remains asset-gated:
1. locate or recreate only the missing student_02-through-student_08 right-facing
   8-frame source strips,
2. inspect each strip before approval,
3. commit only approved strips,
4. derive opposite direction by exact geometric mirror at runtime,
5. keep full-bank activation gated until all eight are ready,
6. run one static resource audit,
7. only then consider one Android verification build.

No CI/build is justified while the approved production bank is incomplete.
