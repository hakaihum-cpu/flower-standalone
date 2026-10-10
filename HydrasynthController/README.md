# HYDRA DOT / AN-62

Dedicated **Hydrasynth Explorer Android remote editor** for RG Rotate (720 x 720 design
coordinates). **Independent APK package** `com.analoglav.hydrasynthcontroller` inside a
feature-only source tree. Existing MIYAKO/FLOWER/EP assets or MASTER branches are unchanged.

## What works in source (NOT device-verified)

- One permanent screen: MODULE → PARAMETER → human-readable VALUE → SEND.
- All selectors and CONFIG are overlays of the same 720-square View; no wizard or activity transitions.
- 4-color warm yellow / black dot presentation and Bayer 4x4 dither, drawing rules
  adapted from previously developed DOT UI, no AI image assets.
- Select **MIDI OUT device, MIDI IN device, channel 1–16** in CONFIG; observe MIDI IN
  byte activity (not parameter sync). Android MIDI API supports class-compliant USB and
  devices that Android exposes.
- **No implicit sending on selection or adjustment.** SEND transmits NRPN via MIDI CC
  99/98/6/38. Raw CC numbers, MIDI parameter numbers are never presented as user controls.
- Mappings include OSC1/2/3 waves and pitch/keytrack, OSC1/2 WaveScan 1–8, four Mutants,
  Ring/Noise, Mixer routing, Filter types/routing, Delay, Reverb and some LFO settings.
- Unsupported modules remain visible with **NO VERIFIED MIDI MAP** and disabled SEND.
- Values are **staged locally**. TX SENT means Android passed bytes to its MIDI port,
  **not** confirmation from the synth. There is no SysEx transfer and no remote patch save.

## Sources / critical limitation

- ASM official Explorer Owner's Manual 2.2.0:
  https://www.mecldata.com/download/asm/Hydrasynth_Explorer_Owners_Manual_2.2.0.pdf
- ASM official (legacy keyboard/desktop) MIDI NRPN & CC Communication Spec FW1.5:
  https://www.mecldata.com/download/asm/legacy/Hydrasynth_KB_DR_MIDI_Spec_1.5.0.pdf
- Version mismatch: mappings and especially **219-wave ordering and device response must
  be checked on Explorer firmware 2.2.0**, not assumed to be equivalent.
- Comprehensive implementation remains pending for ENV1–5, macro assign, mod matrix,
  arp, pre/post FX dependent menus, patch readback and all system functions.
- Avoid parameter guessing. The legacy spec documents many variable-dependent encodings,
  some of which are not safely mapped yet.

## Source-level tests / Android CI

`NrpnEncoder` and `ParameterCatalog` are pure Java 17 and intentionally independent
of Android. See `test_core.sh`. Android CI runs **only by CircleCI parameter**
`run_build=true` (default false); GitHub Actions never used.
This branch uses its own CircleCI build definition targeting `HydrasynthController`.

Status: **SOURCE MVP / NOT BUILT / NOT TESTED ON HYDRASYNTH EXPLORER**.
