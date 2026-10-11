# AMPS — AC30-style combo amplifier trial (AN-68)

Standalone Android guitar-amp effects processor for RG Rotate (720×720 reference).
This is an original AC30-inspired design, not a measured/convolutional reproduction
of a VOX amplifier and not endorsed by the brand. Artwork is drawn directly by
Android Canvas in amber-on-black DOT style; no generated images.

Signal: Android-selected INPUT -> native low-latency AAudio INPUT callback
-> SPSC ring -> native low-latency AAudio OUTPUT callback -> AC30-inspired
preamp/cabinet DSP -> tremolo -> spring reverb -> selected OUTPUT.
Input PCM16 mono; output float stereo, native output hardware sample rate.
The audio callbacks neither allocate memory nor call blocking read/write.

Controls: input gain, channel volume, bass, treble, cut, master, tremolo
speed/depth, spring reverb, two cabinet voicings, bypass, presets (factory 1-5
plus long-press save), device enumeration and RESCAN, buffer 0.5–8 BURSTS.
AUDIO IN/OUT uses OS AudioDeviceInfo enumeration and native AAudio stream
device IDs. EXCLUSIVE low-latency is attempted first; if unavailable, SHARED
mode is used. RG Rotate-named devices are hidden from both selection lists,
but SYSTEM DEFAULT may still physically route to a built-in device. The
CONFIG diagnostics show actual routes, actual output buffer, hardware burst
size, native XRUNs and ring starvation/drop counters. The platform may clamp
0.5 BURST to one or more hardware bursts; actual round-trip latency must be
measured. The app never claims to control the hardware burst size itself.

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

Native audio migration: 2026-10-11. Audio DSP is in AmpModel.h, parameters synchronized via JNI; Java AmpDsp.java remains the existing isolated smoke/model reference. Measure end-to-end analog latency externally; buffer frame count is not a round-trip latency measurement.
