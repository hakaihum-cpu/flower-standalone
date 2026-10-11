#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
javac -d "$TMP" "$ROOT/app/src/main/java/com/analoglav/amps/AmpDsp.java" "$ROOT/test/DspSmoke.java"
java -cp "$TMP" com.analoglav.amps.DspSmoke
