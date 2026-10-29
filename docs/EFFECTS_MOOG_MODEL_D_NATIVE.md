# Minimoog Model D / CA-72 native Pure Data bridge

## Current status
**Source integration only, not a validated Android release.** No CircleCI build or Android-device test has been run for this branch. Do not promote or call the new `.pd` file compatible with any currently installed EFFECTS APK until it is built and verified. The patch intentionally depends on a native object `moog_model_d~`; existing pure Vanilla EFFECTS cannot load that object.

## Engine provenance
The engine is the GPL-3.0-or-later CA-72 circuit-derived Model D voice by Idle Foundry Ltd.: https://github.com/idlefoundry/ca-72 . The dependency is pinned to commit `cfd5859e913b4969e5416abd026cce135c37ffad` (2026-10-29). It is derived from original instrument schematics and SPICE transcriptions, including oscillator, filter, envelope, noise, keyboard and VCA boards. **It is not a literal at-runtime simulation of every original electronic part.** In particular this bridge defaults to CA-72's highest-fidelity `NoCompromises` mode and must not be described as a full-device SPICE solve or an exact every-component simulation. At eight simultaneous voices, realtime Android performance is **unverified and may be infeasible**.

CA-72's GPL obligations apply to redistributed derivative binaries. Keep its attribution and publish corresponding source under compatible terms if distributing this modified application.

## Audio routing
JUCE retains Android audio devices. One embedded libpd native object produces two output channels (same monophonic-per-voice mix duplicated). MIDI note events arrive at Vanilla `[notein]`, and normalized X/Y from EFFECTS arrive at `[r effects-x]` and `[r effects-y]`. `[r knob3]` and `[r knob4]` set amount of contour and filter attack.

## Integration
The bridge is compiled into a static Rust Android aarch64 archive using the existing pinned NDK in the existing *manual* CircleCI workflow. It is linked to the existing libpd/JUCE Android target with a C Pd external registered before loading the user file. No parallel CI provider or standalone Pd audio driver is added. No build was triggered by authoring this branch.

## Required acceptance gates
1. Compile Rust and Pd C native external, link arm64 without unresolved symbols.
2. Verify loading the patch through CONFIG and receiving MIDI On/Off + XY.
3. Run hardware tests on target Android device at 44.1/48/96 kHz, checking xrun, audio clipping, CPU and memory, note stealing, and release.
4. Verify 8-voice stability; if it fails, do not silently reduce requested polyphony or CPU fidelity.
5. Compare rendered waveforms and selected operating points against the CA-72 reference and the original circuit SPICE test data.
