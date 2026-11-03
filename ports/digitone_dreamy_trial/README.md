# Digitone OS 1.44 — EUREKA 04 HYBRID + GRANULAR MIDI v0.3

Experimental **MIDI parameter extension**, based on the user-tested
HG76 firmware (EUREKA 576 frames + GRANULAR v0.2). The HG76 audio mod
has been confirmed operational by the owner on real Digitone hardware;
this NEW v0.3 MIDI extension is **source-only and not yet built or tested
on hardware**. Never replace the verified HG76 baseline automatically.

- Display: the **same approved 04 HYBRID** conversion of 576 original
  user-video frames, 128x64 yellow/black, with note-reactive jump,
  reversal, speed and stutter. EUREKA's code and images are unchanged.
- Mod Menu **GRANULAR** toggles the audio insert.
- Two pitch voices, each with two overlapping triangular-window stereo
  grains. No added delay/reverb/feedback. Built-in Digitone FM and
  FX remain enabled, upstream of the granular insert.

## MIDI implementation (AUTO CHANNEL only, when GRANULAR is ON)

| CC | Parameter | Value mapping |
|---|---|---|
| 33 | PITCH A | 0-15: -12; 16-31: -7; 32-47: -5; 48-63: 0; 64-79: +5; 80-95: +7; 96-111: +12; 112-127: +24 semitones |
| 34 | PITCH B | Same eight ranges as PITCH A |
| 36 | GRAIN SIZE | 40-140 ms, 2 ms quantization |
| 60 | MIX | 0 = dry, 127 = wet |

Defaults: **PITCH A +5**, **PITCH B +12**, **GRAIN SIZE 100 ms**
(4800 frames at 48 kHz, also obtained from CC36 = 76), **MIX 64**.

- A/B pitches and grain size are **latched only at each grain's next
  reset** (zero-width envelope edge), not in the middle of a grain.
  This reduces parameter-change discontinuities without spawning
  additional grains or causing large memory operations.
- Audio continues using 32 stereo frames per render, Q1.31 input/output,
  Q15 internal audio and one 128 KiB history ring.
- For worst case +24 semitones, the 140 ms grain consumes 26880 input
  samples in 6720 output frames, compared with 21000 or 23000 samples
  of ring-history lag; the advancing write head prevents future reads.
  The maximum catch-up is (4-1)*6720 = 20160 < 21000.
- For worst case -12 semitones, the most distant read is
  23000 + (1-0.5)*6720 = 26360 < 27000 fresh history frames.
  Arming occurs after 27000 samples (~563 ms). No stale buffer data.
- CC35, CC37, CC61 and CC62 are **not intercepted**.
- The stock Digitone Delay and Reverb process the sound before GRANULAR,
  not the post-granular signal.

## Safety and next build

The base HG76 real-device result is positive, but **v0.3 has not yet
passed host native/UBSan tests, Elekloader lint, build, or hardware testing**.
The native test source checks CC quantization across 0-127, +24/-12
extreme behavior, changing grain lengths, and dry/wet.

The CI stays **workflow_dispatch only**, without automatic push
builds. An explicit user build authorization is required. Main and
the previously verified HG76 branch are not touched.
