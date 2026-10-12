# AMPS — AC30-style combo amplifier trial (AN-68)

Standalone Android guitar-amp effects processor for RG Rotate (720×720 reference).
This is an original AC30-inspired design, not a measured/convolutional reproduction
of a VOX amplifier and not endorsed by the brand. Artwork is drawn directly by
Android Canvas in amber-on-black DOT style; no generated images.

Signal: Android-selected INPUT (native AAudio read stream, NO input callback)
-> one high-priority output callback -> nonblocking INPUT read(timeout=0)
-> InputFade (no independent FIFO / no prefill) -> AC30-inspired DSP
-> Reverb and Tremolo -> selected OUTPUT.
Input PCM16 mono; output float stereo, AAudio output hardware sample rate.
One callback owns the whole duplex audio operation. It performs zero-timeout
input reads and never waits for an independent producer FIFO.

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

## AN-68 investigation and actual duplex optimization (2026-10-12)
Findings:
- 0.5 BURST showing 20.0ms refers to AAudio OUTPUT buffer capacity
  expressed as time (frames/sampleRate), NOT measured analog round-trip
  latency. AAudio can clamp the request based on the device/driver.
- Prior revision used two independently scheduled callbacks plus a custom
  PCM FIFO (prefill and target queue). This extra queue adds app latency.
- Android/Oboe official FullGuide recommends using output callbacks as the
  duplex master and reading input via a timeout=0 nonblocking read. The
  current native implementation follows this documented approach and omits
  the independent app FIFO entirely.
- Read underflows yield a short 32-frame ramp-to-zero; a 64-frame fade-in
  prevents a full-amplitude discontinuity when input resumes.
- CONFIG shows actual OUTPUT BUFFER frames and ms (NOT RTT), Android
  output/input stream timestamp *estimates* (NOT analog loopback),
  stream callback size/burst, OUT XRUN, input short-read count and native
  low_latency/pro feature declarations. If estimates are unavailable, show
  N/A. AAudio timestamps do not include all USB codec/ADC/DAC latency.
- Success requires RG Rotate + user's USB device: 0.5 vs 8, stream
  performance mode, actual stream device ids, buffer/burst, input misses,
  and analog loopback measurement when available. Some USB/Android HAL
  paths have fixed periods; an app cannot override device-driven burst size.
- Reference: Android AAudio/Oboe FullGuide and AAudio NDK buffer/timestamp
  API docs. App code was reviewed against these; static and native helper
  tests do not establish actual device latency or audible quality.

## AN-68 Interactive TUNING & read-only USB descriptor probe
CONFIG > USB TUNING / DIAGNOSTICS enters an independent 720x720 panel.
AAudio tuning parameters are *actually wired* to the native stream creation:
OUT BUFFER FRAMES (0=AUTO existing BURST setting), CALLBACK FRAMES
(0=platform default), INPUT BUFFER FRAMES (0=platform default), SHARING MODE
(PREFER EXCLUSIVE with SHARED fallback / SHARED ONLY). Press APPLY & RESTART
AUDIO to persist values and reopen streams. Each value can be cycled by
the left/right arrows, or selected from a complete list by tapping its
centre. RESET DEFAULTS & APPLY is a recovery path. All choices have safe
whitelisted bounds.

READ USB DESCRIPTORS queries the chosen connected device, requesting
Android USB permission when necessary. Parsing is non-invasive: no interface
is claimed; no ISO URB is submitted. Captured report includes the raw USB
descriptor bytes in hex and decoded Audio Streaming alternate settings and
ISO endpoint directions. The copy and share buttons generate a complete
text report with applied/draft tuning parameters, actual AAudio settings,
timestamps estimates, buffer frames, selected device IDs and USB descriptors.

This is ***AAudio tuning plus USB reconnaissance***, not yet a direct
usbfs/UAC audio driver. These parameters cannot be claimed to tune
usbfs isochronous URB submission or native USB packet scheduling. A real
analog loopback remains required to pass the <=10ms target. The 2025+
source-only external research constraint remains in effect.

## USBFS direct isochronous transport experiment (AN-68, 2026-10-12)
CONFIG > USB TUNING > USBFS ISO TEST is an actual USB Host/usbfs
transfer experiment, not an AAudio tuning value or descriptor-only scan.
It requires user permission, stops the AAudio engine, force-claims the UAC
Audio Streaming input and output interfaces, sets streaming alternates,
submits isochronous URBs to both IN and OUT endpoints, and collects actual
completion/error counters. Output transfers are SILENCE ONLY. Device may
require a physical reconnect to restore Android's USB audio route.
JNI receives UsbDeviceConnection.getFileDescriptor(). Host native source
uses USBDEVFS_SUBMITURB, REAPURBNDELAY, DISCARDURB; no AAudio calls.
The report, including USB descriptors and usbfs transfer counters, can be
viewed and saved as a UTF-8 .txt using Android's document picker.
This stage has not yet implemented PCM loopback, UAC clock/feedback control,
amp DSP on USB, or analog round-trip latency measurement. ISO submission
success is not equivalent to achieving 10ms. Unrecognized endpoints or
permissions are reported as test errors rather than silently using AAudio.
Test is deliberately manual, not automatically started with the app.
