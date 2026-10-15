#!/usr/bin/env python3
"""Materialize pinned Mutable Instruments Clouds + stmlib sources for Android."""

from __future__ import annotations

import hashlib
from io import BytesIO
from pathlib import Path
from urllib.request import Request, urlopen
from zipfile import ZipFile

EURORACK = "08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4"
STMLIB = "e3bd7c9cc00e4364166f9905c0509b6ffd0535ec"

EXPECTED = {
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
    request = Request(url, headers={"User-Agent": "EFFECTS-Clouds-pinned-build/1"})
    with urlopen(request, timeout=120) as response:
        return ZipFile(BytesIO(response.read()))


def extract_subtree(
    archive: ZipFile,
    archive_root: str,
    subtree: str,
    root: Path,
) -> None:
    clean = subtree.strip("/")
    prefix = archive_root + clean + "/"
    count = 0
    for info in archive.infolist():
        if info.is_dir() or not info.filename.startswith(prefix):
            continue
        relative = info.filename[len(archive_root):]
        target = root / "Source" / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(archive.read(info.filename))
        count += 1

    if count == 0:
        raise RuntimeError(f"No files extracted for {subtree}")


def patch_clouds_debug_pin(root: Path) -> None:
    path = root / "Source/clouds/drivers/debug_pin.h"
    path.write_text(
        "// Android/JUCE port boundary. Replaces only the STM32 hardware debug GPIO.\n"
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
            f"Unexpected stmlib dsp.h TEST branch count: {data.count(needle)}"
        )

    data = data.replace(
        needle,
        b"#if defined(TEST) || defined(__aarch64__) || defined(_M_ARM64)",
    )
    path.write_bytes(data)


def verify(root: Path) -> None:
    for relative, expected in EXPECTED.items():
        path = root / relative
        if not path.is_file():
            raise RuntimeError(f"Mutable source missing: {relative}")

        actual = git_blob_sha(path.read_bytes())
        if actual != expected:
            raise RuntimeError(
                f"Mutable source mismatch: {relative} {actual} != {expected}"
            )

    print(f"Validated {len(EXPECTED)} pinned Mutable source anchors")


def main() -> None:
    root = Path(__file__).resolve().parents[1]

    eurorack_url = (
        f"https://codeload.github.com/pichenettes/eurorack/zip/{EURORACK}"
    )
    with download_zip(eurorack_url) as archive:
        extract_subtree(
            archive,
            f"eurorack-{EURORACK}/",
            "clouds",
            root,
        )

    stmlib_url = f"https://codeload.github.com/pichenettes/stmlib/zip/{STMLIB}"
    with download_zip(stmlib_url) as archive:
        archive_root = f"stmlib-{STMLIB}/"
        stmlib_root = root / "Source/stmlib"
        stmlib_root.mkdir(parents=True, exist_ok=True)

        for directory in ("dsp", "fft", "utils"):
            prefix = archive_root + directory + "/"
            count = 0
            for info in archive.infolist():
                if info.is_dir() or not info.filename.startswith(prefix):
                    continue
                relative = info.filename[len(archive_root):]
                target = stmlib_root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(info.filename))
                count += 1

            if count == 0:
                raise RuntimeError(f"No stmlib files extracted for {directory}")

        for name in ("LICENSE", "stmlib.h"):
            stmlib_root.joinpath(name).write_bytes(
                archive.read(archive_root + name)
            )

    patch_clouds_debug_pin(root)
    patch_stmlib_aarch64(root)
    verify(root)


if __name__ == "__main__":
    main()
