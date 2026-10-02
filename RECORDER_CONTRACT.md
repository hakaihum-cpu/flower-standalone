# RECORDER v0.1 scope

- Independent Android app. Existing products and their Golden baselines are out of scope.
- 3x3 = 9 audio slots.
- REC records continuously and closes the current slot every 5 seconds, then advances to the next slot.
- When slot 9 reaches 5 seconds, recording stops automatically and the app enters PLAY mode.
- Each tile displays its recorded waveform.
- Tapping a populated tile plays that slot from the beginning. Multiple different tiles can play at the same time.
- RANDOM mode chooses among populated slots. A new random slot is started on a clock beat only when no slot is currently playing; it does not chop an already playing five-second sample.
- Clock source: INTERNAL or MIDI clock (24 PPQN).
- INTERNAL BPM range: 30-300.
- UI repaint target: 20 Hz for waveform/playhead motion.
- No pitch, reverse, FX, stretch, character animation, or extra sequencing in v0.1.
- GitHub Actions must not be used. APK build is CircleCI manual_android_build with run_build=true.
- main and all existing Golden branches remain unchanged until explicit user instruction.
