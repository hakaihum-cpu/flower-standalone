# Digitone VM76: stock MUTE investigation + GRANULAR v0.5 long-Hann revision

Date: 2026-11-03. Golden/reference: **user-tested VM76 binary**, based
on HG76 04 HYBRID EUREKA video. Source branch:
`feature/digitone-eureka-mute-granular-video-cc-20261103`.
This file and the new GRANULAR changes live ONLY on
`feature/digitone-mute-audit-granular-long-hann-20261103`.
Do not change main, either former branch, or trigger GitHub Actions
without a NEW explicit user build authorization.

## 1 — MUTE: confirmed user observation, NOT FIXED

The Digitone mk1 runs its **internal sequencer**. FUNC+BANK opens
**GLOBAL MUTE** with **green LEDs**; pressing the TRACK 1-4 keys makes
their LEDs extinguish/light again, but **the internal sequencer
continues sounding those tracks**. This happens with GRANULAR ON
and with GRANULAR OFF. Crucial additional user's A/B check:
**EUREKA video OFF => stock MUTE audio works**;
**EUREKA video ON => LED MUTE state changes but the internal sequencer
continues sounding**. Both were tested on the same instrument. Thus
the failing condition is EUREKA being enabled, not the granular insert.

Firmware / public source review:

- `EUREKA eureka576_key()`: every TRACK key and FUNC combination returns
  0 and therefore passes through to `core_dn_key` then stock `KEYDISP`.
  In VM76 v1.7 the MIDI+YES **YES down** was consumed (return 1)
  but the corresponding **YES up** was passed to the stock dispatcher
  (return 0). This creates a mismatched key-event pair in the
  stock key handler and is a concrete source-level defect candidate.
- The **experimental EUREKA v1.8** corrects this asymmetry: after
  capturing MIDI+YES's YES DOWN, it captures that YES UP too, even if
  the MIDI key was released first. The latching flag is cleared on
  YES UP. Other YES actions and all stock FUNC/MUTE events still pass
  through. Native regression test cases have been added, but NOT
  executed. This repair is plausible, **not a demonstrated MUTE fix**.
- EUREKA `ev_draw` overlays 128x64 image; it does not write the track
  mute state, change the voice audio buffers or write LED status.
- `core-dn1/menu.s` tracks key-down bits, handles long TRACK holds and
  otherwise forwards keys to `core_key` and the stock key dispatcher.
- `core-dn1/leds.s` passes LED rendering to stock with optional
  `ev_key_leds` callbacks. EUREKA does not register such a callback.
- `GRANULAR` uses `CORE_AUDIO_INSERT` at the mixed master output and
  has no stock track mute bypass; it can be disabled. The user's
  **GRANULAR OFF** result excludes Granular's *wet playback* as the
  explanation for continuing new notes.

**Consequence**: a native test asserting only that key callbacks return
0 cannot prove MUTE actually silences any sequencer events. The former
VM76 test's PASS MUTE claim was too broad. No evidence currently
identifies the exact state/voice-routing failure in OS1.44+core.
The EUREKA draw hook paints over the full 128x64 display while enabled.
That hides the stock MUTE page but cannot itself establish why sequencer
audio continues after the LEDs go dark. Do not interpret a hidden
LCD as a proven audio-mute causation.

Do NOT randomly patch stock MUTE flags, spoof the LEDs, or intercept
`ev_note_queue` to mutate note events: its published event interface is
read-only and stock global/pattern mute state and pattern persistence
are NOT exposed by the verified core API.

**Needed for a future verified fix**: confirm via emulator or instrumented
OS1.44 the actual stock global/pattern MUTE state on TRACK key press,
the point where that state gates the sequencer's sound trigs, and
whether the core's hook chain affects it. Compare same pattern on
stock OS1.44, core-only and original HG76, without shipping unverified
mute-flags hacks. A successful MUTE regression must *audibly suppress
fresh internal sequencer note triggers* across a pattern wrap, not
merely change the LEDs; test Mute on/off and four tracks.

## 2 — GRANULAR v0.5 + EUREKA v1.8: implemented SOURCE, no SYX built

