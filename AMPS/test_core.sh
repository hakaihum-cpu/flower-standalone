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
