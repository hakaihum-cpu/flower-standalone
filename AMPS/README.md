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

## AN-68 / 0.7.4 - Direct USBFS ISO scheduling repair, real iRig report

The 0.7.3 USB direct hardware report (RG Rotate, iRig PRO DUO,
VID 6499 PID 16, Android 31) confirmed that AAudio was CLOSED.
Playback/capture UAC1 stereo 24-bit @ 48kHz, interface/alternate
OUT=1/1 EP=0x01 and IN=2/1 EP=0x82; SET_CUR and GET_CUR for 48kHz
BOTH succeeded. Native submissions=2 OUT/2 IN; completed=0.
First OUT URB=-108 ESHUTDOWN, first OUT ISO packet=-18 EXDEV;
device enumeration persisted. This demonstrates a USB endpoint/bus
scheduling failure, not an AAudio routing or sample-rate error.
It is not proof of the exact underlying kernel/host cause.

The direct PCM engine had one 1ms packet per URB with queue depth 2
and polled for up to 20ms BEFORE checking for completions. Such gaps
can starve an ISO queue, which should remain scheduled ahead of time.
Change: 2 packets/URB and a reap-first loop; after EAGAIN poll
for at most 1ms. Requested depth remains configurable (depth=2
offers 4ms nominal transfer queue per direction at full speed).
Each successful ISO input packet decodes PCM from its individual
offset; each output packet contributes to completion diagnostics.
Add detailed packet success counts, first error packet index/start
frame, max event-loop gap to the report. Preserve 0.7.3 output-first
initialization, UAC1 rate controls, fail-closed behavior and no
AAudio fallback. This might help USB host scheduling but does not
prove that device/endpoint ESHUTDOWN will be resolved.

Native host smoke/static tests must pass before one manual CircleCI
run. Real Android build/audio/analog RTT still unverified here.

## AN-68 / 0.7.5 USB audio-control claim + active ALT readback
Actual 0.7.4 RG Rotate/iRig PRO DUO repeats native OUT URB
-108 ESHUTDOWN, packet -18 EXDEV, with 0 IN/OUT completions.
48kHz UAC1 SET_CUR/GET_CUR succeed; device stays enumerated. USB
queue timing fixes alone were ineffective.

The iRig UAC1 AudioControl HEADER at IF0 declares bInCollection=2
streaming interfaces [1,2] (playback+capture). A SEPARATE
AudioControl HEADER at IF3 declares only MIDIStreaming IF4.
Therefore claim only the AudioControl interface matching the exact
playback/capture pair via the UAC1 header descriptor's baInterfaceNr.
For this iRig that is IF0. IF3/IF4 MIDI are NOT touched.

Android UsbDeviceConnection.claimInterface(IF0,true) requests
exclusive control and detaches the kernel driver if needed before
claiming playback/capture streaming interfaces. Log success/failure;
do not continue USB DIRECT if the matching control interface cannot
be verified or claimed. Record standard GET_INTERFACE altsetting for
both IN and OUT before and after USBFS native streaming. The readback
may identify a concurrent reset from ALT1 to ALT0. On error,
terminate fail-closed, release only claimed interfaces; no indiscriminate
USB reset and no AAudio fallback.

Online evidence: Android UsbDeviceConnection API documents force
kernel driver detach; Linux USB audio endpoint code uses
usb_set_interface to activate or deactivate streaming endpoints;
Linux libusb treats -ESHUTDOWN as an unusable/disconnected USB stream.
This is a targeted hypothesis, NOT a verified cure for the actual
kernel's OUT endpoint shutdown. Tested failure may remain.

## AN-68 / 0.8.0 native libusb USB Audio Host (experimental)

The goal is an independent UAC1 USB Host for iRig PRO DUO / RG Rotate,
NOT incremental AAudio burst tuning and not another manual USBDEVFS URB
engine. USB Host architecture uses libusb, Native AmpModel AC30 DSP,
and Android UsbDeviceConnection solely for USB permission, descriptor
verification, AudioControl/AudioStreaming interface claiming, and UAC1
48000 Hz endpoint setup. AAudio mode remains available by user choice,
but USB Host audio never passes through AAudio/AudioRecord/AudioTrack.

