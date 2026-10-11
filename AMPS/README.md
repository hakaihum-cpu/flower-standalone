# AMPS — AC30-style combo amplifier trial (AN-68)

Standalone Android guitar-amp effects processor for RG Rotate (720×720 reference).
This is an original AC30-inspired design, not a measured/convolutional reproduction
of a VOX amplifier and not endorsed by the brand. Artwork is drawn directly by
Android Canvas in amber-on-black DOT style; no generated images.

Signal: selected AUDIO IN -> DC cut -> NORMAL/TOP BOOST filters ->
asymmetric preamp with interpolated 2x drive -> output-stage soft limiting ->
tone cut and open-back speaker response -> tremolo -> spring-style reverb ->
output limiter -> selected AUDIO OUT (stereo dual mono).

Controls: input gain, channel volume, bass, treble, cut, master, tremolo
speed/depth, spring reverb, two cabinet voicings, bypass, presets (factory 1-5
plus long-press save), device enumeration and RESCAN, buffer 0.5–8 BURSTS.
AUDIO IN/OUT selection uses Android actual AudioDeviceInfo and stream
setPreferredDevice, with observed routed IDs reported. Unlike native AAudio,
the Java AudioRecord/AudioTrack trial cannot promise exclusive mode, exact
hardware burst sizing, or particular end-to-end latency. Actual buffer frames,
route IDs and output underruns must be inspected on real hardware; any
unavailable requested device causes a STOP/error, not a fake connection.

## Manual build (existing CircleCI project)
- Branch: feature/amps-ac30-combo-mvp
- Workflow: manual_android_build / job android_build
- Pipeline parameter: run_build=true (default false)
- Gradle module: AMPS/:app
- Artifact: AMPS-Android.apk plus diagnostic SOURCE_COMMIT.txt and SHA256.txt
- Do not modify main/Golden or use GitHub Actions.

AUDIO permission must be granted at first launch. Headphone/USB interface
strongly recommended to prevent acoustic feedback. Route/burst changes restart
the audio stream; ordinary DSP parameter changes do not.
