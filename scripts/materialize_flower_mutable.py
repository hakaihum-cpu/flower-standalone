#!/usr/bin/env python3
"""Materialize pinned Mutable Instruments Plaits, Clouds and stmlib source for FLOWER Android.

The Android build uses immutable upstream commits already validated by the
accepted MIYAKO implementation.  Original MIT notices remain in all source
files.  Only the same hosted-platform compatibility patches used by MIYAKO are
applied after extraction:
- Plaits user_data.h: use its host/no-flash path on Android.
- Clouds debug_pin.h: no-op STM32 GPIO debug pin.
- stmlib dsp.h: portable Clip/Sqrt implementations on AArch64.
"""
from __future__ import annotations

import hashlib
from io import BytesIO
from pathlib import Path
from urllib.request import Request, urlopen
from zipfile import ZipFile

EURORACK = "08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4"
STMLIB = "e3bd7c9cc00e4364166f9905c0509b6ffd0535ec"

# Post-materialization git-blob SHAs taken from accepted MIYAKO Golden #242.
EXPECTED = {
    "Source/plaits/dsp/voice.cc": "64771114a2684fccc59ef87b7442c917cc077caf",
    "Source/plaits/dsp/voice.h": "d925d469b13e30c2081a174452f043e1ddb0c616",
    "Source/plaits/resources.cc": "5624c1e3d9aa48e954e6a197cf80b0cda0ef1fe5",
    "Source/plaits/user_data.h": "663ba8ddada53bf32fa68b7d628c350d95d4b146",
    "Source/clouds/dsp/granular_processor.cc": "2f007bc4933a0721474473c9b931753056b44b10",
    "Source/clouds/resources.cc": "3af91f32e7105877e142bff2bf6dee642809d180",
    "Source/clouds/drivers/debug_pin.h": "697a6c2b609a42d526380cc9d1736643b32059e5",
    "Source/stmlib/dsp/dsp.h": "635a56a2bb55127985d770f9ad32b1e131e39d52",
    "Source/stmlib/utils/random.cc": "3484dac2e415a1c5c0a56162aa82dd16adec94a5",
    "Source/stmlib/stmlib.h": "a4fc9d5cc0b5f9f4784d228783ef1c5f884781d1",
    "Source/stmlib/LICENSE": "362def273410712a6967c46f6503c17525347d24",
}

def git_blob_sha(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode("ascii") + b"\0" + data
    ).hexdigest()

def download_zip(url: str) -> ZipFile:
    req = Request(url, headers={"User-Agent": "FLOWER-Mutable-pinned-build/1"})
    with urlopen(req, timeout=120) as response:
        return ZipFile(BytesIO(response.read()))

def extract_subtree(z: ZipFile, archive_root: str, subtree: str, target_root: Path) -> None:
    clean = subtree.strip("/")
    prefix = archive_root + (clean + "/" if clean else "")
    count = 0
    for info in z.infolist():
        if info.is_dir() or not info.filename.startswith(prefix):
            continue
        relative = info.filename[len(archive_root):]
        target = target_root / "Source" / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(z.read(info.filename))
        count += 1
    if count == 0:
        raise RuntimeError(f"No files extracted for {subtree}")
    print(f"Materialized {count} files from {subtree}")

def patch_plaits_user_data(root: Path) -> None:
    path = root / "Source/plaits/user_data.h"

    # Match the accepted MIYAKO Android port exactly.  The upstream MCU
    # implementation accesses STM32 FLASH directly; Android has no equivalent.
    # Factory Plaits engines remain available because Voice::Render falls back
    # to the compiled factory FM patch table when ptr() returns nullptr.
    path.write_text(
        "// Plaits Android port compatibility shim for user-uploaded STM32 FLASH data.\n"
        "// The upstream MCU original is preserved verbatim as\n"
        "// Source/plaits/user_data_stm32_original.h.\n"
        "// Factory Plaits engines (including the 6-op factory presets) still work:\n"
        "// plaits::Voice::Render uses fm_patches_table when ptr() returns nullptr.\n"
        "// This file is NOT an alternate synthesis engine.\n"
        "#pragma once\n"
        "#include <cstdint>\n"
        "namespace plaits {\n"
        "class UserData {\n"
        " public:\n"
        "  UserData() = default;\n"
        "  inline const uint8_t* ptr(int /*slot*/) const noexcept { return nullptr; }\n"
        "};\n"
        "} // namespace plaits\n",
        encoding="utf-8",
    )

