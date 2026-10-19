#!/usr/bin/env bash
set -euo pipefail
# Pinned libpd 0.16.1 and its pinned Pure Data submodule.
LIBPD_SHA=ba0dc63262901d658af8bbda5e619a60fa975e78
PUREDATA_SHA=f009fd8d7b537e209e09898d487fdf1bf547da2b
mkdir -p third_party
if [ ! -d third_party/libpd/.git ]; then
  git clone --depth 1 https://github.com/libpd/libpd.git third_party/libpd
fi
if [ "$(git -C third_party/libpd rev-parse HEAD)" != "$LIBPD_SHA" ]; then
  git -C third_party/libpd fetch --depth 1 origin "$LIBPD_SHA"
  git -C third_party/libpd checkout --detach "$LIBPD_SHA"
fi
test "$(git -C third_party/libpd rev-parse HEAD)" = "$LIBPD_SHA"
git -C third_party/libpd submodule update --init --depth 1 pure-data
test "$(git -C third_party/libpd/pure-data rev-parse HEAD)" = "$PUREDATA_SHA"
test -f third_party/libpd/libpd_wrapper/z_libpd.h
test -f third_party/libpd/pure-data/src/m_pd.h
# Android SDK default CMake is 3.22.x; libpd 0.16.1 declares 3.25.
# Its CMake constructs used by this static build are compatible with 3.22,
# so patch ONLY generated checkout, never the pinned Git source in repo.
python3 - <<'PY'
from pathlib import Path
path = Path("third_party/libpd/CMakeLists.txt")
data = path.read_text()
needle = "cmake_minimum_required (VERSION 3.25)"
assert data.count(needle) == 1, "Unexpected upstream libpd CMake contract"
path.write_text(data.replace(needle, "cmake_minimum_required (VERSION 3.22)", 1))
PY
printf '[PASS] libpd pinned %s / pure-data %s\n' "$LIBPD_SHA" "$PUREDATA_SHA"
