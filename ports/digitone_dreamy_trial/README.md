# Digitone OS1.44 — EUREKA 04 HYBRID + GRANULAR LONG HANN v0.5

This experimental **SOURCE-ONLY** branch is based on the user-tested VM76
custom firmware (EUREKA 576 original user-supplied HYBRID04 frames, MIDI VFX
and the stereo GRANULAR insert). The previous VM76 binary, original HG76
user-tested baseline and all original visual frames remain unchanged.

**User-reported VM76 issue:** GLOBAL MUTE green LEDs toggle on and off but
internal sequencer notes keep playing, whether GRANULAR is enabled or not.
**MUTE AUDIO IS NOT FIXED IN v0.5.** The MUTE path investigation is in
[docs/DIGITONE_VM76_MUTE_GRANULAR_V05_ANALYSIS.md](../../docs/DIGITONE_VM76_MUTE_GRANULAR_V05_ANALYSIS.md).
The EUREKA key callback already passes MUTE events to stock firmware; this
was insufficient. Do not relabel visual LED passthrough as an audio MUTE fix.

## New GRANULAR v0.5: longer, less glitchy texture

- 2.731s stereo 16-bit circular history (131072 frames, 512 KiB) at 48 kHz,
  compared to the prior 0.683s, 128 KiB.
- A pair of 50%-overlapped Hann-window grains per pitch voice, with a
  256-phase precomputed window (no live trigonometry).
- MIDI CC36 grain length **120–360ms**, 5ms steps; **240ms** is the
  default. CC63 and CC64 both select exactly 240ms.
- The source lag is ~1.46s and ~1.58s before the advancing live head;
  0–62.5ms of additional old-history offset remains available on CC37.
- The effect stays dry until 90000 valid audio frames have been recorded
  (1.875s), then fades into the requested wet/dry mix over 80ms.
- Wet output averages the two pitch voices, instead of halving their
  combined level again: musical sustained grains should be more present.
- Pitch A/B choices on CC33/34 stay the user's eight discrete semitones:
  **−12, −7, −5, 0, +5, +7, +12, +24** (16 CC values per choice).
  The defaults remain **+5/+12**.
- CC60 MIX 0–127 unchanged; no new effects or feedback added.
- Pitch and grain size/position changes latch when a new grain begins.
- Uses Q20.12 audio read positions to avoid +24×360ms integer overflow.
  Safety bounds are asserted for both fastest and slowest pitches.
- The Digitone's built-in FM synthesis, Delay and Reverb are left intact,
  and the output still uses CORE_AUDIO_INSERT after the stock master.
- The HYBRID04 video, its note reactions and video MIDI CC mapping
  (40/65/67/68/69) are untouched.

## Building

**No firmware build is authorized by this request.** This branch includes
a manual-only GitHub Actions workflow; it must not be run without the owner's
fresh explicit permission. The existing SYX files are unchanged.

When authorized, the workflow must pass full original 576/576 frame and
8,192-pixel tests, extended native/UBSan granular tests, cross-compilation,
Elekloader lint and firmware SHA-256 verification. The already working
software must remain available as the fallback.