def patch_clouds_debug_pin(root: Path) -> None:
    path = root / "Source/clouds/drivers/debug_pin.h"
    path.write_text(
        "// Android/JUCE port boundary. Replaces only the STM32 hardware debug GPIO,\n"
        "// never an audio algorithm from the original MIT-licensed Clouds engine.\n"
        "#pragma once\n"
        "namespace clouds {\n"
        "struct DebugPin {\n"
        "    static void Init() noexcept {}\n"
        "    static void High() noexcept {}\n"
        "    static void Low() noexcept {}\n"
        "};\n"
        "}\n"
        "#define TIC\n"
        "#define TOC\n",
        encoding="utf-8",
    )

def patch_stmlib_aarch64(root: Path) -> None:
    path = root / "Source/stmlib/dsp/dsp.h"
    data = path.read_bytes()
    if b"#include <cmath>" not in data:
        anchor = b"#include <math.h>"
        if data.count(anchor) != 1:
            raise RuntimeError("Unexpected stmlib dsp.h include block")
        data = data.replace(anchor, b"#include <cmath>\n#include <math.h>")
    needle = b"#ifdef TEST"
    if data.count(needle) != 2:
        raise RuntimeError(
            f"Unexpected stmlib dsp.h TEST branch count: {data.count(needle)}")
    data = data.replace(
        needle,
        b"#if defined(TEST) || defined(__aarch64__) || defined(_M_ARM64)")
    path.write_bytes(data)

def verify(root: Path) -> None:
    for relative, expected in EXPECTED.items():
        path = root / relative
        if not path.is_file():
            raise RuntimeError(f"Mutable source missing: {relative}")
        actual = git_blob_sha(path.read_bytes())
        if actual != expected:
            raise RuntimeError(
                f"Mutable source mismatch: {relative} {actual} != {expected}")
    print(f"Validated {len(EXPECTED)} MIYAKO-equivalent Mutable source anchors")

def main() -> None:
    root = Path(__file__).resolve().parents[1]

    eurorack_url = f"https://codeload.github.com/pichenettes/eurorack/zip/{EURORACK}"
    with download_zip(eurorack_url) as z:
        archive_root = f"eurorack-{EURORACK}/"
        extract_subtree(z, archive_root, "plaits", root)
        extract_subtree(z, archive_root, "clouds", root)

    stmlib_url = f"https://codeload.github.com/pichenettes/stmlib/zip/{STMLIB}"
    with download_zip(stmlib_url) as z:
        archive_root = f"stmlib-{STMLIB}/"
        stmlib_root = root / "Source/stmlib"
        stmlib_root.mkdir(parents=True, exist_ok=True)

        for directory in ("dsp", "fft", "utils"):
            prefix = archive_root + directory + "/"
            count = 0
            for info in z.infolist():
                if info.is_dir() or not info.filename.startswith(prefix):
                    continue
                relative = info.filename[len(archive_root):]
                target = stmlib_root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(z.read(info.filename))
                count += 1
            if count == 0:
                raise RuntimeError(f"No stmlib files extracted for {directory}")

        for name in ("LICENSE", "stmlib.h"):
            stmlib_root.joinpath(name).write_bytes(
                z.read(archive_root + name))

    patch_plaits_user_data(root)
    patch_clouds_debug_pin(root)
    patch_stmlib_aarch64(root)
    verify(root)

if __name__ == "__main__":
    main()
