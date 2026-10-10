# HYDRA DOT / AN-62

Combined **HYDRA / ANALOG KEYS Android remote editor** for RG Rotate (720 x 720 design
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

## AN-64: SYNTH DOT — one APK, two separate synthesis modes

**One app / one Android package:** `com.analoglav.hydrasynthcontroller`;
the existing APK package is retained so users do not get duplicate standalone
apps. Application name is **SYNTH DOT**, Android versionCode 2, versionName
0.2.0-dev. This branch is NOT a rename/merge of any unrelated app.

**Mode switch:** CONFIG → SYNTH MODE → HYDRA or ANALOG KEYS.
Switching modes or Analog Keys tracks does not send MIDI or overwrite any
stored preset. In-memory edits and module navigation are preserved separately.
HYDRA remains the original CurrentLoad/NRPN workbench. Analog Keys has a
different signal-flow grid with TRK 1–4, FX and PERF and per-track MIDI channels.
MIDI OUTPUT and INPUT use the Android MIDI API to reach a compatible *5-pin DIN*
MIDI interface; **Analog Keys does not need to be connected via USB**.
The selected output MIDI interface and each track's MIDI channel must match
the actual device wiring and settings. Enable RECEIVE CC/NRPN in Analog Keys.

**Analog Keys official reference:** Elektron *Analog Keys User Manual
(OS 1.55, June 2026)* Appendix D MIDI:
https://www.elektron.se/wp-content/uploads/2026/06/Analog-Keys-User-Manual_ENG_OS1.55_260610.pdf

**Implemented editor fields:** OSC1/2, Noise, OSC Common, two filters, AMP,
filter/user envelopes, LFO1/2; EXT IN, Chorus, Delay, Reverb, FX LFO1/2 on the
FX channel; performance macros A–J on their configured performance channel.
Only the explicitly documented NRPN addresses are enabled. MIDI Data Entry
**MSB-only parameters send value << 7** and documented 14-bit parameters
send true 14-bit values. Track Mute/Track Level in the manual use **Data
Entry LSB only** and are sent with the correct low-seven-bit encoding.
Hydrasynth's unrelated NRPN mappings and custom scalings are NOT reused.

**AUTO SEND:** enabled by default and stored independently for each mode,
with CONFIG ON/OFF. Each user-confirmed choice or USE VALUE sends that field
to the configured MIDI port (the device is not polled for an ACK);
scroll/navigation/mode changes and local preset LOAD never transmit edits.
APPLY SOUND manually sends all staged Analog Keys fields for the selected
track, paced like the original HYDRA APPLY; any pending staged transmission
is cancelled before switching tracks, modes, or output ports.

**Local presets:** still NEW / SAVE / LOAD, but analog tracks have independent
AtomicFile JSON documents in private files (one per TRK 1–4, FX, PERF). HYDRA
keeps its original unmodified preset storage. These are local **editor
assignments**, not complete Analog Keys Sound files or a hardware Kit.

### CURRENTLOAD / +Drive / Kit — not yet supported on Analog Keys

Analog Keys mode visibly disables CURRENTLOAD, explaining that a verified
Analog Keys **Sound SysEx read protocol is not yet implemented**. The
Hydrasynth mode retains its saved-slot CurrentLoad; the two instruments have
incompatible SysEx protocols. Likewise, **SAVE is local to the app**:
there is NO Analog Keys +Drive Sound store, Kit write, or automatic read of
the current unsaved sound in this branch. Do not claim to have implemented
them; the prior firmware research was not an Android device test. The
design is intentionally safe from accidental device patch/Kit overwrites.

### Source/test/build status

The non-destructive AN-64 work is confined to branch
`feature/an-64-analog-keys-dot`. Hydra original code remains in the same
Android app; main and Golden Baseline are unchanged.
`test_core.sh` runs Hydra, HydraDump, and AnalogKeysCoreTest's
pure-Java address/value regression checks. **No Android build or hardware
test has yet occurred.** A successful Java fixture still does not prove DIN
MIDI communication on a specific phone/interface/firmware combination.
CircleCI retains the existing manual workflow and default `run_build=false`;
no GitHub Actions.

### Build signing / upgrade blocker (must resolve before a real APK replacement)

The inherited CircleCI `.circleci/config.yml` creates a **new debug.keystore**
with `keytool -genkeypair` inside each short-lived build container and
`build.gradle` signs the release variant with `signingConfigs.debug`.
An Android app update requires the **same signing certificate** as the
previously installed APK; identical `applicationId` alone is not enough.
`AndroidManifest.xml` currently sets `allowBackup=false`, so uninstalling
the previous version can delete app-private patch libraries that cannot
automatically be restored.

**This branch has NOT yet implemented a compatible signing migration.**
Do not tell users a new APK can safely overwrite an installed HYDRA DOT
release without checking certificate compatibility. A stable secret keystore
must be established for future CircleCI builds, with the fingerprint checked
against the currently installed application's signer if that signer is known.
If it is not known or cannot be recovered, agree a data-preserving migration
plan first. Do not commit signing keys or copy an arbitrary new debug key
into the repository.
