#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
javac -d "$TMP" "$ROOT/app/src/main/java/com/analoglav/amps/AmpDsp.java" "$ROOT/test/DspSmoke.java"
java -cp "$TMP" com.analoglav.amps.DspSmoke

# Native model test runs without the Android SDK; identical DSP header
# is also compiled by NDK as part of the actual APK.
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -o "$TMP/native_smoke" "$ROOT/test/NativeSmoke.cpp"
"$TMP/native_smoke"

# Simulate split full-duplex capture/playback clocks on the host CPU.
# Covers steady callbacks, clock drift and input-disconnect fade.
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -o "$TMP/fifo_smoke" "$ROOT/test/AudioFifoSmoke.cpp"
"$TMP/fifo_smoke"

# Match the 20ms/960-frame output callback the user observed in the UI.
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -o "$TMP/large_callback" "$ROOT/test/VariableBurstSmoke.cpp"
"$TMP/large_callback"

# Test the production input fade helper used by the output callback.
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -o "$TMP/input_fade_smoke" "$ROOT/test/InputFadeSmoke.cpp"
"$TMP/input_fade_smoke"
# Static duplex guard: old two-callback FIFO must not silently return.
grep -Fq 'AAudioStream_read(e.input_,input,count,0)' "$ROOT/app/src/main/cpp/AAudioEngine.cpp"
! grep -Fq 'inputCallback(' "$ROOT/app/src/main/cpp/AAudioEngine.cpp"
! grep -Fq 'fifo_.pop(' "$ROOT/app/src/main/cpp/AAudioEngine.cpp"
grep -Fq 'AAudioStream_getTimestamp(' "$ROOT/app/src/main/cpp/AAudioEngine.cpp"

# Same settings and descriptor parser used by AMPS TUNING page.
javac -d "$TMP" "$ROOT/app/src/main/java/com/analoglav/amps/TuningProfile.java" \
  "$ROOT/test/TuningProfileTest.java"
java -cp "$TMP" com.analoglav.amps.TuningProfileTest
javac -d "$TMP" "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbAudioDescriptorParser.java" \
  "$ROOT/test/UsbAudioDescriptorParserTest.java"
java -cp "$TMP" com.analoglav.amps.usb.UsbAudioDescriptorParserTest
# All four adjustable fields must be used by the native AAudio engine.
grep -Fq 'AAudioStream_setBufferSizeInFrames(input_,inFrames)' "$ROOT/app/src/main/cpp/AAudioEngine.cpp"
grep -Fq 'AAudioStream_setBufferSizeInFrames(output_,requestedOutput)' "$ROOT/app/src/main/cpp/AAudioEngine.cpp"
grep -Fq 'AAudioStreamBuilder_setFramesPerDataCallback(builder,cbFrames)' "$ROOT/app/src/main/cpp/AAudioEngine.cpp"
grep -Fq 'shareMode==1?' "$ROOT/app/src/main/cpp/AAudioEngine.cpp"

# Compile usbfs JNI probe on Linux before expensive Gradle setup.
JDK_TOP="$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")"
test -f "$JDK_TOP/include/jni.h"
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -I "$JDK_TOP/include" -I "$JDK_TOP/include/linux" \
  -c "$ROOT/app/src/main/cpp/UsbIsoTester.cpp" -o "$TMP/UsbIsoTester.o"
grep -Fq 'USBDEVFS_SUBMITURB' "$ROOT/app/src/main/cpp/UsbIsoTester.cpp"
grep -Fq 'AAUDIO=NOT_USED_DURING_USBFS_TEST' "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbIsoTester.java"

# AN-68/libusb Host mode: source-path and dependency isolation audit.
# Actual libusb JNI compilation happens in the existing NDK/CMake job after
# FetchContent obtains the SHA-pinned upstream source.
test -f "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp"
grep -Fq 'GIT_TAG bea6567b63796ab27c1c33257d230284fb2d8316' \
  "$ROOT/app/src/main/cpp/CMakeLists.txt"
grep -Fq 'libusb::usb-1.0' "$ROOT/app/src/main/cpp/CMakeLists.txt"
grep -Fq 'UsbLibusbHost.cpp' "$ROOT/app/src/main/cpp/CMakeLists.txt"
! grep -Eq '^[[:space:]]*UsbPcmEngine\.cpp([[:space:]]|$)' \
  "$ROOT/app/src/main/cpp/CMakeLists.txt"
grep -Fq 'libusb_wrap_sys_device' "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp"
grep -Fq 'libusb_init_context' "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp"
grep -Fq 'LIBUSB_OPTION_NO_DEVICE_DISCOVERY' "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp"
grep -Fq 'libusb_fill_iso_transfer' "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp"
grep -Fq 'libusb_submit_transfer' "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp"
grep -Fq 'libusb_handle_events_timeout_completed' "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp"
grep -Fq 'libusb_cancel_transfer' "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp"
grep -Fq 'ENGINE=LIBUSB_UAC1_HOST_ASYNC_DSP' "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp"
grep -Fq 'ENGINE_CONFIG="+(usbDirectEnabled?"LIBUSB_UAC1_HOST":"AAUDIO")' \
  "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'UsbDirectFormat.find(device,c.getRawDescriptors())' \
  "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectEngine.java"
grep -Fq 'findAudioControl(device,c.getRawDescriptors()' \
  "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectEngine.java"
grep -Fq 'UAC_CONTROL_CLAIMED=true' \
  "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectEngine.java"
grep -Fq 'UsbDirectEngine.execute(this,device' \
  "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'AAUDIO_RUNTIME_RUNNING=' \
  "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'INSTALLED_APK_VERSION=' \
  "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'UsbDiagnosticStore.load(this)' \
  "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'controlTransfer(0x22,0x01,0x0100' \
  "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectEngine.java"
grep -Fq 'TARGET_DEVICE_STILL_ENUMERATED=' \
  "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectEngine.java"
# Native JNI symbols are defined exclusively by the selected libusb host.
test "$(grep -c 'Java_com_analoglav_amps_usb_UsbDirectEngine_nativeRun' \
  "$ROOT/app/src/main/cpp/UsbLibusbHost.cpp")" -eq 1
