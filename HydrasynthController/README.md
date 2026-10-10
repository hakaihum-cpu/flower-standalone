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
  Ring/Noise, Mixer routing/source levels, Filter cutoff/resonance/model, Amp level, Delay wet/feedback, Reverb wet/time, some LFO settings, ENV 1–5 triggers/curves/sync, ARP Division/Swing/Gate/Octave/Length/Tap/Phrase/Ratchet/Chance, Voice performance settings, Macro panel value (but not Macro Assign).
- Unsupported modules remain visible with **NO VERIFIED MIDI MAP** and disabled SEND.
- Values are **staged locally**. SEND is disabled until the user explicitly selects/adjusts a value, because the app cannot read current synth values. TX SENT means Android passed bytes to its MIDI port,
  **not** confirmation from the synth. There is no SysEx transfer and no remote patch save.

## Sources / critical limitation

- ASM official Explorer Owner's Manual 2.2.0:
  https://www.mecldata.com/download/asm/Hydrasynth_Explorer_Owners_Manual_2.2.0.pdf
- ASM official (legacy keyboard/desktop) MIDI NRPN & CC Communication Spec FW1.5:
  https://www.mecldata.com/download/asm/legacy/Hydrasynth_KB_DR_MIDI_Spec_1.5.0.pdf
- Version mismatch: mappings and especially **219-wave ordering and device response must
  be checked on Explorer firmware 2.2.0**, not assumed to be equivalent.
- Comprehensive implementation remains pending for ENV time envelopes, Macro Assign/Target/Depth,
  Mod Matrix routing, some Arpeggiator modes, pre/post FX dependent menus, patch readback,
  System parameters and numerous continuous engine parameters.
- Avoid parameter guessing. The legacy spec documents many variable-dependent encodings,
  some of which are not safely mapped yet.

## Source-level tests / Android CI

`NrpnEncoder` and `ParameterCatalog` are pure Java 17 and intentionally independent
of Android. See `test_core.sh`. Android CI runs **only by CircleCI parameter**
`run_build=true` (default false); GitHub Actions never used.
This branch uses its own CircleCI build definition targeting `HydrasynthController`.

Status: **SOURCE MVP / NOT BUILT / NOT TESTED ON HYDRASYNTH EXPLORER**.

## Sound-design workbench (source revision)
- Top: patch identity + dirty state, Explorer signal-path/module map (OSC 1-3,
  Ring/Noise, Mutants, Mixer, Filters, AMP, FX, and modulation families).
- Middle: current module's visible control cards (four at a time, accessible in
  the same pane); each tap opens an **in-place** discrete-option or numeric editor.
- Bottom: SEND FIELD, APPLY PATCH (all deliberately staged parameter values,
  paced at 22ms), NEW, SAVE, LOAD, CONFIG.
- Local SAVE and LOAD now use AtomicFile-backed schema-1 patch documents;
  patch files are app-private. Name editing uses an in-place Android keyboard dialog.
- This version does **not** create full manufacturer binary patches, remotely
  perform INIT, or write a hardware preset slot. APPLY PATCH edits the current
  synth's active temporary patch; the hardware must be set to a known initial
  patch first. A complete no-touch workflow requires further firmware-verified
  patch import/initialization & persist-to-device support.
- Normalized 0–100% controls added only for NRPN parameters whose v1.5
  default 14-bit raw range is [0,8192]. These controls represent **relative**
  positions, not certified cut-off Hz/time or full 14-bit precision; further
  validation is required before claiming fidelity to Explorer FW2.2.
- FW1.5 VOICE/Glide Off/On mapping conflicts with Explorer FW2.2.0 manual
  Off/Glide/Glissando. Disabled pending verified implementation.

- Envelope 1–5 cards now surface **ADSR** together as a working shape: Attack, Decay,
  Sustain, Release normalized positions (FW1.5 NRPN positions). Hold and secondary
  controls remain available in the same pane via next controls. Time in milliseconds
  is not fabricated from these values. BPM sync-specific values require distinct mapping.

## CurrentLoad: read-only saved-slot patch acquisition (feature/an-62-currentload-readback)

The single 720x720 workspace now has a **CURRENTLOAD** control next to NEW/SAVE/LOAD/CONFIG.
It opens a bank A–H and slot 1–128 selector overlay without any wizard or new Activity.
Both **MIDI OUT and MIDI IN** must be connected to the Hydrasynth. Read-only SysEx:

1. Open session via encoded HEADER 18 00; wait for HEADER RESPONSE 19 00.
2. Request selected saved bank/slot 04 00 [bank 0..7] [slot 0..127].
3. Receive 22 ordered chunks (21 × 128 bytes plus final 102 bytes), validate
   base64+CRC32, and ACK 17 00 [index] 16 after each. Collect **2790 raw bytes**.
4. Send FOOTER 1A 00 and wait for 1B 00; then validate version and slot metadata.
5. **Only on complete validation**, atomically replace the editor's working document,
   patch name, and all *currently implemented* UI mappings. The full original raw
   patch remains available for private on-device SAVE/LOAD alongside the JSON overlay.

A READ timeout, missing/corrupt chunk, bad CRC or wrong slot **leaves the current
editor untouched** and attempts to send a session-release FOOTER. CANCEL READ is
available while a transaction is in progress. There is **no Flash WRITE command**.
For safety, after CurrentLoad, APPLY EDITS transmits only values deliberately
changed in the UI after loading, rather than re-transmitting every imported value.

### Critical limitations

- The Synth's **unsaved current edit buffer cannot be queried** with the known
  SysEx commands. CURRENTLOAD therefore means *read a specified SAVED slot*, not
  'find and read whatever is currently sounding'. The user must select the bank
  and slot, and the app never guesses.
- It reads **all raw bytes** but currently maps only the subset of engine controls
  defined in ParameterCatalog into semantic UI fields. E.g. Mod Matrix and Macro
  Assign do not yet have complete UI coverage; RAW retention must not be mistaken
  for 100% GUI parameter coverage.
- A raw baseline retained in a local preset is an **original snapshot**, not
  regenerated when editor values change. Hardware flash writes / patch SysEx
  upload are NOT supported or enabled.
- Edisyn's reverse-engineered patch and message format, not ASM's published
  guarantee: https://github.com/eclab/edisyn/blob/master/edisyn/synth/asmhydrasynth/info/SysexEncoding.txt
  and https://github.com/eclab/edisyn/blob/master/edisyn/synth/asmhydrasynth/ASMHydrasynth.java.
- Android FW2.2.0 USB MIDI reply chunk sizes/handshake have **not been tested**
  on an actual Explorer. A reply using undocumented chunk lengths is rejected
  rather than incorrectly installed as a complete patch.

### Auto Send (separate from CurrentLoad)

CONFIG also includes **AUTO: ON/OFF** (default ON). It sends **only confirmed
user edits** to a connected MIDI output; opening/scrolling modules, receiving
a SysEx patch, and loading a local preset cause **no MIDI parameter writes**.
SEND FIELD is retained as a manual fallback.

### Build safety

Changes are on independent feature branch `feature/an-62-currentload-readback`.
The previous working branch and main are unchanged. `test_core.sh` now includes
pure-Java SysEx fixture tests (CRC, fragmented MIDI, 22-ACK sequence, slot identity,
mapped UI values, corrupt/out-of-order cancellation and no Flash WRITE). Source
has **not** been built in CircleCI or tried on device.
