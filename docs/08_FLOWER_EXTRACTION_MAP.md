# AN-7 Flower Extraction Map

## Read-only upstream
Repository: `hakaihum-cpu/Vstplugin`
Branch: `feature/flower-actor-v3`

No write operation is permitted against the MIYAKO repository for AN-* work.

## Directly portable component
### FlowerAnimationComponent
Source:
- `Source/FlowerAnimationComponent.h`
- `Source/FlowerAnimationComponent.cpp`

Reason:
- already encapsulates the Flower Actor v3 runtime,
- owns 8 actor identities and independent state,
- runs its own 16 Hz timer,
- receives only Flower parameter/state values,
- does not require DX7, Sampler, Twilight or Notebook FX to define actor behavior.

Initial action:
- copy into standalone source,
- preserve behavior before any refactor,
- wire to standalone Flower parameters,
- copy only the visual assets it actually requires.

## Extract from PluginProcessor
Flower-specific state/functions identified:
- `flowerWaveformBins = 256`
- `flowerGrainCount = 4`
- `getFlowerWaveform`
- `hasFlowerLoop`
- `isFlowerRecording`
- `getFlowerRecordProgress`
- `getFlowerLoopValidFraction`
- `getFlowerBasePosition`
- `getFlowerGrainPosition`
- `getFlowerActiveGrains`
- `clearFlowerLoop`
- `processFlower`
- `resetFlowerState`
- `nextFlowerRandomBipolar`
- Flower loop buffer, smoothing, telemetry and grain state
- Flower parameter declarations

Standalone adaptation:
- replace MIYAKO generated-audio source with standalone Sine synth output,
- keep Flower audio behavior first,
- remove unrelated MIYAKO processor paths.

## Extract from PluginEditor
Flower-specific UI:
- `FlowerPanel`
- `FlowerWaveformComponent`
- `FlowerAnimationComponent`
- FLOWER enable
- REVERSE
- CLEAR
- POSITION
- SIZE
- DENSITY
- SPREAD
- HOLD
- PITCH
- MIX
- FEEDBACK
- waveform telemetry
- Android full-overlay layout behavior

Current upstream note:
- `flowerRecord` and `flowerOverdub` identifiers remain in the source/state surface, but the current Actor-v3 UI does not expose REC/DUB controls.
- Current Actor-v3 audio continuously captures the most recent 16 seconds and explicitly does not use the older REC/DUB workflow.
- Standalone therefore keeps rolling capture as the baseline behavior. REC/DUB must not be reintroduced implicitly from the older written spec.

## Standalone synth module
New minimal module:
- Sine oscillator
- ADSR
- Filter
- LFO

The synth is the record/input source for Flower.

Do not import:
- DX7
- Sampler
- Twilight
- Notebook FX
- alternate oscillator types
- unrelated MIYAKO pages

## Flower parameter set to preserve initially
- flowerEnabled
- flowerRecord
- flowerOverdub
- flowerPosition
- flowerSize
- flowerDensity
- flowerSpread
- flowerHold
- flowerPitch
- flowerReverse
- flowerMix
- flowerFeedback

Standalone parameter IDs may later be renamed only through an explicit migration decision. Extraction begins by preserving semantics.

## First static implementation order
1. Introduce JUCE standalone Android project target.
2. Add minimal Sine/ADSR/Filter/LFO synth.
3. Port Flower parameter/state definitions.
4. Port Flower audio buffer/granular processing.
5. Port FlowerWaveformComponent.
6. Port FlowerAnimationComponent Actor v3.
7. Port required approved Flower visual assets only.
8. Remove temporary Java bootstrap screen.
9. Static dependency audit: confirm no MIYAKO-only symbols remain.
10. Only then run one CircleCI verification build.

No build is permitted before step 9 passes.
