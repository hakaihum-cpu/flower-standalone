#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
g++ -std=c++20 -O2 -Wall -Wextra -pedantic \
  "$ROOT/Core/WavIO.cpp" "$ROOT/Core/SamplerCore.cpp" "$ROOT/tests/test_core.cpp" \
  -o "$ROOT/tests/test_core"
"$ROOT/tests/test_core"
