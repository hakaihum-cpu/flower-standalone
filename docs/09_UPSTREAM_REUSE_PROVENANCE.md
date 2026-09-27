# Upstream Flower Reuse Provenance

This directory contains code/assets copied into the standalone project from the MIYAKO repository as a one-time extraction baseline.

Read-only upstream:
- repository: `hakaihum-cpu/Vstplugin`
- branch: `feature/flower-actor-v3`

Copied without logic redesign:
- `Source/FlowerAnimationComponent.h`
- `Source/FlowerAnimationComponent.cpp`
- `Source/RetroLookAndFeel.h`
- `Source/RetroLookAndFeel.cpp`
- `Resources/flower_embedded_atlas.png`
- `Resources/flower_actor_v3_walk_student01.png`

Upstream blob IDs at extraction:
- FlowerAnimationComponent.h: `3e64de47666541ef2487b729540c12f96d0a3ae6`
- FlowerAnimationComponent.cpp: `e7d6ba2cab824fd61e39441b0198337b9a82b6b0`
- RetroLookAndFeel.h: `84437adc6aca0db95e5eb3407901abf4af44d62a`
- RetroLookAndFeel.cpp: `8e3f3ad844427da7bc3aefd4b8a16873105da405`
- flower_embedded_atlas.png: `ec5873bb022f6efdef9fc72c0099ca71e344887b`
- flower_actor_v3_walk_student01.png: `d516c56bef14ae5cc2e73e755b4f221ddf2aa04d`

After this copy, these files belong to flower-standalone. There is no automatic sync back to MIYAKO.
