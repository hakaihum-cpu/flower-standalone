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

# USB Direct transport/DSP; native JNI compilation is mandatory before Gradle.
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -I "$JDK_TOP/include" -I "$JDK_TOP/include/linux" \
  -c "$ROOT/app/src/main/cpp/UsbPcmEngine.cpp" -o "$TMP/UsbPcmEngine.o"
grep -Fq 'UsbDirectFormat.find(device,c.getRawDescriptors())' "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectEngine.java"
grep -Fq 'UsbDirectEngine.execute(this,device' "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'UsbDiagnosticStore.load(this)' "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"

# Regression guards for actual iRig PRO DUO endpoint shutdown report.
grep -Fq 'FIRST_OUT_URB_STATUS' "$ROOT/app/src/main/cpp/UsbPcmEngine.cpp"
grep -Fq 'failure==-ESHUTDOWN' "$ROOT/app/src/main/cpp/UsbPcmEngine.cpp"
grep -Fq 'controlTransfer(0x22,0x01,0x0100' "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectEngine.java"
grep -Fq 'inputRateControl=fmtIn.rateCtrlIn' "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectFormat.java"
grep -Fq 'USB_DESCRIPTORS' "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"

# Avoid misleading AAudio reports when USB DIRECT is selected.
grep -Fq 'ENGINE_ACTUALLY_RUNNING=' "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'AAUDIO_RUNTIME_RUNNING=' "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'INSTALLED_APK_VERSION=' "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'if(usbDirectEnabled)chooseUsbQueueDepth();else chooseBurst();' "$ROOT/app/src/main/java/com/analoglav/amps/MainActivity.java"
grep -Fq 'aaudioRunning=s[0]==1' "$ROOT/app/src/main/java/com/analoglav/amps/AudioBridge.java"

# AN-68: USB OUT-first implementation guards. Fail before expensive Gradle.
grep -Fq 'DIRECT_INIT_ORDER=OUT_FIRST' "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectEngine.java"
grep -Fq 'TARGET_DEVICE_STILL_ENUMERATED=' "$ROOT/app/src/main/java/com/analoglav/amps/usb/UsbDirectEngine.java"
grep -Fq 'OUTPUT_FIRST_STARTUP=1' "$ROOT/app/src/main/cpp/UsbPcmEngine.cpp"
grep -Fq 'err=-ETIMEDOUT' "$ROOT/app/src/main/cpp/UsbPcmEngine.cpp"
grep -Fq 'completedOut.load()>0 ? 1:0' "$ROOT/app/src/main/cpp/UsbPcmEngine.cpp"
