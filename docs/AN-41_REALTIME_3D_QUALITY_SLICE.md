# AN-41 FLOWER realtime 3D quality slice

Updated: 2026-09-30
Branch: `feature/flower-realtime3d-pose-prototype`
Base: `golden/flower-android-2026-09-29`

## Purpose

This branch is a disposable graphics-feasibility experiment. It does **not** prove merely that OpenGL can draw a cube or a low-detail mannequin. Its purpose is to determine whether a visually useful realtime-3D FLOWER scene can run on the target Android hardware.

The user-supplied FLOWER pose sheet remains the visual/motion reference.

## Isolation contract

Protected and unchanged:
- `Source/PluginProcessor.cpp`
- `Source/PluginProcessor.h`
- DSP / synth / looper
- MIDI implementation
- Carnival implementation
- Golden branch
- main
- old AN-33 offline-3D branch

The production XY/config code remains present on this branch and is only hidden behind the new visual component. No production asset is deleted.

## Realtime rendering path

This experiment uses JUCE `juce_opengl` and an actual Android OpenGL context.

Runtime path:

`Realtime3DPoseComponent -> GLSL ray-marched 3D scene -> Android GPU -> screen`

It does **not** render 3D assets to PNG frames and does not use the old AN-33 offline 3D-to-2D pipeline.

## Quality target

The first vertical slice intentionally includes rendering work that is representative of a finished visualizer rather than a minimal 3D proof:
- full 3D rooftop scene,
- articulated human silhouette with hair, blouse, skirt, bag, socks and shoes,
- automatic pose sequence,
- walk cycle,
- walk start/stop,
- front-to-back turn,
- back view,
- sit,
- crouch,
- knees-up sit,
- lie-down pose,
- dynamic lighting,
- soft shadow,
- ambient occlusion,
- atmospheric fog,
- rooftop material variation,
- near-monochrome grading,
- film grain,
- scan structure,
- vignette,
- highlight/bloom approximation,
- realtime FPS display.

## Acceptance

Do not mark the 3D direction successful merely because the shader starts.

Record on physical hardware:
1. app starts without ANR/crash,
2. all pose states appear,
3. 3D remains realtime throughout the full cycle,
4. measured FPS at 720x720,
5. visible stutter/jank,
6. thermal behaviour after several minutes,
7. audio regression / dropout while the existing engine remains loaded,
8. whether the visual result is good enough to justify replacing the 2D visual route.

### Working interpretation

- ~60 FPS: comfortable headroom for refinement.
- ~40-60 FPS: viable; optimise before increasing detail.
- ~30-40 FPS: potentially viable for a deliberately cinematic visualizer.
- below ~30 FPS: current high-quality shader path is too heavy and must be simplified or changed.

These are project gates, not claims about device capability before testing.

## Build policy

Follow the repository build-last rule:
- static diff audit first,
- source review first,
- exactly one justified APK build after the branch is ready,
- no rerun without analysis of the previous result.

No merge to main or Golden without explicit user instruction.
