# Digitone OS1.44 — EUREKA 2.1 + GRANULAR 0.5

**SOURCE-ONLY trial. No firmware build has been authorized or run for
this branch.** The working user-tested **MC78** firmware remains unchanged
on branch `feature/digitone-eureka-midi-yes-isolated-mute-20261103`.

The MC78 user-confirmed correct audio-track MUTE behavior is preserved:
the MIDI+YES shortcut must NOT forward MIDI DOWN to the stock Digitone
MIDI-track mute bank. Its ordinary MIDI key, MIDI+TRACK, FUNC+BANK,
TRACK LED and sequencer routines have not been replaced or overridden.

## EUREKA six dedicated video pages

MIDI+FUNC while video is ON toggles page-key mode. TRIG, SYN1, SYN2,
FLTR, AMP and LFO each open a separate visual parameter page, and the
same key pressed again returns to the full-screen 576-frame animation.
The other five page buttons open their pages directly. MIDI+FUNC again
disables the custom page-key mode and returns to video.

A–D encoders each adjust a real setting on every page, using the stock
Digitone 4 encoder counts/detent. E–H are protected from silently
editing stock FM parameters behind a custom page. Video CC40,65,67,68,69
still act on the same video effects. All 16 TRIG pad key events 26–41
continue through to the stock sequencer and keyboard functionality in
either view. This **does not create synthetic note events or guarantee
a hardware pad behavior not yet tested**.

The 24 control labels, values, defaults and behavior are listed in
[the full six-page specification](../../docs/DIGITONE_EUREKA_2_1_SIX_PAGES.md).
Original 576 HYBRID04 pixels remain bit-exact at default video settings;
random 16-scene playback and note-driven cuts are retained.

## GRANULAR v0.5 unmodified

2.731s 512KiB stereo capture ring, 120–360ms Hann grains (240ms default),
80ms wet fade, and original CC33/34/36/37/60 controls. FM synthesizer,
stock Delay and Reverb are unchanged.

The new branch's GitHub Actions workflow is **workflow_dispatch only**
and has not been run. Before a future authorized build, require native
video/parameter/16-key/MIDI+FUNC tests, UBSan for audio/video, 576-frame
source comparison, Elekloader link/lint and SYX SHA verification.
On-device audio MUTE and each stock pad's function must be confirmed
rather than inferred from native hook-return values.
