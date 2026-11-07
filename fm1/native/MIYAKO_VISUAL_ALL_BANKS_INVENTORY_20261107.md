# MIYAKO-to-FM-1: complete animation-bank inventory (source audit, NOT integrated firmware)

Status: 2026-11-07, source inventory only. **No CI build authorised or started.**
Authoritative original MIYAKO:
`hakaihum-cpu/Vstplugin@feature/miyako-ep-sampler-16part-20261030`.
Port under development:
`hakaihum-cpu/flower-standalone@feature/fm1-miyako-recorder-eureka-fx-20261107`.

## 2026-11-07 implementation update: original user tile sheets

- **Implemented in the FM-1 source, not target-linked or device-tested:** all 488
  cells from six newly supplied **square** 1536x1536 contact sheets
  (81 portrait poses, 64 eye tiles, 100 Shibuya tiles, 81 Shibuya tiles,
  81 portrait tiles, 81 portrait tiles). No cell was deliberately skipped.
- The generated `fm1/native/mi_tile_atlas.h` is a 32x32 **lossy**, 1-bit
  8x8 VQ bank (256 8-byte dictionary entries + 488 x 16-byte index maps;
  9856 decoded bytes, 13144 base64 bytes in XIP). Its checksum is
  SHA-256 `27e7e780db7ae202e2a2d420fee7ff4c8de984787b4cb3fd76b0c0f0d929180c`.
  The original images are neither fabricated nor replaced with AI artwork.
- New FM-1 FX screen is 240x240: physical K1..K4 on left with four 2x2
  dial cells, a **square** photo-tile preview on the right. SELECT in FX or
  SEL views cycles AUTO / 6 photo banks / EFFECTS / EUREKA (0..8);
  FX selection/sound WET remain independent. SEL opens full-screen image.
  Audio amplitude drives tile selection, small VHS tracking/tearing,
  displaced 8x8 source blocks, and sparse yellow DOT accents; silent
  animations freeze according to the existing scheduler.
- **Not complete:** the old Android-only TOP/SCENES/CRUSH/SAMPLER/TWILIGHT,
  KNOWING, CLOUDS-DOT, DREAMY ensemble and EP video banks are still absent.
  The native FX audio code is not an exact original Dreamy/Eureka port.
- The current EFFECTS/EUREKA banks are still **128x78 VQ** and therefore
  their original source aspect metadata has not been restored. Only the six
  newly supplied photo-tile banks are guaranteed to render 1:1.
- Source-integrity/read-only validation succeeded outside CircleCI;
  no JieLi target compile, link, FWSC package, or FM-1 hardware test has
  been executed for this change. **Do not trigger CircleCI without consent.**

## Original image and animation categories

