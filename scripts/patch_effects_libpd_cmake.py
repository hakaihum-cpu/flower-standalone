#!/usr/bin/env python3
"""Add upstream pinned libpd C sources to generated JUCE Android CMake 3.22.

libpd 0.16.1's own CMake requires 3.25; do not change Gradle's proven NDK.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
LIBPD = ROOT / "ThirdParty" / "libpd"
PINNED = "ba0dc63262901d658af8bbda5e619a60fa975e78"


def sources():
    cmake = LIBPD / "CMakeLists.txt"
    if not cmake.is_file() or not (LIBPD / "pure-data/src/m_pd.h").is_file():
        raise SystemExit("[FAIL] pinned libpd and Pd submodule not present")
    text = cmake.read_text(encoding="utf-8")
    result = []
    for key in ("PD_SOURCES", "LIBPD_SOURCES"):
        match = re.search(r"set\s*\(\s*" + key + r"\s+(.+?)\n\)", text, re.S)
        if match is None:
            raise SystemExit(f"[FAIL] cannot parse upstream {key} CMake sources")
        for token in match.group(1).split():
            if token.endswith(".c"):
                path = LIBPD / token
                if not path.is_file():
                    raise SystemExit(f"[FAIL] missing libpd source {path}")
                result.append(path)
    if len(result) < 65 or len(set(result)) != len(result):
        raise SystemExit(f"[FAIL] implausible upstream libpd C list ({len(result)})")
    return result


def generated_block(srcs):
    root = str(LIBPD.resolve()).replace("\\", "/")
    lines = [
        "",
        "# EFFECTS_LIBPD_BEGIN -- generated from pinned upstream source lists",
        'set(EFFECTS_PD_SOURCE_DIR "' + root + '")',
        "add_library(effects_pure_data STATIC",
    ]
    lines += ['    "' + str(p.resolve()).replace("\\", "/") + '"' for p in srcs]
    lines += [
        ")",
        "set_target_properties(effects_pure_data PROPERTIES",
        "    POSITION_INDEPENDENT_CODE ON C_STANDARD 11)",
        "target_compile_options(effects_pure_data PRIVATE",
        "    -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast)",
        "target_compile_definitions(effects_pure_data PRIVATE",
        "    PD_INTERNAL=1 PD=1 USEAPI_DUMMY=1 HAVE_UNISTD_H=1)",
        "target_include_directories(effects_pure_data PUBLIC",
        '    "$' '{EFFECTS_PD_SOURCE_DIR}/libpd_wrapper"',
        '    "$' '{EFFECTS_PD_SOURCE_DIR}/pure-data/src")',
        "target_link_libraries(effects_pure_data PUBLIC m)",
        "target_link_libraries($" "{BINARY_NAME} PRIVATE effects_pure_data)",
        "# EFFECTS_LIBPD_END",
        "",
    ]
    return "\n".join(lines)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: patch_effects_libpd_cmake.py Builds/Android/app/CMakeLists.txt")
    path = Path(sys.argv[1]).resolve()
    if path != (ROOT / "Builds/Android/app/CMakeLists.txt").resolve():
        raise SystemExit("[FAIL] refusing unexpected CMake target")
    if not path.is_file():
        raise SystemExit("[FAIL] Projucer must generate Android CMake first")
    original = path.read_text(encoding="utf-8")
    if "project(juce_jni_project)" not in original:
        raise SystemExit("[FAIL] not a JUCE Android Projucer CMake project")
    if "target_link_libraries( " + "$" "{BINARY_NAME}" not in original:
        raise SystemExit("[FAIL] expected JUCE native link target missing")
    if "EFFECTS_LIBPD_BEGIN" in original:
        raise SystemExit("[FAIL] duplicate libpd injection")
    srcs = sources()
    path.write_text(original + generated_block(srcs), encoding="utf-8")
    patched = path.read_text(encoding="utf-8")
    assert patched.count("EFFECTS_LIBPD_BEGIN") == 1
    assert patched.count("target_link_libraries($" "{BINARY_NAME} PRIVATE effects_pure_data)") == 1
    print(f"[PASS] pinned libpd native JNI link: {len(srcs)} C sources")


if __name__ == "__main__":
    main()
