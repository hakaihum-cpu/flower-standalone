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
- Current input/detection status.
- Explicit `iRig Streamer DETECTED / NOT DETECTED` status. Detection checks both JUCE-selectable input names and Android `AudioManager.getDevices(GET_DEVICES_INPUTS)` / `AudioDeviceInfo.getProductName()` so a generic JUCE route name does not hide the physical USB device name.
- Input level meter.
- MIDI CH 1..16.
- CLOCK: Internal / MIDI.
- BPM is shown only for Internal because BAR timing cannot be defined without an internal tempo.
- MIDI clock assumes 24 PPQN and 4/4.

## iRig verification acceptance criterion
On the target Android device with iRig Stream connected, CONFIG must expose enough device/routing information to confirm that the external input is active. The target-device smoke test must confirm that the Android physical-input probe exposes the connected iRig product name, or otherwise records exactly what Android reports. A generic JUCE route alone must never be treated as proof that iRig is connected.