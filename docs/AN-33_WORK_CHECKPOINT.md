# AN-33 Work Checkpoint — Flower visual / Actor v3

Updated: 2026-09-28
Issue: AN-33
Repository: hakaihum-cpu/flower-standalone
Working branch: `feature/AN-33-flower-visual-actor-v3`

## Fixed source baseline
- Golden commit: `df2378f584f155349c33fea5e7ae212a29027049`
- Golden branch: `golden/flower-android-2026-09-27`
- Do not modify or merge `main` without explicit user instruction.

## Current task
Replace the current legacy-atlas people artwork with the latest approved Flower actor quality and revise the live animation grammar so the eight identities do not read as one synchronised group.

This is not an "animation is stopped" task. The current animation moves.

## Latest approved visual baseline
The authoritative restart contract is the MIYAKO Flower read-only checkpoint:
- repository: `hakaihum-cpu/Vstplugin`
- branch/checkpoint: `feature/flower-actor-v3`
- checkpoint commit: `820c4dcd08789cbe2fcefd481628e988622fd091`
- document: `docs/FLOWER_PAUSE_CHECKPOINT_2026-09-26.md`

That checkpoint supersedes the older Level-4 cast as the production visual baseline.

Approved:
- current high-resolution monochrome photographic character quality,
- accepted motion/cutout QA reference: `MIYAKO_Flower_8Actor_Motion_QA_v3_DEFRINGE.mp4`,
- accepted variation QA reference: `MIYAKO_Flower_8Actor_Variation_QA_v1.mp4`,
- `student_01` walking motion is the authoritative motion/quality reference,
- runtime display 16 fps with the 8-frame walk source held for two display ticks (effective 8 fps pose cadence),
- no artificial vertical bobbing,
- facing direction always matches travel direction,
- opposite travel direction derived by exact geometric mirror,
- actor-local occasional variation rather than global effects,
- no high-resolution/legacy artwork mixing in one runtime scene.

Explicitly retired / do not reintroduce:
- Level-4 degradation as the final production look,
- low-resolution 32x60 artwork as final source,
- low-resolution compositing followed by NEAREST enlargement,
- chibi/anime proportions,
- uninspected generated student assets.

Historical `flower_lofi_level_4.png` and `flower_level4_cast.zip` remain provenance/reference material only; they are not the latest production visual baseline.

## Static source findings from Flower standalone Golden
- `FlowerAnimationComponent::timerCallback()` currently advances only `advanceActors()`.
- The older scene system (`chooseNextScene`, `configureScene`, `updateAmbientBehaviours`, `updateStudentActions`, `advanceSceneIfNeeded`) remains in source but is not on the live timer path.
- Live Actor-v3 currently performs independent X-target movement and per-actor variation.
- Repository high-resolution walk source contains only `flower_actor_v3_walk_student01.png`.
- `hasCompleteHighResActorCore()` requires all eight students' left/right walk banks. Therefore Golden uses the legacy embedded atlas for all actors; it does not mix one high-resolution actor with seven legacy actors.
- The existing scene code already contains conversation, hand-holding, sitting, staggered jump, push, disperse and mixed-tableau logic, but it is not currently driving the live animation.
- Current `paint()` uses `state.currentX`; scene `xOffset` values are not part of the current Actor-v3 draw position. Re-enabling scene selection alone would therefore not be sufficient.

## Asset state carried forward from the 2026-09-26 checkpoint
Committed and approved:
- `Resources/flower_actor_v3_walk_student01.png`

Student 02–08:
- candidate right-facing 8-frame strips had been generated externally,
- they were not visually QA-approved at the pause point,
- they were not committed as production assets,
- do not assume they are valid,
- do not regenerate them merely to fill the bank before checking recoverable prior assets.

High-resolution activation rule:
- all eight students need approved source walk strips before the high-resolution Actor-v3 core is used,
- left/right counterpart may be derived by exact mirror as already approved.

## Required animation behaviour
- Eight identities remain persistent.
- Each actor owns independent movement, decision and variation timing.
- No synchronised marching, shared pose clock, global scene reset, all-eight same pose, same timing, same horizontal move or same spacing-change cue.
- Normal quiet independent movement is the baseline.
- Strange events happen only sometimes and per actor.
- Preserve actor identity, relation and position continuity.
- Do not fake ordinary poses by stretching/squashing a standing image.
- Multi-actor actions must use independently selected participants/start delays.
- Scene changes must not cancel an unrelated actor's in-progress local action.

Approved/basic actor-local variation:
- Fade
- Disperse
- Overlap
- UpperBodyWrong
- Distant
- Silhouette
- Invert
- Noise
- Freeze

## Protected / out of scope for AN-33
Do not change unless a visual asset loader addition is strictly required:
- Android startup/window fix,
- stb PNG decoder path,
- fixed 720x720 UI/layout,
- audio/DSP,
- MIDI,
- Flower waveform,
- synth controls,
- CI configuration.