Dependency: libusb-cmake commit bea6567b63796ab27c1c33257d230284fb2d8316
includes a vendored upstream libusb source tree. CMake FetchContent
downloads this exact immutable commit at NDK configure time and builds
libusb::usb-1.0 STATIC, linked into amps_audio. This is an EXTERNAL
GIT NETWORK BUILD DEPENDENCY; the existing CircleCI environment needs
GitHub access for the first Gradle/CMake configuration. No new workflow
or GitHub Actions introduced, signing configuration unchanged.

Transport: libusb_init_context(NO_DEVICE_DISCOVERY), libusb_wrap_sys_device
on the Android-permitted USB fd (Linux backend retains ownership of fd
with Android), asynchronous ISO two packets/transfer and 1–4
transfers/endpoint. Playback silence primed first, then capture. A
single native event thread uses libusb_handle_events_timeout_completed
at 1ms intervals, takes mono input channel 1, processes AmpModel
AC30 and writes stereo 24-bit output. All active transfers are
cancelled and drained on stop, or held allocated on timeout to avoid
USB use-after-free; cleanup leaks are explicit in diagnostics.
Native JNI run/stats/controls API kept compatible with UsbDirectEngine.
Legacy UsbPcmEngine.cpp stays as source reference but is EXCLUDED
from amps_audio target to prevent duplicate JNI symbols.
MIDI interfaces IF3 and IF4 are untouched by the Android claim logic.

Previous 0.7.4 real device behavior was OUT -ESHUTDOWN and zero
completions, despite correctly configured UAC1/48kHz/streaming ALT.
LIBUSB DOES NOT circumvent the underlying Linux USB host controller
and also uses Linux usbfs; therefore the same hardware issue may
persist. v0.8.0 is a deliberately separate transport implementation
and not a claim of ESHUTDOWN cure, working output or <=10ms RTT.
Only built/verified APK and loopback measurement can establish results.

Preflight source and invariants: AMPS/test_core.sh (existing CircleCI).
Run exactly one manual CircleCI manual_android_build with
run_build=true on the identified feature HEAD, after preflight.

## AN-68 — 0.9.0 private USB audio transport comparison (2026-10-13)

CONFIG > ENGINE displays AAudio (Android OS), libusb direct UAC1 and
Superpowered direct USB Host in one real mode-selection dialog. Selected mode
persists. Switching calls stop on both direct backends, closes AAudio, and
waits for the old worker to finish before reopening the selected transport.
No silent AAudio fallback exists in direct modes.

The Superpowered USB path uses Android UsbManager permission, openDevice,
raw USB descriptors + file descriptor -> Superpowered::AndroidUSB::onConnect
-> AndroidUSBAudio::easyIO(48000, 24, 2, 2, bufferSize, callback).
Callback runs the SAME AmpModel C++ AC30 DSP. stopIO/onDisconnect run before
USB connection close. Private evaluation options: 128/256/512 frame
Superpowered buffers; libusb's separate ISO URB depth remains 1–4.
Active stream requires delivered PCM callbacks; failed native start shows
SUPERPOWERED_ERROR instead of pretending AAudio playback. Existing USBFS
ISO TEST is still a transport diagnostic, not a proven USBFS DSP engine.

2026-09-09 upstream Superpowered SDK 2.8.2 is pinned to
7f3d2f1ec15334b2424e2a60e0af457aa3f76620. The arm64 SDK static
library is fetched at NDK configure time and is never copied into this repo.
The vendor evaluation license is for private R&D ONLY; no public release
without a paid license: https://superpowered.com/licensing .
Neither libusb nor Superpowered bypass the USB host controller or assure
working iRig playback. This revision does NOT implement analog RTT measurement;
software callback counters and buffer values must not be called measured RTT.

Status before CircleCI/real-device tests: READY FOR CI / NOT BUILT.
Do not merge, trigger CI or claim latency gains without real evidence.
