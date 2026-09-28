#!/usr/bin/env bash
set -euo pipefail

BLENDER_VERSION="4.5.14"
ARCHIVE="blender-${BLENDER_VERSION}-linux-x64.tar.xz"
URL="https://download.blender.org/release/Blender4.5/${ARCHIVE}"
ROOT="${1:-$PWD/.flower3d-tools}"
DEST="${ROOT}/blender-${BLENDER_VERSION}"

mkdir -p "${ROOT}"

if [ ! -x "${DEST}/blender" ]; then
  cd "${ROOT}"
  if [ ! -f "${ARCHIVE}" ]; then
    curl -fL --retry 3 --retry-delay 3 -o "${ARCHIVE}" "${URL}"
  fi
  tar -xf "${ARCHIVE}"
fi

"${DEST}/blender" --version
