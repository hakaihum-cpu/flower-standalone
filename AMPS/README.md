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

## AN-68 USB DIRECT engine (v0.7.0 experimental)
Existing CONFIG includes an AUDIO ENGINE selector (AAUDIO / USB DIRECT).
When USB DIRECT is selected, the application stops AAudio and attempts to
identify iRig Pro Duo via USB Host, obtain device permissions, verify UAC1
AudioStreaming Type-I signed PCM with 48kHz, exactly 2 channels, and matching
input/output subframe size and resolution; claim the UAC interfaces, set
alternate settings and use linux USBDEVFS_SUBMITURB/REAPURBNDELAY for a
continuous native input -> AmpModel AC30 DSP -> output path. This stream never
uses AudioRecord, AudioTrack, or AAudio, and NEVER silently falls back when
USB support is insufficient.

48k PCM is supported for explicitly verified USB Full Speed and High Speed.
Format and speed are checked at runtime; unsupported UAC2 and uncertain formats
do not start direct output. Output is hard-limited through existing AmpModel.
The USB data endpoint choice excludes feedback endpoints. External clock
feedback and asynchronous long-term drift management are not yet implemented,
so audible stability and <=10ms analog roundtrip are NOT verified. Initial
output contains silent packets; subsequent packets contain processed samples.
USB device may require replug after interface claim; use low volume on testing.

The existing manual USBFS ISO TEST remains available only in AAudio mode.
The existing AAudio amp behavior is preserved when explicitly selected.

TXT export fixes: before opening ACTION_CREATE_DOCUMENT, diagnostic contents
are atomically persisted in private app files. If Activity is recreated while
the Android document picker is open, onActivityResult loads this snapshot and
refuses to write a 0-byte report. Startup and terminal USB status are
persisted. The mode reports native IN/OUT completions, bytes, underruns and
errors as the basis for subsequent tuning. This revision adds NO automatic
CI build and makes NO changes to Golden/main or other apps.

## AN-68 iRig Pro Duo USB DIRECT report (2026-10-12, v0.7.1)
On actual RG Rotate Android SDK31 / iRig PRO DUO (USB VID 6499 PID 16):
- USB Full Speed, UAC1 48k/24bit stereo PCM Type I, 3-byte subframes.
- Input streaming IF=2 ALT=1 ep=0x82, OUT IF=1 ALT=1 ep=0x01.
- Both endpoints advertise maxPacket=288 bytes per 1ms (48 frames).
- Native direct USB input completed 22 packets / 6336 bytes.
- Native direct USB output completed ZERO packets despite 35 submit
  attempts. The kernel reported ERROR_CODE=-108 (ESHUTDOWN), with
  33 aggregate transfer errors; the first direction/URB/packet status
  was not recorded. This does NOT prove physical USB output is impossible.
- Native audio loopback NOT running, analog round trip still unmeasured.
The prior 96/96/192 AAudio tweaks are unrelated to USBFS direct PCM.

Fixes:
- Native USB error reporting records FIRST_IN/OUT_URB_STATUS and
  FIRST_IN/OUT_PACKET_STATUS separately and first submission failures,
  instead of hiding the first failure behind 32 blind retries.
  Fatal ESHUTDOWN/ENODEV/EPIPE/EPERM stops immediately.
- UAC1 parser now reports output endpoint sync type and optional feedback
  endpoint address, and class-specific sampling-frequency-control bit.
- When UAC1 explicitly advertises the endpoint sampling rate control,
  send standard class-specific SET_CUR 48000Hz (3 little-endian bytes)
  after selecting the alternate interface. If GET_CUR succeeds, verify
  actual rate. If SET_CUR fails or GET_CUR disagrees, fail closed.
- Always persist the full USB descriptor scan alongside the terminal
  direct PCM result in the diagnostic TXT rather than overwriting it.
- No AAudio fallback, no modification to existing normal AAudio mode,
  no guessed USB VID/PID wiring; external clock feedback implementation
  remains pending, analog latency target <=10ms still unverified.

This is source-level repair, NOT BUILT or USB-device verified at this
checkpoint; needs the usual preflight and one manual CircleCI job.

## AN-68 Engine-path audit v0.7.2 (2026-10-12)
User still perceives AAudio-like behavior, with no audible output change.
Previous RG Rotate report explicitly showed USBFS_DIRECT_PCM_DSP, USB input
22 successful URBs (6,336 bytes), USB output 0 completed URBs and -108.
Hence the native IN was USBFS, not AAudio; the actual problem is USB
output completion, not a proven AAudio fallback.

Code audit shows USB DIRECT mode calls bridge.stop() and does not invoke
bridge.start() automatically on failure. However CONFIG always displayed
AAudio burst/latency values, and DIAGNOSTICS title said AAUDIO ACTIVE
even when USB DIRECT was selected. These are misleading and were fixed:
- USB DIRECT CONFIG now shows engine selected vs actually running;
  native AAudio state is read from nativeStats()[0], shown separately.
- AAudio INPUT/OUTPUT and burst controls are not usable in USB DIRECT.
  Instead, show USB queue depth, explicit restart, USB device info,
  native IN/OUT/underrun/error counters, and link to USB DIAGNOSTICS.
- USB DIAGNOSTICS no longer says AAUDIO ACTIVE while in USB DIRECT
  and no longer shows inactive AAudio tuning controls.
- TXT report identifies INSTALLED_APK_VERSION, ENGINE_CONFIG,
  ENGINE_ACTUALLY_RUNNING, AAUDIO_RUNTIME_RUNNING and USB_DIRECT_STATS;
  AAudio buffer metrics are only included when AAudio is selected.
- USB DIRECT mode does NOT claim success when USB output fails. The
  OUTPUT 0 URB -108 ESHUTDOWN hardware issue remains unresolved until
  a v0.7.1-or-newer real-device error report is available.
- Only AMPS UI/Java diagnostics changed; native DSP and USB transfer
  code are unchanged from v0.7.1. No auto-CI build. Manual APK
  version must be verified to avoid mistaking old installed APK for
  new code; no claims of working audio or <=10ms roundtrip.

## AN-68 / 0.7.3 USB OUT-first playback startup experiment

RG Rotate with iRig PRO DUO on v0.7.0 provided real USBFS traffic:
IN IF=2 ALT=1 EP=0x82, 22 received ISO packets (6336 bytes);
OUT IF=1 ALT=1 EP=0x01, 35 submitted but no ISO packet completed.
Terminal error -108 (ESHUTDOWN) with 33 transfer errors.
No working amp output or analog round-trip <=10ms was measured.

Changes in 0.7.3:
- Configure the UAC1 playback alternate interface BEFORE capture.
- Submit playback SILENCE URBs before capture URBs.
- Treat zero output packet completions after 250ms as -ETIMEDOUT.
- Record TARGET_DEVICE_STILL_ENUMERATED for bus failure diagnosis.
- Native RUN only indicates both IN and OUT completed a packet.
- Keep per-direction initial URB/packet error diagnostics.
- No implicit AAudio fallback, no DSP/UI redesign, no new CI system.

This is an error-isolation and possible startup ordering fix, NOT proof
that Android released the USB playback interface. If OUT remains zero,
inspect first-error codes and USB enumeration before further changes.