## Recovery evidence already verified on 2026-09-28
Library contains:
- historical `flower_level4_cast.zip` with eight extracted standing figures,
- `MIYAKO_Flower_actual_atlas_extracted.png` / `flower_embedded_atlas_v24_q32.png` matching the current legacy multi-pose atlas lineage,
- accepted/near-accepted Flower motion QA artifacts around the Actor-v3 work,
- `MIYAKO_Flower_student01_direction_QA_v2.gif`,
- the final repository checkpoint above, which resolves the Level-4-vs-high-resolution ambiguity in favour of the later high-resolution approved baseline.

## Next actions
1. Search recoverable Library/prior artifacts specifically for the previously generated student_02–08 high-resolution walk candidates or their source cutouts.
2. Inspect any recovered candidates against the accepted student_01 / v3 DEFRINGE quality before committing anything.
3. If the exact candidates cannot be recovered, do not silently regenerate or substitute them; record the missing-bank boundary and continue only with code work that does not falsely mark the bank complete.
4. Define the smallest Actor-v3 scene/state patch needed to preserve independent actor motion while adding the desired scene variety.
5. Audit the full resource/source diff against Golden.
6. Build only after source/resource diff is fixed and audited.

## Build gate
No CircleCI build at this checkpoint.


## Approved-reference recovery — 2026-09-28
The exact three-role reference set has been recovered from Library and visually inspected:

- `1000005617.png` — finished-screen composition/staging reference.
- `1000005616.png` — eight-student identity / placement / effects reference. It contains the labelled `student_01`–`student_08` cast and rooftop example.
- `1000005615.png` — pose/body-proportion/walk reference.

These are the recovered source references behind the AN-33 role contract. They are **not** permission to invent missing per-student walk strips; derived production cutouts must preserve the referenced identity.

The 2026-09-26 Actor-v3 production rule still applies: the high-resolution output is authoritative, Level-4 degradation remains retired, and student_02–08 walk-strip candidates are not considered production assets unless recovered/verified or newly reviewed.

## Actor-v3 local-event implementation — 2026-09-28
Implemented on this branch only:
- header state commit: `19ab00e6e4691a7ba12eb6cb126528c7041b0499`
- runtime commit: `e2c0c68933177ea24b4f0e5a9e770a2c74444624`

The implementation deliberately does **not** reconnect the retired global `SceneType` scheduler.

Added actor-scoped local events:
- quiet individual pose,
- two-person conversation,
- hand holding,
- push/recoil,
- staggered two-person jump,
- single jump,
- ascend/suspend/return,
- fall/sit/rise,
- occasional odd pose.

Rules preserved:
- normal Actor-v3 independent movement stays dominant,
- pair participants approach using their own normal walk path rather than teleporting,
- local events exclude unrelated actors,
- pair/event timing is local and staggerable,
- variation is not stacked onto an active local event,
- population exit cancels only the affected local relationship,
- no global pose reset or shared pose clock.

Static audit against Golden `df2378f...`:
- branch is 4 commits ahead / 0 behind,
- changed paths are only:
  - `Source/FlowerAnimationComponent.cpp`
  - `Source/FlowerAnimationComponent.h`
  - `docs/AN-33_WORK_CHECKPOINT.md`
- `timerCallback()` still drives `advanceActors()` only; old scene functions remain dormant,
- Android startup, UI/layout, audio/DSP, MIDI, waveform and CI files are unchanged,
- no CircleCI build has been run.

## Remaining visual-bank boundary
The exact approved `student_02`–`student_08` high-resolution 8-frame walk source strips have not been recovered from Git history or Library.

The recovered cast reference gives authoritative identity appearance, but it is not itself seven complete directional walk strips. Therefore:
- do not mark image replacement complete,
- do not enable a mixed high-resolution/legacy runtime,
- do not extract compressed QA-video frames and pretend they are production source,
- do not silently generate substitute walk strips.

Also, the current high-resolution renderer has only walk + optional stand banks. The new local-event poses will require an approved per-identity high-resolution pose bank (or an explicitly approved derivation workflow) before the legacy atlas can be removed without losing pose identity.

## Current next action
1. Keep the local-event patch unbuilt until its source diff is fully audited.
2. Define the high-resolution per-identity pose/asset contract from the recovered three references.
3. Do not cross the asset boundary until production walk/pose sources are recoverable or explicitly reviewed.
4. Only then run the minimum useful build; do not spend CI minutes on an image-replacement build that still falls back to the legacy atlas.


## Non-generative extraction test — rejected
A local, non-committed extraction test was performed against the recovered eight-identity reference `1000005616.png`.

Result:
- the sheet is already flattened against a light background,
- the white blouse / skin edge values overlap the background values,
- automatic alpha recovery either removes valid body regions or retains visible light/white matte around the silhouette,
- this directly conflicts with the accepted DEFRINGE quality requirement.

Decision:
- do not commit these derived cutouts,
- do not use the flattened cast sheet as a shortcut production sprite source,
- retain it as identity/placement/effects reference only,
- production transparent actor assets still require the original clean cutouts or a separately reviewed regeneration/derivation path.
