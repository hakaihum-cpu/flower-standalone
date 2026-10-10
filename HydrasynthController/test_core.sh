#!/bin/sh
set -eu
cd "$(dirname "$0")"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
SRC=app/src/main/java/com/analoglav/hydrasynthcontroller
javac -d "$TMP" "$SRC/NrpnEncoder.java" "$SRC/ParameterCatalog.java" test/CoreTest.java
java -cp "$TMP" CoreTest