| Property | VM76 v0.4 | Trial v0.5 |
|---|---:|---:|
| Stereo capture ring @48k | 32768 frames = 0.683s | 131072 frames = **2.731s** |
| Ring RAM | 128KiB | **512KiB** |
| Grain size CC36 | 40-140ms, 2ms steps | **120-360ms, 5ms steps** |
| Grain size default | 100ms | **240ms**, CC63 or CC64 |
| Crossfade envelope | Triangle | **256-point Hann Q15** |
| Two pitch voices | +5/+12 by default | **unchanged** |
| 50% grain overlap | 2 grains per voice | **unchanged** |
| Wet level | 1/4 of four contributions | 1/2 of four contributions (two voice average) |
| Initial wet behavior | 0 until armed | Full dry until armed; 80ms wet fade |
| Arm after fresh history | 30000 frames (0.625s) | 90000 frames (1.875s) |
| Grain source lag | 21000 / 23000 frames | 70000 / 76000 frames |
| POSITION CC37 | 0 to +62.5ms | unchanged |
| Pitch CC33/34 | -12/-7/-5/0/+5/+7/+12/+24 | unchanged |
| MIX CC60 | 0..127 | unchanged |

The 512KiB ring increases RAM use by 393216 bytes. Relative to
VM76 linked RAM use 490464/1310720, expected v0.5 RAM use is
approximately 883680/1310720 with ~427040 bytes spare, excluding
small code and state growth. Confirm with actual Elekloader lint
on any newly authorized build.

The Q20.12 source interpolation is necessary: +24 semitones is a 4x
read speed. The earlier Q16.16 product `17280*262144` overflows a
32-bit accumulator at 360ms; v0.5 uses steps max 16384 in Q20.12,
which remains below 2^32. At 360ms the +24 read head approaches
the advancing write pointer by at most 3*17280=51840 frames,
strictly less than lag A 70000. At -12 with maximum 62.5ms POSITION,
the oldest referenced history is bounded by
76000+3000+(17280/2)=87640, below the 90000-frame arm threshold.

The Hann LUT is designed so opposite 128-index positions sum to
32767. The output keeps the two-pitch/two-overlap design; grain
parameters latch on new grain starts to avoid truncating active
grains. No malloc, calloc, memset of audio ring, cos(), floating-point
or division inside the inner `grain_at()` loop.

## Test coverage and limitations

The updated `tests/test_dreamy.c` exercises:
- pitch CC sweep 0..127, position, all 49 grain sizes and safe
  history bounds at all 8 pitch choices,
- 256 Hann complementary-pair points,
- startup history fill, voice tail, hot CC changes, output bounds,
  disable/reopen and MIX=0 exact dry pass-through.

Tests have **not yet been executed on a host or digitone** in this
branch; a future manual-only workflow contains compiler+UBSan tests,
Elekloader lint, a combined-module link check, and SYX packaging.
Do not claim mute fixed or granular sounding better on hardware
before actual playback verification.


## 3 — Correct regression criteria and build gate

The working branch includes the MIDI+YES press/release symmetry candidate
and Granular v0.5. **No new firmware image has been built or supplied**.
The EUREKA C test verifies correct event consumption and preservation of
other key events, but does not include stock Digitone's actual MUTE
machine or audio sequencer. Therefore static PASS is not sufficient.

On next explicitly user-authorized build, the required on-device checks
include: with the *internal* Digitone sequencer playing, EUREKA ON,
FUNC+BANK Global MUTE green LEDs, each T1-4 genuinely silences its
own new steps; releasing mute resumes them; toggling EUREKA OFF/ON
keeps the mute working; GRANULAR OFF and ON both tested. Confirm that
MIDI+YES still toggles video reliably, and no phantom YES actions
occur in stock UI. If MUTE audio still fails, diagnose exact stock
state before making further guesses.

## 4 — GH76 user A/B: MUTE LED/real sound mismatch, and EUREKA v1.9 random scenes

**GH76 hardware failure confirmed.** In stock-visible mode before MIDI+YES,
audio-track GLOBAL MUTE works. With the synth tracks already muted,
pressing MIDI+YES to enable EUREKA leaves the sound muted but all TRACK
LEDs turn green. Inside EUREKA, TRACK presses change LED brightness,
but do not change the audible synth mute; a video-off return makes
stock audio MUTE work again.

