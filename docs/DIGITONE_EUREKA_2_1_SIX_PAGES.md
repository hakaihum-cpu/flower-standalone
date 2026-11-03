# Digitone OS1.44 EUREKA 2.1 — six video parameter pages

Status: **source implementation; no firmware build yet**.
Baseline: user-confirmed working **MC78** (MUTE fixed, 576 HYBRID04
frames, 16 random scenes, Granular v0.5); preserved unchanged in
`feature/digitone-eureka-midi-yes-isolated-mute-20261103`.
New isolated working branch:
`feature/digitone-eureka-six-pages-trig-keys-20261103`.

## Key operations

1. **MIDI+YES**: turn EUREKA video ON/OFF, without sending MIDI DOWN
   into stock's MIDI MUTE bank (MC78 behavior is preserved).
2. While EUREKA video ON, **MIDI+FUNC** (either press order) toggles
   dedicated page-key mode ON/OFF. It does **not** forward the
   reserved MIDI DOWN, and therefore must not change stock MUTE
   audio-track/MIDI-track selection.
3. When page-key mode is ON, press **TRIG / SYN1 / SYN2 / FLTR / AMP /
   LFO** to open a page; press **that same key again** to return to the
   full-screen animation, keeping page-key mode armed for another page.
   Pressing another page key switches directly. MIDI+FUNC again
   disarms the feature and returns to animation.
4. During EUREKA panels, encoders **A–D** control four real video
   parameters. Encoders E–H are captured as no-ops solely so they
   cannot silently alter stock FM parameters under the custom page.
   In animation view, all 8 encoders pass through normally.
5. **TRIG step keys 1–16, OS key codes 26–41:** full press, hold,
   release and FUNC combinations pass directly to stock Digitone
   regardless of whether the EUREKA video or parameter panel is shown.
   This does not remap the pads or invent note-on messages; it preserves
   their actual stock behavior and project/sequencer meaning.
6. With EUREKA OFF or page-key mode OFF, the six page buttons and
   encoders continue to work as their normal stock counterparts.

## Editable video parameters (all have implemented effects)

| Page | Encoder A | Encoder B | Encoder C | Encoder D |
|---|---|---|---|---|
| TRIG | SCNE: choose scene 0–15 | JUMP: note-driven random scene probability 0–100% (default 55%) | AUTO: timed scene cuts 0/1 (default 1) | CUT: 0 random 2–4 beats, 1=2 beats, 2=3 beats, 3=4 beats |
| SYN1 | SPED: same video speed as CC67 | REVR: scene-local reverse | STUT: same video stutter as CC65 | SCRB: same frame scrub as CC68 |
| SYN2 | LOOP: 0=12 frames, 1=24, 2=36 (default 2) | NOTE: note video effect probability (default 100%) | MODE: 0=Auto, 1=Scene cut, 2=Stutter, 3=Speed, 4=Reverse | SEED: reproducible pseudo-random seed/sequence |
| FLTR | GLCH: same horizontal glitch as CC40 | INVT: same image inversion as CC69 | XOFF: horizontal pixels -8 to +8 (default 0) | YOFF: vertical pixels -8 to +8 (default 0) |
| AMP | DIM: on-pixel density 0–127 (default 127 / original) | WIDE: expand yellow pixels by 0/1/2 pixels | BLNK: screen blink rate (default OFF) | PULS: pulse/brightness-dither depth (default OFF) |
| LFO | RATE: 0–127 | DEPT: motion/inversion depth 0–8 (default 0) | DEST: 0=X motion, 1=Y motion, 2=invert | WAVE: 0=triangle, 1=square |

When all controls are at default, the original user-supplied
black/yellow 128×64 EUREKA source pixels are unchanged.
No added images or AI art. MIDI CC40/65/67/68/69 update the same
stored effect state for their matching parameters, and the panel
reads their current effective values.

## Regression contract (for new separately authorized build)

- Real MUTE on internal sequencer: MUTE before EUREKA ON, MUTE while
  video ON, unmute and re-mute T1–T4; green LEDs must correspond to
  the audible synth tracks, whether panel is shown or not
- MIDI+YES and MIDI+FUNC must not accidentally select MIDI-track MUTE;
  MIDI alone and MIDI+TRACK must still work normally
- All 16 physical TRIG keys must actually operate the sequencer as
  they do on stock OS, both in the animation view and parameter pages;
  this cannot be proven by checking the key callback alone
- Six page buttons each open their dedicated values, pressing the
  same button again returns to animation, A-D are effective, E-H do
  not affect hidden stock synth settings
- All 576 EUREKA frames reconstruct pixel-exactly; original video
  effect CC controls, random scenes, and Granular 0.5 stay operational
- Run 3-pass preflight, native/UBSan regression, Elekloader lint,
  SHA verification, then *physical instrument test*. Do not call
  native key-passthrough proof of real audible 1–16 operation.

Only the feature branch is changed. No main push or automatic CI build.
