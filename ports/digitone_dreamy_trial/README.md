# Digitone EUREKA RG DOT + DREAMY (experimental)

This is a **trial** port of the existing EFFECTS DREAMY idea, not a bit-exact port of the JUCE algorithm.

- Hardware: Digitone mk1 / Digitone Keys on official OS 1.44.
- Base: existing `eureka576` 576-frame EUREKA video, source composition unchanged, Bayer 4x4 128x64 1-bit LCD.
- Mod Menu > DREAMY: toggle the insert on/off. The original Digitone FM voices and the stock effects are **not muted**.
- Auto-channel MIDI CC 60: dry/wet mix (default 64/127); CC 61: delay time 160–520 ms (default 75); CC 62: room tail level (default 72).
- DREAMY: two overlapping stereo pitch-shifted grains at +5/+12 semitones, a damped stereo cross-feedback delay, and three recirculating damped ambience lines.
- Software path: stock audio master -> `CORE_AUDIO_INSERT` -> DREAMY -> outputs.
- Audio: 48 kHz stereo, 32-frame Q1.31 blocks; internal Q15 DSP. Stateful buffers are statically allocated and reset in bounded segments **outside** the audio callback.
- No added third-party samples or generated visual images. The existing EUREKA image bank and playback controls are untouched.

## Limits and safety

The stereo ambience lines approximate a spacious reverb; they do **not** reproduce JUCE Reverb's internal algorithm. Parameter smoothing, detailed independent controls, and automatic source switching are not in this trial. The output uses a soft saturation knee to constrain peaks, which may affect very loud material. This is **not yet validated on real hardware**; test first at a low monitoring level. Do not treat static checks or a valid SYX as proof of audio quality or boot safety.

The workflow is inherited GitHub Actions, with prebuild native tests and a combined Elekloader lint/patch check. The user-approved one-time run is manually gated. No automatic pushes to main or changes to the approved baseline.