These observations are **not proof of separate firmware mute storage**.
Digitone mk1 OS1.41+ official user manual section 8.5.2 says:
GLOBAL MUTE's unmuted audio tracks are green, muted ones unlit, and
[MIDI] in MUTE mode accesses MIDI tracks. In EUREKA v1.8,
`eureka576_key` records the MIDI DOWN but returns 0, letting stock
`KEYDISP` see the MIDI key press BEFORE YES is intercepted as the
video shortcut. The all-green LEDs with previously muted synths are
consistent with the stock MUTE view switching to unmuted MIDI tracks.
This hypothesis is stronger than the now-disproven orphan YES-UP
hypothesis, but the OS1.44 view state has not been instrumented.
**Do not claim the mute bug fixed by v1.9.**

A robust future fix must preserve MIDI+YES as a chord while avoiding
accidental stock MUTE bank changes; other MIDI-only and MIDI+TRACK
interactions must continue to work. Do not simply discard all MIDI
key events or fabricate a parallel per-track MUTE database. Real
synth-track MUTE in GLOBAL and PATTERN modes must be tested both before,
during and after EUREKA video, including pre-muted tracks.

**User visual finding:** GH76 video appeared sequential, with no
perceivable random scene cuts. The v1.8 code normally advanced through
frames 0..575 in order; random jumps were only 35% of actual
`ev_voice_on` reactions, with no guaranteed cuts in quiet bars.
EUREKA **v1.9 trial** makes the 16 original 36-frame groups separate
local-loop scenes; forward/reverse wrap within the selected group.
When PLAY runs, the UI chooses a **different random group every 8,
12 or 16 sixteenth-note steps** (2/3/4 quarter-note beats) independent
of voice events, and 55% of actual voice events jump to a distinct
random scene. Random scene selection is suppressed while CC68 SCRUB
is active, and playback freeze CC67 remains respected. The original
576 source frames and image pixels, 5 video CCs, MIDI+YES toggling,
Granular v0.5, and stock FM audio paths are unchanged. No new
SYX was built or authorized.

**Testing limitations:** `tests/test_random.c` now checks local
scene loops and independent cuts without voice callbacks, but only
an on-device test can judge whether each 36-frame group looks distinct
and that scene cuts do not disturb the stock UI and timing. No
event handler in v1.9 writes or patches stock MUTE.

## 5 — EUREKA 2.0: implementation targeting MUTE audio/LED mismatch

GH76 still has the confirmed MUTE bug. EUREKA v1.8 paired only YES
down/up but passed the original MIDI key press into stock, which
explains why no mute improvement was heard.

A change is now IMPLEMENTED ON SOURCE in branch
`feature/digitone-eureka-midi-yes-isolated-mute-20261103`:

1. `eureka576_key` consumes the physical MIDI-down, deferring the stock
   MIDI action until the next relevant key event.
2. If YES is pressed next while MIDI remains held, both MIDI events
   and the matched YES events are consumed as an isolated video-toggle
   chord, so the **stock MUTE track group is not switched to MIDI**.
3. If another key is pressed first (including TRACK), a call is made
   to the original OS1.44 KEYDISP at `0x40016fc0` with one simulated
   MIDI DOWN before the actual other key passes stock. This preserves
   the normal MIDI+TRACK modifier use.
4. If only MIDI is pressed, stock receives the delayed MIDI DOWN
   before the actual physical MIDI UP, still permitting MIDI-only
   track-bank navigation.
5. All ordinary TRACK, FUNC+BANK, FUNC+TRACK and PTN+TRACK events
   pass stock unchanged. No firmware MUTE mask is edited; original
   global and pattern mute persistence are unchanged.

The corresponding native tests now verify no stock MIDI DOWN on
the MIDI+YES chord, exactly one delayed MIDI DOWN on solo MIDI and
MIDI+TRACK, and paired YES/MIDI UP even when released in reverse
order. The harness does NOT model actual OS1.44 stock mute sound, so
the change must not be declared hardware verified before a new
authorized SYX build and on-device audio test.

The original 576 HYBRID04 images, 16 scene v1.9 random-cut behavior,
and Granular v0.5 remain unchanged. The branch's workflow remains
manual-only and HAS NOT BEEN STARTED.
