# Digitone EUREKA RG DOT + GRANULAR (experimental)

This is an experimental **granular-only** port based on the previous
DREAMY prototype. Because the DREAMY prototype repeatedly froze on real
Digitone hardware, it is NOT considered reliable. Do not use or distribute
it as a validated firmware without further tests.

- Hardware: Digitone mk1 / Digitone Keys, official OS 1.44.
- Visual: EUREKA original 576 frames, 128x64 1-bit Bayer RG DOT;
  sound-driven jump, reversal, speed and stutter remain unchanged.
- Mod Menu **GRANULAR**: toggle the master insert.
- Audio: two simultaneous granular pitch voices at **+5 semitones**
  (perfect fourth up) and **+12 semitones** (octave up). Each pitch
  uses two overlapping, triangular-window stereo grains.
- **No added delay, reverb or feedback network.** Both the stock Digitone
  DELAY and REVERB remain available, with their original controls.
- AUTO-channel MIDI **CC 60**: dry/wet MIX (0–127, default 64).
  CC 61 and 62 are **not intercepted** by this mod.
- The existing Digitone FM and stock FX remain enabled.
  Audio path is **stock FM/stock delay/reverb -> GRANULAR insert -> output**:
  the stock FX cannot provide a post-granulator reverb in this routing.
- 48 kHz stereo, 32 frames per render, Q1.31 I/O and Q15 intermediate.
  The history ring uses about 128 KiB, down from DREAMY's ~430 KiB.
  Enabling from Mod Menu resets counters only: it never clears the
  128 KiB array in a UI callback.
- Existing base OS, UI assets, EUREKA frames and master remain unchanged.

## Safety and verification

Removing the custom delay and reverb reduces the audio callback work but
does **not** prove this code meets the Digitone real-time CPU budget.
Likewise, a successful host test, Elekloader lint or firmware build cannot
prove absence of the prior hardware freeze. No emulator, measured CPU budget,
or real-device validation has yet been completed for GRANULAR-only.

The firmware is configured for an existing **manual-only** GitHub Actions
build. There is no auto-trigger. Do not build or upload to hardware without
the owner's explicit permission, and use a low monitor volume for the first
hardware experiment.
