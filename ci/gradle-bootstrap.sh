#!/usr/bin/env bash
set -euo pipefail

GRADLE_VERSION="9.6.0"
GRADLE_SHA256="bbaeb2fef8710818cf0e261201dab964c572f92b942812df0c3620d62a529a01"
CACHE_ROOT="${HOME}/.cache/androidapp"
ZIP_PATH="${CACHE_ROOT}/gradle-${GRADLE_VERSION}-bin.zip"
DIST_DIR="${CACHE_ROOT}/gradle-${GRADLE_VERSION}"
URL="https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip"

mkdir -p "${CACHE_ROOT}"

if [[ ! -x "${DIST_DIR}/bin/gradle" ]]; then
  if [[ ! -f "${ZIP_PATH}" ]]; then
    curl --fail --location --retry 3 --output "${ZIP_PATH}" "${URL}"
  fi
  echo "${GRADLE_SHA256}  ${ZIP_PATH}" | sha256sum --check -
  rm -rf "${DIST_DIR}"
  unzip -q "${ZIP_PATH}" -d "${CACHE_ROOT}"
fi

exec "${DIST_DIR}/bin/gradle" "$@"
