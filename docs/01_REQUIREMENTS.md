# Requirements

## Audio behaviour
1. Accept a monophonic musical input signal.
2. Detect the fundamental pitch in real time.
3. Create generated chord voices by pitch-shifting the input audio; do not synthesize replacement oscillators.
4. Generate theory-guided chord movement with weighted randomness.
5. A new accepted input note immediately re-harmonises the output and resets the progression interval.

## Main parameters
### COMPLEX
0..100. At low values use primarily diatonic triads. Increasing values progressively permits sevenths/add9/sus colours, borrowed chords, secondary-dominant behaviour and, near maximum, altered/substitute dominant colours.

### BAR
Discrete: 1/4, 1/2, 1, 2 bars. Time signature is 4/4. Determines the progression-change interval when LENGTH is not infinite.

### WIDTH
0..100. Controls close vs spread voicing. Generated notes must never be placed below C3 (MIDI 48). High values may spread upper voices across additional octaves.

### LENGTH
0..100 plus effective MAX at the endpoint. Below MAX, controls gate duration within each BAR interval. At MAX the generated chord is held indefinitely and changes only when a new input note is accepted.

## Transport-like interaction
- Tap the background image area while stopped: start generator state and show a thin red screen border plus red `● REC` text.
- Tap the background image area while running: stop and CLEAR generated state.
- Parameter/config hit areas consume touch and must not toggle REC.
- Stopped/CLEAR state passes input through dry for setup/audition; active state outputs generated harmony.

## Visuals
- Use the exact user-supplied 01..300 image frames as the only main visual bank.
- Progress sequentially; wrap after frame 300.
- Frame advances on an accepted generated chord event (new input note or timed chord transition).
- Stop+CLEAR resets to frame 1.

## CONFIG
- Audio input list/select.
- On RG Rotate, JUCE-selectable input names containing `RG Rotate` are excluded from the CONFIG input list before the six-row display limit. This does not affect Android physical-device detection or other USB/external inputs.
- Current input/detection status.
- Explicit `iRig Streamer DETECTED / NOT DETECTED` status. Detection checks both JUCE-selectable input names and Android `AudioManager.getDevices(GET_DEVICES_INPUTS)` / `AudioDeviceInfo.getProductName()` so a generic JUCE route name does not hide the physical USB device name.
- Input level meter.
- CONFIG diagnostics show numeric input level in dBFS, actual JUCE input route, actual output route, active input-channel count, sample rate, and the most recent route-open error. This diagnostic display must not change routing by itself.
- MIDI CH 1..16.
- CLOCK: Internal / MIDI.
- BPM is shown only for Internal because BAR timing cannot be defined without an internal tempo.
- MIDI clock assumes 24 PPQN and 4/4.

## iRig verification acceptance criterion
On the target Android device with iRig Stream connected, CONFIG must expose enough device/routing information to confirm that the external input is active. The target-device smoke test must confirm that the Android physical-input probe exposes the connected iRig product name, or otherwise records exactly what Android reports. A generic JUCE route alone must never be treated as proof that iRig is connected.

## MIDI controller mode
- CONFIG includes MIDI CONTROL ON/OFF; default is OFF.
- With MIDI CONTROL OFF, existing REC/touch/frame behaviour remains unchanged.
- With MIDI CONTROL ON, the main image area becomes an XY MIDI controller.
- X is 0 at the left edge and 127 at the right edge.
- Y is 0 at the bottom edge and 127 at the top edge; the top-right corner is X=127 / Y=127.
- X and Y can each be assigned CC, NOTE or CLOCK.
- CC sends the axis value 0..127 with an independently selectable CC number.
- NOTE maps the axis value to a MIDI note quantised to the selected KEY/SCALE. Note-off is sent on touch release or when NOTE/controller mode is disabled.
- NOTE scales: Chromatic, Major, Natural Minor, Major Pentatonic, Minor Pentatonic.
- CLOCK maps the selected axis 0..127 to 40..240 BPM and emits 24 PPQN MIDI Clock. Only one axis owns CLOCK at a time.
- MIDI CH applies to CC/NOTE; MIDI Clock is channel-less.
- MIDI SETTINGS includes explicit MIDI OUT selection for standalone routing.
- XY selects the exact 300-frame visual bank as a 20 x 15 grid. Top-right selects frame 300.
- Eight internal preset slots SAVE/LOAD MIDI CH, X/Y modes, CC numbers, KEY/SCALE and XY position.
- Preset LOAD does not change MIDI CONTROL enable state or MIDI OUT device.


## XY Motion REC
- Available when MIDI CONTROL is ON.
- Android physical L1 uses the existing key bridge mapping KEYCODE_BUTTON_L1 -> F17.
- L1 while stopped/playing starts a new XY recording and replaces the previous motion.
- L1 while recording stops immediately and starts loop playback.
- If recording reaches the configured MOTION BARS length, it automatically stops and starts loop playback.
- Android physical R1 uses KEYCODE_BUTTON_R1 -> F18 and clears/stops the motion.
- MOTION BARS is configured in MIDI SETTINGS, integer 1..16, default 1.
- Motion resolution is 24 PPQN / 4/4 = 96 XY samples per bar.
- Internal CLOCK derives the motion tick from BPM. MIDI CLOCK mode advances motion from received MIDI clock ticks.
- Playback replays both X and Y, including the associated 300-frame XY visual selection and MIDI controller output.
- Motion data itself is temporary and is not stored in controller presets; MOTION BARS is normal saved parameter state.


## Chord generator MIDI output
- CONFIG -> MIDI SETTINGS includes CHORD OUT ON/OFF; default is OFF.
- CHORD CH is independently selectable from 1..16.
- The physical MIDI OUT device is shared with the existing MIDI controller output selection.
- When enabled, every generated ChordPlan.midiNotes voicing is emitted as MIDI Note On messages on CHORD CH.
- On chord changes, active generated notes are sent Note Off before the new generated chord is sent Note On.
- When LENGTH closes the generated-audio gate, the active MIDI chord is also sent Note Off.
- REC stop/CLEAR sends Note Off for all active generated chord notes.
- Switching CHORD OUT OFF sends Note Off for all active generated chord notes.
- Changing CHORD CH while a generated chord is active sends Note Off on the previous channel before re-triggering the same current chord on the new channel.
- CHORD OUT is independent of MIDI CONTROL. The generator can send chord MIDI while XY MIDI CONTROL is OFF.
- CHORD OUT and CHORD CH are normal saved parameters; they are not part of the eight XY-controller preset slots.


## Harmony pitch-quality constraints
- Generated audio voicings must be placed around the current accepted input-note register instead of a fixed C4/C5 register.
- The existing generated-note hard floor at C3 (MIDI 48) remains.
- Avoid unnecessary two-octave upward shifts that drive the pitch shifter into extreme ratios.
- The legacy fixed-grain overlap/resample shifter is not accepted: a 220 Hz sine at ratio 2.0 must produce 440 Hz as the dominant component rather than the former ~408/502 Hz sidebands.
- Pitch shifting uses an internal 1024-point, hop-256 phase-vocoder analysis/synthesis path. The analysis FFT is shared by all harmony voices.
- No new external audio library or JUCE-only dependency is introduced into the core pitch-shifter source; the existing dependency-free C++ preflight remains valid.
- Per-voice pitch ratio is constrained to 0.5..2.5 as a safety bound.
- Multi-voice output is averaged by active voice count to reduce clipping/flattened transients.
- A single unstable pitch-detector estimate must not immediately retune all active harmony voices. Accepted-note bend/vibrato may follow with smoothing.
