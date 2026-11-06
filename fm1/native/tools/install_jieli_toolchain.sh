#!/bin/sh
# Install the pinned original JieLi pi32v2 compiler; no firmware build here.
# Never stream a network response directly to tar (CI #267).
set -eu
DEST=${1:?usage: install_jieli_toolchain.sh INSTALL_DIR}
SHA=${MI_JIELI_ARCHIVE_SHA256:-f686586bcfb45e0f0bb27fd2b39c7a7f313cb4f0e88a66a14da621ffa8225958}
PRIMARY=https://jl-update.oss-cn-shenzhen.aliyuncs.com/jieli-linux-toolchains-20250324.1.tar.xz
BACKUP=https://pkgman.jieliapp.com/s/linux-toolchain
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
archive="$WORK/jieli.tar.xz"
obtained=0
for url in "$PRIMARY" "$BACKUP"; do
    rm -f "$archive"
    printf 'JieLi toolchain: requesting verified archive from %s\n' "$url"
    if curl --fail --location --silent --show-error \
        --connect-timeout 8 --max-time 95 --retry 0 \
        --output "$archive" "$url"; then
        if printf '%s  %s\n' "$SHA" "$archive" | sha256sum --check --status; then
            obtained=1
            break
        fi
        echo 'JieLi toolchain: archive SHA-256 mismatch; rejecting this source' >&2
    else
        echo 'JieLi toolchain: download failed; trying alternate source' >&2
    fi
done
if [ "$obtained" -ne 1 ]; then
    echo 'FAIL: neither JieLi source provided the pinned verified compiler' >&2
    exit 1
fi
# Guard format and contents before writing to the final toolchain directory.
xz --test "$archive"
tar -tJf "$archive" >/dev/null
tar -xJf "$archive" -C "$WORK"
root=$(find "$WORK" -mindepth 1 -maxdepth 1 -type d -name 'jieli-linux-toolchains-*' -print -quit)
if [ -z "$root" ] || [ ! -x "$root/pi32v2/bin/clang" ]; then
    echo 'FAIL: archive has no expected jieli-linux-toolchains-*/pi32v2/bin/clang' >&2
    exit 1
fi
mkdir -p "$DEST"
name=$(basename "$root")
if [ -e "$DEST/$name" ]; then
    echo 'FAIL: pinned toolchain destination already exists; refusing overwrite' >&2
    exit 1
fi
mv "$root" "$DEST/$name"
ln -sfn "$name" "$DEST/toolchain"
test -x "$DEST/toolchain/pi32v2/bin/clang"
"$DEST/toolchain/pi32v2/bin/clang" --version
printf 'PASS: pinned JieLi toolchain, SHA-256 verified: %s\n' "$SHA"
