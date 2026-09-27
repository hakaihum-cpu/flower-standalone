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
Replace the current people artwork with the previously approved Flower visual source and revise the live animation grammar so the eight identities do not read as one synchronised group.

This is not an "animation is stopped" task. The current animation moves.

## Approved visual source
- Canonical cast appearance: exact approved `flower_lofi_level_4.png`.
- Do not redraw it, re-pixelate it, add a new Level-4-style filter, or approximate it with newly generated art.
- Preserve the approved low-resolution monochrome treatment, body proportions, low facial information and identity differences.
- Pose/action artwork must come from previously approved/recoverable source material. Missing production art is not to be invented.

Reference-role contract:
1. first reference: finished-screen composition/staging,
2. second reference: eight identities, individual variation, placement/effects,
3. third reference: pose/body-proportion/walk standard.

## Static source findings from Golden
- `FlowerAnimationComponent::timerCallback()` currently advances only `advanceActors()`.
- The older scene system (`chooseNextScene`, `configureScene`, `updateAmbientBehaviours`, `updateStudentActions`, `advanceSceneIfNeeded`) remains in source but is not on the live timer path.
- Live Actor-v3 currently performs independent X-target movement and per-actor variation.
- Repository high-resolution walk source contains only `flower_actor_v3_walk_student01.png`.
- `hasCompleteHighResActorCore()` requires all eight students' left/right walk banks. Therefore Golden uses the legacy embedded atlas for all actors; it does not mix one high-resolution actor with seven legacy actors.
- The existing scene code already contains conversation, hand-holding, sitting, staggered jump, push, disperse and mixed-tableau logic, but it is not currently driving the live animation.
- Current `paint()` uses `state.currentX`; scene `xOffset` values are not part of the current Actor-v3 draw position. Re-enabling scene selection alone would therefore not be sufficient.

## Required animation behaviour
- Eight identities remain persistent.
- Each actor has independent timing/state.
- No all-eight same pose, same timing, same horizontal move or same spacing-change cue.
- Ordinary rooftop scenes dominate; strange/impossible events are occasional.
- Support scene-like combinations such as conversation, sitting/standing, one actor walking or looking back, hand holding, staggered jump/float, push/recoil, disappear/remain, disperse/odd/overlap.
- Preserve actor identity, relation and position continuity between scene changes.
- Walking remains restrained: low pose cadence, minimal vertical bounce, direction/facing consistent with travel.
- Do not fake ordinary poses by stretching/squashing a standing image.

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

## Next actions
1. Recover the exact previously approved Level-4-derived cast/pose assets from Library, prior APK/archive, or read-only historical MIYAKO Flower source.
2. Verify recovered assets visually and by file/hash provenance before using them.
3. Define the smallest resource + Actor-v3 patch required to use those assets and wire independent scene grammar into the live path.
4. Audit the full diff against Golden.
5. Build only after the source/resource diff is fixed and audited.

## Build gate
No CircleCI build at this checkpoint.
