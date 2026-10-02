# UI Specification

Design space: 720 x 720 logical square, scaled to actual display bounds.

## Main screen
- CHORD visual baseline is exactly the previously approved `golden/chordfx-android-2026-10-02` main screen. Do not add CHORD-A/B labels, X/Y readout, or Motion indicators to the CHORD main screen.
- Fullscreen current supplied EFFECTS frame.
- No decorative knobs.
- Bottom readability gradient only.
- Small `CONFIG` text at top-right.
- Running only: 3px red border inset 8px and `● REC` at top-left.
- CHORD-A and CHORD-B share the original CHORD screen: IN/CHORD readout plus COMPLEX/BAR/WIDTH/LENGTH controls.
- DREAMY mode keeps the XY-focused EFFECTS screen.

## Touch
- CHORD-A / CHORD-B: original CHORD interaction is restored. Bottom four parameter cells drag horizontally; with MIDI CONTROL OFF the remaining image/background toggles REC; with MIDI CONTROL ON the image is XY and top-left is REC.
- DREAMY mode: top-left is REC and the remaining image is Dreamy XY.
- CONFIG label opens config.
- Parameter interaction never toggles REC.

## CONFIG screen
Same current supplied frame behind a dark translucent layer.
- MODE row cycles CHORD-A / CHORD-B / DREAMY, default CHORD-A. Legacy effectMode remains CHORD/DREAMY internally so existing DREAMY saved states keep their meaning.
- Audio input names as plain text/radio marks.
- iRig detection text plus the Android physical input product name when available.
- Thin input meter.
- MIDI CH text row.
- CLOCK text row.
- BPM row only in Internal mode.
- CLOSE text; no ornamental panel/knob styling.

## MIDI controller mode
When MIDI CONTROL is ON:
- The main image field is the XY surface.
- Top-left becomes the dedicated REC toggle so XY touches never toggle REC.
- A small X/Y numeric readout is shown; no decorative panel is added.
- CONFIG -> MIDI SETTINGS opens a second text-only configuration page.
- Rows: ENABLE, MIDI OUT, MIDI CH, X MODE, X CC, Y MODE, Y CC, KEY, SCALE, PRESET, LOAD, SAVE.
- X/Y modes: CC / NOTE / CLOCK.
- Coordinate-to-frame mapping is 20 columns x 15 rows over the exact supplied 01..300 frames.


## Motion REC controls
- Physical L1: record / stop-and-play.
- Physical R1: clear.
- MIDI SETTINGS includes MOTION BARS 1..16.
- Main screen shows MOTION REC while recording and MOTION PLAY while looping.
- Holding L1/R1 must not retrigger repeatedly; actions occur on physical press transitions.


## Chord MIDI output controls
- MIDI SETTINGS adds CHORD OUT and CHORD CH.
- CHORD OUT default is OFF.
- CHORD CH is 1..16 and independent from the XY MIDI CH.
- MIDI OUT remains the common destination device for XY/Motion and generated-chord MIDI.
- No new panel or decorative control is introduced; the existing text-row style is retained.


## EFFECTS shared surface
- Keep the RealtimeChordFX/EFFECTS 300-frame visual surface. Do not import FLOWER screen layout, 100-frame UI, CONFIG, ARP, Delay, or Granular controls.
- Top-left remains REC start/stop.
- Main image area outside controls is the shared XY surface in both CHORD and DREAMY.
- X/Y readout remains on the EFFECTS screen.
- DREAMY uses the XY values for its original Dreamy mapping plus the new upper-right ambience depth.
- With MIDI CONTROL ON, the same XY values are also sent by the MIDI controller mode.
- CONFIG keeps the EFFECTS style and iRig PEAK/RMS/NONZERO diagnostics.
- DREAMY must not show the CHORD IN/CHORD/COMPLEX/BAR/WIDTH/LENGTH block; both CHORD modes do show it.

## CHORD audio modes
- TheoryEngine is shared by CHORD-A and CHORD-B.
- Generated ChordPlan notes are constrained to MIDI 60..83 (C4..B5).
- The generated ChordPlan must contain the live pitch class.
- Dry live input remains audible as the performance anchor.
- CHORD-A captures a real recent input phrase and resamples that same phrase simultaneously at the ChordPlan target pitches. HOLD=0 preserves the approved minimum of 240 ms capture / 20 ms seam crossfade. HOLD can extend them together up to 2000 ms / 160 ms; it never makes either value shorter than the approved minimum. HOLD is CHORD-A only. The final HOLD value is committed on touch release with one recapture, avoiding repeated capture work during a drag. It must not reduce the source to one/two pitch periods, PSOLA grains, or a micro-loop oscillator. BAR progression reuses the captured phrase rather than recapturing silence.
- CHORD-B uses the same ChordPlan but renders random constituent notes with a sine-wave arpeggiator at an eighth-note step.
- CHORD-A and CHORD-B both pass through the shared fixed reverb stage.
- TD-PSOLA is not on the audible CHORD-A/B path.
- No legacy fixed-grain GranularPitchBank is compiled into the EFFECTS app.

## DREAMY audio mode
- XY values latch at the last touched position when the finger is released; Dreamy continues using that X/Y until the next touch.
- DREAMY shows a 10-step REVERB indicator derived from the same X*Y ambience mapping.
- Dreamy history is fed continuously from the raw live input even while CHORD is selected, matching the accepted standalone effect behaviour; only the wet layer is gated by DREAMY selection.
- Uses the accepted FLOWER Master micro-loop core, not the old chord pitch shifter.
- Wet micro-loop output is low-pass smoothed to reduce high-frequency fizz.
- The original Dreamy X/Y mapping remains active.
- Additional delay/reverb depth is derived from X*Y; top-right is strongest.


## DREAMY processing boundary
- FLOWER Master Dreamy core is unchanged.
- No low-pass or other processing is inserted inside the Dreamy core.
- EFFECTS adds only a post-Dreamy Delay/Reverb stage.
- Post Delay/Reverb depth is driven by X*Y and is strongest at the upper-right.
