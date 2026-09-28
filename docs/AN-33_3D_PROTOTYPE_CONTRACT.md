# AN-33 FLOWER 3D Prototype Contract

Updated: 2026-09-28
Branch: `feature/AN-33-flower-3d-prototype`

## Branch ancestry
- This 3D experiment branches from the current AN-33 visual/Actor-v3 line at:
  - `9a24000cf8f8ca8e0920c569fb83185886958264`
- Original standalone Golden remains:
  - branch: `golden/flower-android-2026-09-27`
  - commit: `df2378f584f155349c33fea5e7ae212a29027049`
- `main` and the Golden branch must not be changed or merged into without explicit user instruction.

## Purpose
Test a 3D-authored production pipeline for the Flower cast while preserving the approved visual atmosphere.

Primary reasons:
1. avoid white-fringe / matte contamination from extracting actors from flattened white-background sheets,
2. keep each student's face, hair, uniform, bag, proportions and identity stable across poses,
3. create repeatable walking, direction changes, sitting, crouching, lying, pair actions and other poses,
4. output controlled 2D runtime assets without requiring real-time 3D inside the Android app.

## Authoritative user references

### Reference A — motion / pose / effect sheet
Source file supplied by user: `1000005621.png`
SHA-256:
`a2cf73dbf157ca6a02e3af18bf721f9c52def9192221ad78b4c94e0057a13bb1`

Role:
- 8-frame walk-cycle appearance,
- walk start / stop,
- direction / turn,
- sitting / crouching / lying,
- group combinations,
- effect / variation atmosphere.

### Reference B — cast / scene sheet
Source file supplied by user: `1000005616.png`
SHA-256:
`01ccf84be7382bfdf81a2520d11cece678ed53ac2f242422769c8f81452b9a1d`

Role:
- student_01–student_08 identity baseline,
- relative height / body variation,
- rooftop placement,
- group composition,
- ordinary-pose and effect language.

These two references are preserved separately in the persistent Library under:
`/MIYAKOProject/FLOWER_3D_REFERENCE/`

Do not reinterpret either reference as anime character art.
Do not replace realistic monochrome proportions with anime/chibi proportions.

## Current prototype material
The previously generated 3D-look concept sheet and its walk preview are preserved only as prototype evidence under the same Library folder.

They are NOT an approved production model or source asset.
They must never replace Reference A/B as the visual authority.

## 3D production direction
Initial 3D use is OFFLINE AUTHORING, not real-time runtime rendering.

Pipeline:
1. build one persistent rigged character identity,
2. animate it in 3D,
3. render approved views / animation frames with alpha,
4. generate final transparent PNG frame banks,
5. feed those frames to the existing Actor-v3 runtime.

Do not add a real-time 3D engine, 3D asset loader, skeleton runtime or GPU dependency to Flower standalone during this prototype.

## White-fringe prevention
The 3D render pipeline must:
- render directly with alpha instead of cutting the body out of a white flattened image,
- use a consistent transparent background,
- preserve correct alpha edges around white blouse, socks, skin and hair,
- avoid white-matte premultiplication,
- inspect reduced-size runtime PNGs over the actual rooftop background before acceptance,
- reject any frame with a visible white halo.

## Visual invariants
Keep the atmosphere of the user references:
- monochrome / near-monochrome,
- realistic human proportions,
- understated late-1990s / early-2000s game-event visual feeling,
- quiet body language,
- restrained low-frame-rate walking,
- no exaggerated bounce,
- no glossy modern 3D-game look,
- no cel-shaded anime look,
- no beautified anime face redesign.

## First implementation gate: student_01
Before producing eight full characters, complete student_01 only:

1. neutral standing front,
2. side standing / travel facing,
3. 8-frame walk cycle,
4. walk start,
5. walk stop,
6. front / diagonal / side / back turn references,
7. sit,
8. crouch,
9. knees-up sit,
10. lying pose.

Acceptance checks:
- identity is stable across every view,
- same hair length / silhouette,
- same uniform construction,
- same bag,
- same leg length and body ratio,
- no white fringe when composited over rooftop,
- walk direction and body facing agree,
- vertical head movement remains restrained.

Do not scale to student_02–08 until student_01 passes visual review.

## Later stages
After student_01 approval:
- create eight identities from Reference B,
- reuse the same rig topology where practical,
- preserve identity-specific hair/bag/body details,
- derive movement from shared base motion only where it does not make all eight actors look synchronized,
- retain per-actor timing offsets and Actor-v3 local events.

## Existing Actor-v3 work
The parent AN-33 branch already contains the local-event experiment for:
- quiet pose,
- conversation,
- hand holding,
- push/recoil,
- staggered jump,
- jump,
- ascend,
- fall/sit/rise,
- odd pose.

The 3D branch may provide production-quality pose frames for these events, but must not restore the retired global SceneType scheduler.

## Protected areas
Do not change during the 3D prototype unless explicitly necessary and separately reviewed:
- Android startup / ANR fix,
- stb PNG decoding path,
- fixed 720x720 UI/layout,
- audio/DSP/looper,
- MIDI,
- waveform,
- CI configuration,
- Golden branch,
- main.

## Build gate
No CI/APK build merely for 3D asset authoring.
First finish and visually inspect student_01 frame assets.
Only build after runtime-facing asset/code changes exist and have passed static diff review.

## Current status
- 3D branch created.
- Original 2D references preserved in persistent Library.
- Golden/main untouched.
- No 3D production model has been claimed complete yet.
