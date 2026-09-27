# Requirements

## Confirmed
REQ-001: The project develops an Android application.

REQ-002: The application is a completely standalone version of Flower. MIYAKO is a separate project and must not be modified by this project.

REQ-003: MIYAKO may be used only as a read-only source reference for the initial extraction. No standalone change is written back to MIYAKO unless explicitly requested in the future.

REQ-004: The initial Flower standalone baseline reuses the latest MIYAKO Flower implementation from `feature/flower-actor-v3` as closely as practical before new behavior is designed.

REQ-005: The initial synth is intentionally minimal:
- oscillator: Sine only
- ADSR envelope
- Filter
- LFO

REQ-006: The initial UI reuses the existing Flower page/UI as the starting point. Do not redesign it during the extraction phase unless required to remove MIYAKO-only controls.

REQ-007: Initial Flower behavior to retain from the current Actor-v3 source:
- continuous rolling capture of the most recent generated synth audio, up to 16 seconds
- Flower bypass still keeps the rolling capture current while leaving the dry signal unchanged
- CLEAR resets the rolling buffer/state
- 4-grain granular engine
- POSITION / SIZE / DENSITY / SPREAD / HOLD / PITCH / REVERSE / MIX
- waveform / rolling-buffer-validity / playhead telemetry
- approved rooftop/cast visual baseline
- Actor Engine v3 independent-actor behavior and continuity rules
- existing Flower MIDI behavior where it remains meaningful in standalone

The older REC/DUB workflow in `docs/FLOWER_SPEC.md` is superseded by the current Actor-v3 source. `flowerRecord` and `flowerOverdub` parameter IDs may remain in the extracted state for compatibility while no visible REC/DUB controls or active REC/DUB audio path are present. FEEDBACK remains an inherited parameter/UI surface but is not used as an overdub feedback control while REC/DUB is inactive.

REQ-008: MIYAKO-only systems are not part of the standalone baseline unless explicitly reintroduced:
- DX7
- Sampler
- Twilight
- Notebook FX
- other MIYAKO oscillator types
- MIYAKO-specific pages unrelated to Flower
- MIYAKO project/release state

## Isolation rule
The two repositories are independent products.
- Read MIYAKO source when necessary.
- Copy required code/assets into flower-standalone.
- Never edit MIYAKO as part of AN-* work.
- Never make MIYAKO depend on flower-standalone.
- Never make flower-standalone depend on MIYAKO repository contents at build/runtime.
- After extraction, standalone evolves independently.

## Still undecided
- Final app/package name shown to users
- Final applicationId
- Minimum supported Android version
- Exact standalone main-screen arrangement after the reused Flower UI baseline
- Whether CONFIG/MIDI setup remains a separate page or is simplified
- Audio/MIDI device policy beyond the first working baseline
- Distribution/signing/store policy
- Further Flower behavior changes after standalone parity is established

## Requirement change rule
Confirmed requirements are changed only by an issue that records:
- previous requirement
- requested change
- reason
- impact
- verification method
