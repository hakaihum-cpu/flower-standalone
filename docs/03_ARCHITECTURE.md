# Architecture

## Product architecture decision for AN-7

The extraction baseline keeps the Flower implementation in **JUCE/C++** rather than rewriting Flower into the temporary Java bootstrap UI.

Reason:
- the MIYAKO Flower audio engine, UI and Actor v3 behavior are already implemented in JUCE/C++,
- the requirement is reuse-first, not redesign-first,
- a native-Java rewrite would change too many variables before parity is established.

The current Java `MainActivity` is therefore bootstrap-only and may be replaced when the JUCE standalone target is introduced.

## Standalone target
- Android standalone application
- JUCE/C++ audio/UI core
- Sine oscillator only
- ADSR
- Filter
- LFO
- Flower looper + four-grain engine
- Flower waveform/telemetry UI
- Flower Actor v3 rooftop runtime
- MIDI support retained only where required by standalone behavior

## Explicitly excluded from the first extraction
- MIYAKO DX7 code
- MIYAKO Sampler code
- Twilight
- Notebook FX
- unrelated MIYAKO visualizer modes
- MIYAKO VST3 target
- cross-repository runtime/build dependency

## Extraction principle
Do not copy entire MIYAKO files unchanged merely because Flower code is currently embedded inside them.

For large shared files such as `PluginEditor.cpp` and `PluginProcessor.cpp`:
1. identify the Flower-specific code,
2. extract the minimum required supporting code,
3. preserve Flower behavior,
4. remove MIYAKO-only branches,
5. keep source provenance documented,
6. validate statically before running CI.

## Independence boundary
Once copied, standalone files are owned by flower-standalone.
Future standalone changes do not imply corresponding MIYAKO changes.
Future MIYAKO changes do not automatically flow into standalone.
