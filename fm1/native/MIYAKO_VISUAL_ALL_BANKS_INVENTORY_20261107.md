# MIYAKO全アニメーションの元素材・FM-1移植進捗

更新日2026-11-07。元のMIYAKOリポジトリ
`hakaihum-cpu/Vstplugin@feature/miyako-ep-sampler-16part-20261030`。
FM-1版 `hakaihum-cpu/flower-standalone@feature/fm1-miyako-recorder-eureka-fx-20261107`。

**⑦の確定定義：収録したアニメーションは通常SEL画面で使用する。**
FXプレビューは同じ画像の副次的な使用先であり、FXがOFFでも通常SELに
戻れる。通常SELのSELECTで**10バンク**を手動固定できる。

## ソース実装済み（実機未確認）

- **488枚**（6枚のユーザー提供シート: 81+64+100+81+81+81）。
  各48×48正方形、16色RGB565・8×4タイル256コードブックVQ。
  元のEFFECTSと同一のパレット・タイルブック・フレームマップ配列方式。
  映像データは92,928 bytes。古い32×32・1bit形式は使わない。
  元画像からの生成は非可逆だが、元の488箇所を省略していない。
- **EFFECTS**: 元の144フレーム、収録済み（62,592 bytes）。
- **TAPE DOT**：元の `Source/MiyakoPixelMotion.h` の `Scene::Tape` にある
  65×65 DOT格子、2つのリール、60点の円、3本の回転スポーク、カセットレール。
  同じ24Hz DOT時計と既存LCD描画を使用。追加の画像アセットなし。
- **DREAMY DOT**：元の `Source/MiyakoPixelMotion.h` の
  `initPortrait()` 原画60×46ドット・2ビット（デコード690 bytes）、
  `Scene::Dreamy` の涙・粒子ストリーム、24Hzタイマー進行を通常SELに実装。
  原画Base64（920文字）は元ソースと**完全一致**。
  これは `Resources/flower_embedded_atlas.png` の人物アニメーションと
  **異なり**、後者はまだ未移植。
- **EUREKA**: 元の16クリップ×36フレーム = 576枚（250,368 bytes）。
  元の `VisualizerComponent::setAudioActivity` の音量変化に連動する
  同一クリップ内進行、必要時のみクリップ切替、無音・一定音量で静止
  する方式を移植し、GitHubとbyte-for-byte一致したCソースを使った
  GCC/Clang ASan/UBSanテストに合格。
- 6タイル群、EFFECTS、EUREKA、DREAMY DOT、TAPE DOTは**通常SELのAUTO再生対象**。
  SELECTで10バンクを切替可能。FXプレビュー専用ではない。

## 未移植

下表のTOP／SCENES／CRUSH／SAMPLER／TWILIGHT／KNOWING／CLOUDS DOT／
元のDREAMYの別画像アトラス／EP動画は、元のGitHub上で存在を確認しているだけで、
現行FM-1の実行イメージには**収録されていない**。未収録素材に対して
再生可能とする空のメニューや仮データを作らない。

## Original image and animation categories

