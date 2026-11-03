# Digitone OS1.44 — EUREKA 04 HYBRID + GRANULAR + VIDEO MIDI

Based strictly on user-confirmed working HG76 (EUREKA 576 HYBRID 04 and
GRANULAR-only). This is a NEW trial combining the three approved items:
(1) stock MUTE passthrough while animation continues,
(2) GRANULAR MIDI, and (3) EUREKA VIDEO MIDI.

This trial is not the user-confirmed HG76 master until the owner tests it.
No stock Digitone audio parameter, Delay/Reverb, file or MAIN branch is changed.
One **user-approved** GitHub Actions firmware build, with mandatory native
preflight and elekloader lint, is permitted. Rebuild/retry requires permission.

## 1: Stock MUTE while EUREKA remains visible

The EUREKA ev_key handler returns 0 for stock FUNC combinations and every
TRACK key, so the firmware itself processes MUTE. **No separate MUTE screen**
is added, no mute state is written by the mod, and EUREKA does not hook
ev_key_leds or change TRACK LEDs.

- FUNC+BANK: Global/Pattern MUTE mode through stock firmware.
- FUNC+double BANK: toggle Global/Pattern mode, unchanged.
- T1..T4: stock track MUTE toggle while in MUTE mode.
- FUNC+T1..T4: quick Global MUTE.
- PTN+T1..T4: quick Pattern MUTE.
- MIDI: stock MIDI track selection during MUTE.
- MIDI+YES: EUREKA on/off, as in HG76; FUNC+YES is never stolen.

## 2: GRANULAR MIDI (AUTO channel, while GRANULAR enabled)

| CC | Parameter | Mapping |
|--|--|--|
| 33 | PITCH A | 0-15=-12; 16-31=-7; 32-47=-5; 48-63=0; 64-79=+5; 80-95=+7; 96-111=+12; 112-127=+24 semitones |
| 34 | PITCH B | same eight 16-value steps |
| 36 | GRAIN SIZE | 40–140 ms in 2 ms increments; CC 76=100 ms |
| 37 | POSITION | add 0–3000 input-history frames, corresponding to +0–62.5 ms |
| 60 | MIX | 0=100% dry; 127=100% granular wet |

Defaults at boot: +5/+12 semitones, 100 ms, 0 offset, MIX=64.
Existing grains finish before pitch/size/position take effect.
Two pitched voices, two overlapped grains per voice, 128KiB ring,
30,000-frame clean startup, Q15 audio, Q1.31 stereo, fixed-point DSP,
no added Delay/Reverb.

## 3: EUREKA VIDEO MIDI (AUTO channel, while EUREKA enabled)

| CC | Control | Mapping |
|--|--|--|
| 40 | GLITCH | 0=OFF; 1–127 increases max row-slice horizontal shift to 8 px |
| 65 | STUTTER | 0=OFF; 1–127 loops 2–8 frames within the current 36-frame clip |
| 67 | SPEED | 0–15 freeze; 16–47 half; 48–79 auto/note-response (64 default); 80–111 double; 112–127 reverse |
| 68 | SCRUB | 0=OFF/auto; 1–127 holds frame positions 0–575; usable when STOPPED |
| 69 | INVERT | 0=OFF; 1–126 variable 4-row stripe inversion; 127 full image inversion |

EUREKA uses **the exact original HYBRID04 576 source frames at 128x64**.
Effects are applied only to the rendered 1-bit pixels or playback position,
without changing source assets or audio. MIDI callbacks only store scalar
controls; all image decompress/seeks happen in UI task, not MIDI/audio task.
One row-slice horizontal shift or inversion can never access outside the
128x64 buffer. No screen patching, DMA or physical LED hooks.

CC41/42/43/44 deliberately **NOT** used: Digitone stock OS MIDI manual
maps them to native AMP/CHORUS and FILTER parameters. CC65/67/68/69 are not
assigned in the Digitone parameter implementation table, but they are
standard General MIDI pedal/legato CCs: direct them only to the Digitone
AUTO channel, not simultaneously to other instruments.

Safety caveat: this new version must be tested on the user's hardware;
the original HG76 stays intact until separately approved.
