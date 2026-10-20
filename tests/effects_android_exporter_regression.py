#!/usr/bin/env python3
"""Fast isolated regression for the exact Projucer-to-Android libpd patcher."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[1]
patcher = repo / "scripts/patch_android_effects_libpd.py"
assert patcher.is_file()

with tempfile.TemporaryDirectory(prefix="effects-pd-cmake-preflight-") as td:
    root = Path(td)
    script = root / "scripts/patch_android_effects_libpd.py"
    script.parent.mkdir(parents=True)
    shutil.copyfile(patcher, script)
    cmake = root / "Builds/Android/app/CMakeLists.txt"
    gradle = root / "Builds/Android/app/build.gradle"
    cmake.parent.mkdir(parents=True)
    cmake.write_text(
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(juce_jni_project)\n"
        "set(BINARY_NAME juce_jni)\n"
        "add_library(${BINARY_NAME} SHARED dummy.cpp)\n"
        "target_link_libraries( ${BINARY_NAME} PRIVATE m)\n"
    )
    gradle.write_text(
        "plugins { id 'com.android.application' }\n"
        "android {\n"
        "    externalNativeBuild {\n"
        "        cmake { version = \"3.22.1\" }\n"
        "    }\n"
        "}\n"
    )
    hdr = root / "third_party/libpd/libpd_wrapper/z_libpd.h"
    hdr.parent.mkdir(parents=True)
    hdr.write_text("/* deliberately minimal libpd fixture */\n")

    for iteration in (1, 2):
        proc = subprocess.run(
            [sys.executable, str(script)], cwd=root,
            capture_output=True, text=True, timeout=15,
        )
        assert proc.returncode == 0, (
            f"Projucer patcher iteration {iteration} failed:\n"
            f"{proc.stdout}\n{proc.stderr}"
        )
        c = cmake.read_text()
        g = gradle.read_text()
        assert c.count("# EFFECTS_PINNED_LIBPD_LINK_BEGIN") == 1
        assert c.count("target_link_libraries(${BINARY_NAME} PRIVATE libpd_static m dl)") == 1
        assert g.count('version = "3.31.6"') == 1
        assert g.count('ndkVersion "26.1.10909125"') == 1
        assert 'version = "3.22.1"' not in g

    print("[PASS] #219/#220 Android CMake/NDK exporter regression; second pass idempotent")
