# EP Live Instrument Sampler (AN-72)

Windows standalone sampling workstation for building EP-SAMPLE-compatible `EPBANK1` files from a real instrument.

## User flow

1. **AUDIO / ASIO**
   - Select driver, input/output device, input/output channel (MONO) or adjacent channel pair (STEREO), and buffer.
   - Capture rate is intentionally fixed at **48 kHz** for EP-SAMPLE compatibility.
   - Run **RUN ASIO CHECK**. A complete ASIO verification requires:
     - ASIO driver enumerated.
     - Selected device opens at 48 kHz.
     - Real input signal detected while the user plays a key.
     - Test tone is physically heard from the selected output and confirmed by the user.
     - Device closes and reopens with the same setup.
   - Passing this self-check is required before the build can be called ASIO VERIFIED on that Windows/interface combination.

2. **SESSION**
   - Select START NOTE within the existing EP-SAMPLE range A0-C8 (MIDI 21-108).
   - Enter KEYS (1-88, and the final note must not exceed C8).
   - Set BANK NAME.
   - Press START.

3. **RECORD EACH KEY**
   - The app shows one target note at a time.
   - Press REC, play/hold the source instrument, then STOP.
   - The recorded waveform is shown.
   - Drag START/END handles (or enter seconds) to choose the region.
   - SET confirms that key and advances automatically.
   - RETAKE archives the rejected RAW WAV under `RAW/RETAKES` and records it again.
   - RAW recordings are preserved separately and never destructively trimmed.
   - Writer FIFO dropouts and audio-device xruns are checked per take. If either is detected, SET is disabled and RETAKE is required.

4. **FINISH / TUNING**
   - `FINISH → DIRECT BIN` builds an EPBANK1 directly from the confirmed takes.
   - `EXPORT MASTER WAV` creates a single 48 kHz/24-bit stereo WAV containing all keys in order, separated by 0.5 seconds of silence, plus `.epmap.json`.
   - Pitch-correct the Master WAV externally **without changing the timeline length**.
   - `IMPORT TUNED WAV → BIN` uses the `.epmap.json` boundaries to build a tuned EPBANK1.

## EPBANK1 policy

- Each recorded note is stored once.
- The existing EP-SAMPLE app's 8 velocity slots all point to the same recorded PCM for that note.
- RR1/RR2/RR3 also point to the same PCM; no fake variation is generated.
- Sustain uses the full selected region.
- Release uses the final 0.5 seconds of the selected region.
- No per-key normalization is applied.

## Build

Requires Windows x64, CMake, a current Visual Studio with C++ desktop tools, and internet access for JUCE FetchContent.

Run `BUILD_WINDOWS.bat`. The BAT lets CMake choose the installed Visual Studio generator instead of pinning a specific VS release.

The project pins JUCE 9.0.3. On Windows it compiles with `JUCE_ASIO=1` and `JUCE_WASAPI=1`.

## Verification boundary

Core WAV/EPBANK processing is platform-independent and has automated tests. ASIO itself is Windows/hardware dependent. Do **not** claim ASIO VERIFIED until the in-app ASIO CHECK passes on the target Windows PC with the target ASIO interface.

## Licensing note

JUCE ASIO builds depend on ASIO SDK source/licensing terms. Review the JUCE/ASIO license terms before redistributing a closed-source binary.
