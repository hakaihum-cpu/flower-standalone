# RECORDER v0.1 scope

- Independent Android app. Do not modify or merge MIYAKO/FLOWER/RealtimeChordFX product state.
- 3x3 = 9 audio slots.
- REC records continuously and closes the current slot every 5 seconds, then advances to the next slot.
- After slot 9, recording wraps to slot 1 and overwrites oldest content.
- Each tile displays its recorded waveform.
- Tapping a populated tile plays that slot from the beginning.
- RANDOM mode chooses among populated slots. A new random slot is started on a clock beat only when no slot is currently playing; it does not chop an already playing five-second sample.
- Clock source: INTERNAL or MIDI clock (24 PPQN).
- INTERNAL BPM range: 30-300.
- UI repaint target: 20 Hz for waveform/playhead motion.
- No pitch, reverse, FX, stretch, synth, character animation, or extra sequencing in v0.1.
- GitHub Actions must not be used. APK build is CircleCI manual_android_build with run_build=true.
- main and all existing Golden branches remain unchanged until explicit user instruction.