| Source bank | Primary original source | Original content / behavior | FM-1 today |
| --- | --- | --- | --- |
| TOP/PREVIEW | `Android/resources.b64` Resources/frames; `Source/VisualizerComponent.*` | 56-frame lazy-loaded preview set; fallback 7x8 spritesheet | Absent as a separate bank |
| Six SCENES | `Android/resources.b64` Resources/scenes/{window,tears,ground,corridor,mind,nobody} | Six distinct scene image banks | Absent |
| CRUSH | `Android/resources.b64` Resources/crush | Separate crush images and interval playback | Absent |
| SAMPLER | `Android/resources.b64` Resources/sampler | Sampler playhead/region responsive images | Absent |
| TWILIGHT | `Android/resources.b64` Resources/sampler_twilight | Base, double, shadow, face, home, park, rooftop, far, gap, overlook, absence image sets; activity/scene state | Absent |
| EFFECTS | `Resources/effects_video_chunks`, `Source/VisualizerComponent.cpp` | 144 original images, onset-reactive random frames | **通常SELに収録済み** 144-frame 16-colour VQ |
| EUREKA | `Resources/eureka_*_frame_chunks`, `Source/VisualizerComponent.cpp` | 16 clips x 36 = 576 original frames, audio-triggered, not free-running | **通常SELに収録済み** 576-frame 16-colour VQ、元のクリップ進行を移植 |
| KNOWING | `Android/knowing_frames.b64`, `Source/KnowingVisualMotion.h` | 140 supplied JPGs composed with displaced tiles/eyes/hair as a performance-dependent effect | Absent |
| CLOUDS DOT | `Android/clouds_dot_frames.b64`, `Source/MiyakoDigitaktPage.h` | Dedicated DOT image sequence | Absent |
| DREAMY visual（ensemble） | `Resources/flower_embedded_atlas.png`, `Source/PluginEditor.cpp` FlowerAnimationComponent | Procedural character/ensemble motion from provided atlas | **未移植**。ただし `MiyakoPixelMotion.h` の別系統 DREAMY DOT 原画＋涙の手続き的アニメーションは通常SELに移植済み |
| EP-SAMPLER videos | `AndroidAssets/EP/*.mp4`, `Source/MiyakoEpVideoReader.h` | At least drums, pianica/accordion, saxophone, wood bass, xylophone background clips | Absent |
| DOT/VFX procedural | `Source/MiyakoPixelMotion.h`, `Source/VisualizerComponent.cpp` | Yellow DOT, performance reactive glitch, tiles, noise, jitter, scanline, RGB shift, pixelation, ghost, flicker, zoom, etc. | Absent |
| KNOWING-related eye / graphics | `Source/KnowingVisualMotion.h`, `Source/EyeKnobAtlas.h` | Procedural overlays using provided image/atlas, no synthetic source replacement | Absent |

## Confirmed resource scale and boundaries

- FM-1 installable application slot: **581,564 B** (`fm1-base/tools/fm1pkg_make.py` APP_SLOT = `0x8DFBC`).
- Existing 144 EFFECTS VQ images: **62,592 B** media.
- Existing 576 EUREKA VQ images: **250,368 B** media.
- 現在の収録済み映像合計（元EFFECTS + EUREKA + ユーザー提供488枚）: **405,888 B**。残り **175,676 B** にアプリコード・フォント等が必要。元MIYAKOの全素材が収まると判断したわけではない。
- Original Android embedded resources: 49 x 600,000 plus 344,144 characters in `Android/resources.b64` (~22.3 MB decoded bytes, before any recoding); source must be reconstructed and classified before claims about each bank.
- Original `Android/knowing_frames.b64`: 1,677,036 encoded characters (~1.26 MB decoded bytes).
- Original `Android/clouds_dot_frames.b64`: 2,479,688 encoded characters (~1.86 MB decoded bytes).
- EP MP4s are additional assets. Embedded frames / original zip sources are not equivalent to a ready-to-link FM-1 16-colour image bank.

## Mandatory porting rules

1. **Do not claim all animation banks are integrated** until every original bank is present, selectable, correctly sequenced and physically validated. A registry/menu stub is not integration.
2. Preserve source images from the original MIYAKO repository. Do not use generated or substituted frames or silently drop frames/clips. DREAMY DOTは原画690 bytesを保全し元の動きを移植済み。大きな別アトラスは未移植。
3. Aspect ratios: 488枚の正方形原画像は48×48として正方形のまま表示。元のEFFECTS/EUREKAは128×78で、それぞれ元の縦横比を保持。
4. **One shared selector** shall allow explicit bank selection independent of audio FX: SELECT/knob navigation for bank, SEL to show media; a sound FX selection must not silently replace the user's visual bank.
5. Allow separate *AUTO* routing keyed to FX/instrument for live performance, plus a manual fixed-bank override. DREAMY's procedural ensemble is distinct from EUREKA's 576 source frames.
6. Port original motion semantics (time/onset-driven/random/current-frame/tile events), not only the still images.
7. Implement VFX as shader-equivalent **native pixel/tile operations on the provided images**, not AI-generated replacements. Performance-reactive randomness must stop on silence when its source mode requires it.
8. Source-only/host simulation does not establish target link feasibility. Prove app image byte counts, free flash partitions and safe data-access semantics before changing any FM-1 flash layout or installation packaging.
9. No new build tools or CircleCI runs without permission. Do not modify main. Preserve known-good branch baseline.

## Blocking gap

**All-bank standalone playback is not yet demonstrated on FM-1.** The original asset bank size is much greater than the current firmware app slot. Retaining every source frame at the original fidelity cannot be promised from current evidence. A safe, capacity-checked storage strategy is required, not a fake success badge or hidden frame omission.
