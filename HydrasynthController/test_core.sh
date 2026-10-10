#!/bin/sh
set -eu
cd "$(dirname "$0")"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
SRC=app/src/main/java/com/analoglav/hydrasynthcontroller
javac -d "$TMP" "$SRC/NrpnEncoder.java" "$SRC/ParameterCatalog.java" \
  "$SRC/HydraDumpProtocol.java" "$SRC/HydraPatchSnapshot.java" \
  "$SRC/AnalogKeysCatalog.java" \
  test/CoreTest.java test/HydraDumpTest.java test/AnalogKeysCoreTest.java
java -cp "$TMP" CoreTest
java -cp "$TMP" HydraDumpTest
java -cp "$TMP" AnalogKeysCoreTest
