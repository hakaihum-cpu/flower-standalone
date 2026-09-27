# Flower Standalone Extraction Scope

Jira: AN-7

## Source reference
Read-only source repository:
`hakaihum-cpu/Vstplugin`

Reference branch:
`feature/flower-actor-v3`

Key observed source objects:
- `Source/PluginEditor.cpp` blob `ef3bcc279e3e92f4515e5f63ac2d5ff431ebcf19`
- `Source/PluginProcessor.cpp` blob `0f5aefeadd36570cb973a0edaabf90ac9723d2eb`
- `Source/SynthVoice.cpp` blob `1b82bdac38fa2ab3514cad0c58b8e05188303fb8`
- `Source/VisualizerComponent.cpp` blob `0e82b0236473f70ddcccc95fc32fc569926c6147`
- `docs/FLOWER_SPEC.md` blob `67c38266868183ba76f0843705ad1bfe33b50926`

These identifiers document what was inspected. They do not create a build dependency on MIYAKO.

## Reuse unchanged in behavior first
- 16-second Flower loop concept
- REC / DUB / CLEAR
- feedback and reverse
- four-grain granular playback
- Flower waveform/telemetry concepts
- Flower parameter semantics
- Actor Engine v3:
  - independent actors
  - no global pose reset
  - independent motion/timing
  - 16 Hz actor simulation
  - no synchronous whole-cast movement
  - progressive actor entry/exit
  - continuity and stable identities
- approved 2026-09-26 visual baseline described by the Actor v3 Flower spec

## Replace for standalone
MIYAKO record/input source becomes the standalone synth/audio source.

Initial synth:
- Sine oscillator
- ADSR
- Filter
- LFO

## Remove
- DX7
- Sampler
- Twilight
- Notebook FX
- MIYAKO-only oscillator choices
- unrelated MIYAKO pages and state
- MIYAKO compatibility requirements that only exist to preserve old MIYAKO parameter indices

## UI rule for first standalone baseline
Reuse Flower layout and visual language first.
Only remove controls that become meaningless outside MIYAKO and add the minimum Sine/ADSR/Filter/LFO controls required to make the standalone instrument usable.

Do not perform a general visual redesign during extraction.

## Definition of first standalone parity
The first parity checkpoint is reached when:
1. Android app launches independently of MIYAKO.
2. MIDI note input can sound the Sine synth.
3. ADSR, Filter and LFO affect the synth.
4. Flower can record that synth output.
5. Flower loop playback works.
6. DUB, CLEAR, feedback and reverse work.
7. Four-grain processing and core parameters work.
8. Waveform/telemetry reacts correctly.
9. Actor v3 visual behavior runs without MIYAKO dependencies.
10. No MIYAKO repository access is needed to build or run.
11. MIYAKO repository remains unchanged.
12. Static checks pass before the first CI build.

Only after this checkpoint do we treat new Flower behavior as design work rather than extraction work.
