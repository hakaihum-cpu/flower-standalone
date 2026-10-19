# EFFECTS / Pure Data patch mode — source contract

Status: implementation requirements and test fixture ONLY. A native libpd runtime
has NOT yet been compiled, linked, or exercised in EFFECTS. Never treat this
branch as a successful Android patch-import build.

## Intent
Users author real, ordinary Pure Data (.pd) patches in Pd (as on Organelle)
and import them in EFFECTS from CONFIG. An imported patch changes the actual
realtime DSP, not just built-in parameters. This is an additive mode; existing
modes CHORD A/B, DREAMY, EUREKA, CHORDBOT, CLOUDS, ULTRASHEER, ROSETONES,
SOLARFAZE, POLYPHRASE, MULTI are unchanged.

The previous experimental feature/effects-patch-import-delay-20261019 branch
invented a JSON modules schema and is REJECTED as unrelated to the requirement.
Do not build or promote it.

## Native engine / compatibility
Use upstream libpd as the embedded Pure Data Vanilla engine and pin a proven
source revision with its pure-data submodule. Integrate using the same CircleCI
manual_android_build pipeline after full static checks and smoke tests. Do not
use the independent Android Pd audio hardware stream: JUCE continues to own the
existing audio device and its routing/buffers, passing its audio data to libpd.

Input and output routing: Pd [adc~ 1 2] receives input L/R; Pd [dac~ 1 2]
returns 2 independent audio channels. The CONFIG Input Mode setting being built
in feature/effects-config-input-mono-stereo-20261019 is orthogonal: STEREO
supplies true L/R, MONO duplicates its single physical input to both Pd input
lanes; output remains fixed 2ch. The two feature branches require explicit
integration before release; they have NOT been merged.

A Pd patch is a .pd text file, not a new JSON extension. Vanilla objects
should work when backed by libpd. Community "externals" are NOT guaranteed:
each external needs an explicit native build and initialization. Likewise,
patches depending on other local .pd abstractions require file-directory
import and path resolution, not silent substitution with the invented JSON DSL.
Provide a clearly visible error on missing objects/open failure.

Import: CONFIG -> LOAD PATCH -> select actual .pd file. Preserve a copy in
private app storage and restore the last accepted patch on app relaunch.
Operations which load/unload/reconfigure the Pd graph must never occur
concurrently with a live render callback. On failed load, retain the previous
working patch. Real DSP runs in PATCH mode and does not rewrite preexisting
effects.

Sample-frame integration: libpd uses 64-frame ticks; preserve L/R channel
assignment, implement a preallocated buffering adapter from arbitrary JUCE
callback blocks to 64-frame interleaved Pd buffers without heap allocation or
files opened on the realtime thread. Include silence/invalid-state handling,
bounded input/output, saved settings, and no output gain surprises.

## PATCH video
PATCH-only video selection combines all supplied EUREKA clips AND Vongon clips.
It uses EUREKA onset/sustained-input random cut opportunities and the Vongon
input/XY/parameter-driven pacing and degradations. Videos can switch during
playback. No fixed carousel interval and no new/generated media.

## Test fixture
patches/Effects_Simple_Stereo_Delay.pd is **real Pd source**:
  adc~ 1 2 -> independent delay L/R with 360 ms each;
  feedback 0.34, wet 0.65, dry 0.35 -> dac~ 1 2.
Uses only vanilla: adc~, dac~, delwrite~, delread~, +~, *~.
The static patch graph has 16 objects / 18 wires and separate delay lines.

## Acceptance gates
1. Compile libpd and its exact pinned pure-data submodule into JUCE Android.
2. Import provided .pd from CONFIG; verify full audio graph executes.
3. Test an L-only impulse and verify NO echo appears in R; repeat R-only.
4. Verify feedback/tail and clip safety with actual offline Pd DSP.
5. Verify stereo/mono input choices on physical USB interface and 2ch output.
6. Verify arbitrary valid user-authored Pd Vanilla patches, invalid/missing
   patches, restoring after restart, and old patch retention after a failure.
7. Verify timing/visual banks for PATCH and that 0..8 modes remain unchanged.
8. Keep main/Golden/#214 untouched; only build on explicit permission via
   CircleCI manual_android_build and run_build=true; no GitHub Actions.

No compile, CircleCI, or runtime test has been claimed on this branch.