| Source bank | Primary original source | Original content / behavior | FM-1 today |
| --- | --- | --- | --- |
| TOP/PREVIEW | `Android/resources.b64` Resources/frames; `Source/VisualizerComponent.*` | 56-frame lazy-loaded preview set; fallback 7x8 spritesheet | Absent as a separate bank |
| Six SCENES | `Android/resources.b64` Resources/scenes/{window,tears,ground,corridor,mind,nobody} | Six distinct scene image banks | Absent |
| CRUSH | `Android/resources.b64` Resources/crush | Separate crush images and interval playback | Absent |
| SAMPLER | `Android/resources.b64` Resources/sampler | Sampler playhead/region responsive images | Absent |
| TWILIGHT | `Android/resources.b64` Resources/sampler_twilight | Base, double, shadow, face, home, park, rooftop, far, gap, overlook, absence image sets; activity/scene state | Absent |
| EFFECTS | `Resources/effects_video_chunks`, `Source/VisualizerComponent.cpp` | 144 original images, onset-reactive random frames | **Present** 144-frame 16-colour VQ |
| EUREKA | `Resources/eureka_*_frame_chunks`, `Source/VisualizerComponent.cpp` | 16 clips x 36 = 576 original frames, audio-triggered, not free-running | **Present** 576-frame 16-colour VQ (FX condition), not a separate selectable bank |
| KNOWING | `Android/knowing_frames.b64`, `Source/KnowingVisualMotion.h` | 140 supplied JPGs composed with displaced tiles/eyes/hair as a performance-dependent effect | Absent |
| CLOUDS DOT | `Android/clouds_dot_frames.b64`, `Source/MiyakoDigitaktPage.h` | Dedicated DOT image sequence | Absent |
| DREAMY visual | `Resources/flower_embedded_atlas.png`, `Source/PluginEditor.cpp` FlowerAnimationComponent | Procedural character/ensemble motion from provided atlas | Absent |
| EP-SAMPLER videos | `AndroidAssets/EP/*.mp4`, `Source/MiyakoEpVideoReader.h` | At least drums, pianica/accordion, saxophone, wood bass, xylophone background clips | Absent |
| DOT/VFX procedural | `Source/MiyakoPixelMotion.h`, `Source/VisualizerComponent.cpp` | Yellow DOT, performance reactive glitch, tiles, noise, jitter, scanline, RGB shift, pixelation, ghost, flicker, zoom, etc. | Absent |
| KNOWING-related eye / graphics | `Source/KnowingVisualMotion.h`, `Source/EyeKnobAtlas.h` | Procedural overlays using provided image/atlas, no synthetic source replacement | Absent |

## Confirmed resource scale and boundaries

- FM-1 installable application slot: **581,564 B** (`fm1-base/tools/fm1pkg_make.py` APP_SLOT = `0x8DFBC`).
- Existing 144 EFFECTS VQ images: **62,592 B** media.
- Existing 576 EUREKA VQ images: **250,368 B** media.
- Combined existing media: **312,960 B**, leaving only **268,604 B** for all application/board code, fonts and other animation data.
- Original Android embedded resources: 49 x 600,000 plus 344,144 characters in `Android/resources.b64` (~22.3 MB decoded bytes, before any recoding); source must be reconstructed and classified before claims about each bank.
- Original `Android/knowing_frames.b64`: 1,677,036 encoded characters (~1.26 MB decoded bytes).
- Original `Android/clouds_dot_frames.b64`: 2,479,688 encoded characters (~1.86 MB decoded bytes).
- EP MP4s are additional assets. Embedded frames / original zip sources are not equivalent to a ready-to-link FM-1 16-colour image bank.

## Mandatory porting rules

1. **Do not claim all animation banks are integrated** until every original bank is present, selectable, correctly sequenced and physically validated. A registry/menu stub is not integration.
2. Preserve source images from the original MIYAKO repository. Do not use generated or substituted frames or silently drop frames/clips.
3. Asset processing must preserve source aspect ratios: square user images stay square when displayed on the FM-1's square 240x240 LCD; do not flatten 720x720 into 128x78 and then stretch to 240x146.
4. **One shared selector** shall allow explicit bank selection independent of audio FX: SELECT/knob navigation for bank, SEL to show media; a sound FX selection must not silently replace the user's visual bank.
5. Allow separate *AUTO* routing keyed to FX/instrument for live performance, plus a manual fixed-bank override. DREAMY's procedural ensemble is distinct from EUREKA's 576 source frames.
6. Port original motion semantics (time/onset-driven/random/current-frame/tile events), not only the still images.
7. Implement VFX as shader-equivalent **native pixel/tile operations on the provided images**, not AI-generated replacements. Performance-reactive randomness must stop on silence when its source mode requires it.
8. Source-only/host simulation does not establish target link feasibility. Prove app image byte counts, free flash partitions and safe data-access semantics before changing any FM-1 flash layout or installation packaging.
9. No new build tools or CircleCI runs without permission. Do not modify main. Preserve known-good branch baseline.

## Blocking gap

**All-bank standalone playback is not yet demonstrated on FM-1.** The original asset bank size is much greater than the current firmware app slot. Retaining every source frame at the original fidelity cannot be promised from current evidence. A safe, capacity-checked storage strategy is required, not a fake success badge or hidden frame omission.
