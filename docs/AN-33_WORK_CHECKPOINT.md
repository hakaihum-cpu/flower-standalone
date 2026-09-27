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
