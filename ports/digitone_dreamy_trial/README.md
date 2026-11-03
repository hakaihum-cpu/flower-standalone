# Digitone OS1.44 — EUREKA 2.0 + GRANULAR LONG HANN 0.5

Source-only integration built from the working GH76 firmware components.
GH76 is unchanged. No new SYX has been built for this branch.

## Mute audio repair (source and tests updated, hardware pending)

User-tested reproduction on GH76:

- Internal-sequencer audio track is muted in normal/global Mute mode
- Enter EUREKA with MIDI+YES: the audio remains muted but all track LEDs
  illuminate green
- Pressing TRACK changes the LEDs but does not unmute/mute the synth audio
- EUREKA OFF restores the regular audio track Mute operation
- This is independent of whether the GRANULAR insert is enabled

Root cause in original shortcut handling: the MIDI key press was passed
to stock Digitone OS before the YES was consumed by EUREKA. In GLOBAL
MUTE, the stock MIDI key switches the displayed Mute control group from
audio tracks to MIDI tracks, matching the user's preserved synth audio
mute with green MIDI track LEDs.

The EUREKA 2.0 key hook now defers a bare MIDI DOWN:

- If followed by YES, **neither the MIDI DOWN nor MIDI UP** reaches stock;
  EUREKA owns the paired YES DOWN/UP and toggles video
- If MIDI is pressed alone, send one original-stock MIDI DOWN when the
  key is released, then let the real MIDI UP through
- If MIDI+TRACK or other non-YES chord is performed, dispatch the deferred
  MIDI DOWN to original stock KEYDISP *before* allowing the other key
  through; allow MIDI UP to pass normally
- FUNC+BANK, PTN+TRACK, FUNC+TRACK and normal TRACK keys pass untouched
- Do not create any duplicate or fake track-Mute database; the original
  firmware controls the Mute/LEDs/project/pattern storage

The original stock KEYDISP location 0x40016fc0 is grounded in
elekloader/core-dn1 4.1.1 OS1.44, and a native test double intercepts
the direct stock call. Real hardware MUTE and MIDI-alone delayed key
handling remain to be verified.

## Random EUREKA animations: 2.0 includes 1.9 changes

The same 576 unmodified 128x64 black/yellow HYBRID04 frames now play as
16 independent 36-frame scenes. Forward and reverse loop within their
current scene; scene selection is random every 8, 12 or 16 sixteenth-note
steps during PLAY, independently of FM voice activity. The note-driven
event switch favors a visibly different random scene 55% of the time
instead of the original 35%. Speed/reverse/stutter and video CC40/65/67/
68/69 are preserved. No new video assets have been added.

## Granular v0.5 unchanged

2.731s stereo 512KiB history, 120-360ms Hann-window 50% overlapping
grains, 240ms default, 80ms WET fade; CC33/34 pitch, CC36 grain size,
CC37 position, CC60 mix.

No main push, no workflow run, no existing approved SYX changes.
The authorized GH76 is still the fallback. A future **explicitly authorized**
manual build must pass native/UBSan tests, Elekloader lint, 576-frame
pixel-perfect checksum, memory budget and on-device real Mute sound test.
