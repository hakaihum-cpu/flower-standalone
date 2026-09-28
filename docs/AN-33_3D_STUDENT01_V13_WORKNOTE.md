# AN-33 student_01 photographic relief v13 work note

Updated: 2026-09-28

## Direction
Continue from v12c, but do not show the user another ambiguous mannequin/proxy checkpoint.

The new path preserves the supplied photographic side reference much more directly:
- segment the supplied student_01 side reference,
- create real 3D relief geometry from the silhouette using a depth field,
- project the original monochrome reference back onto the mesh,
- render with transparent RGBA through VTK,
- keep fully-transparent RGB at zero.

## Current result
The neutral/side actor is now visually much closer to the supplied target than the earlier procedural mannequin and v12c front/side-intersection hull.

A real 3D parallax test has also been produced from the same actor.

## Remaining blocker before showing the next sample
The one-person walk bank is not yet clean enough.

The low-resolution walk-sheet extraction still introduces segmentation / cell-alignment artefacts in some frames. These must be fixed before the next user-facing sample, because the user explicitly asked not to be shown confusing intermediate results.

Therefore:
- do not promote the current 8-frame walk bank,
- do not integrate into runtime,
- continue refining the clean 8-frame source/masks,
- only show the next sample when one complete student_01 walk cycle is visually coherent.

## Protected
- Golden/main untouched.
- No APK build.
- No CI run.
- No runtime/DSP/MIDI/Android modification in this checkpoint.
