#!/usr/bin/env python3
"""Idempotently link pinned libpd static into Projucer Android CMake output.
Run ONLY after Projucer --resave. Android build remains CircleCI manual.
"""
from pathlib import Path

r = Path(__file__).resolve().parents[1]
cm = r / "Builds/Android/app/CMakeLists.txt"
if not cm.exists():
    raise SystemExit("[FAIL] generated Android CMakeLists missing")
txt = cm.read_text()
if "project(juce_jni_project)" not in txt:
    raise SystemExit("[FAIL] unexpected Projucer CMake project")
if "target_link_libraries( ${BINARY_NAME}" not in txt:
    raise SystemExit("[FAIL] native target link anchor absent")
# Projucer uses the plain CMake target_link_libraries signature.
# A PRIVATE signature on the same juce_jni target fails CMake (#221).
if "target_link_libraries( ${BINARY_NAME} PRIVATE" in txt:
    raise SystemExit("[FAIL] unexpected keyword link signature in generated JUCE CMake")
if not (r / "third_party/libpd/libpd_wrapper/z_libpd.h").is_file():
    raise SystemExit("[FAIL] libpd sources not materialized")
for rel in ("Source/compat/android_api24_glob.c", "Source/compat/android_api24_glob.h"):
    if not (r / rel).is_file():
        raise SystemExit(f"[FAIL] missing Android API24 libpd glob compatibility: {rel}")

marker = "# EFFECTS_PINNED_LIBPD_LINK_BEGIN"
if marker not in txt:
    txt += """
# EFFECTS_PINNED_LIBPD_LINK_BEGIN
# Use the JUCE-owned Android audio callback. libpd never opens Android I/O.
set(PD_MULTI OFF CACHE BOOL "" FORCE)
set(PD_UTILS OFF CACHE BOOL "" FORCE)
set(PD_EXTRA OFF CACHE BOOL "" FORCE)
set(LIBPD_STATIC ON CACHE BOOL "" FORCE)
set(LIBPD_SHARED OFF CACHE BOOL "" FORCE)
set(PD_BUILD_C_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/libpd"
    "${CMAKE_CURRENT_BINARY_DIR}/effects-libpd"
    EXCLUDE_FROM_ALL)
# API 24-27 bionic lacks glob()/globfree() used by upstream x_file.c.
# Preserve pinned source and the existing Android API 24 minimum.
target_sources(libpd_static PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../../Source/compat/android_api24_glob.c")
target_compile_options(libpd_static PRIVATE
    "-include"
    "${CMAKE_CURRENT_LIST_DIR}/../../../Source/compat/android_api24_glob.h")
target_link_libraries(${BINARY_NAME} libpd_static m dl)
target_include_directories(${BINARY_NAME} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/libpd/libpd_wrapper"
    "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/libpd/pure-data/src")
# EFFECTS_PINNED_LIBPD_LINK_END
"""
cm.write_text(txt)
assert txt.count(marker) == 1

# Projucer 9.0.2 writes Android CMake version 3.22.1. Pinned libpd
# requires 3.25 or newer; set Android Gradle to the exact installed
# SDK version without modifying upstream libpd source.
gradle = r / "Builds/Android/app/build.gradle"
if not gradle.is_file():
    raise SystemExit("[FAIL] generated Android build.gradle missing")
gr = gradle.read_text()
old = 'version = "3.22.1"'
new = 'version = "3.31.6"'
if gr.count(old) == 1:
    gr = gr.replace(old, new)
elif gr.count(new) != 1:
    raise SystemExit("[FAIL] unexpected Projucer Android CMake version")
# Match the installed NDK, not Gradle's automatic newest-installed NDK
# selection. #220 was configured with NDK 28 despite this CI pinning 26.
ndk = 'ndkVersion "26.1.10909125"'
has_final_newline = gr.endswith("\n")
gradle_lines = gr.splitlines()
android_blocks = [i for i, line in enumerate(gradle_lines)
                  if line.strip() == "android {"]
ndk_lines = [i for i, line in enumerate(gradle_lines)
             if line.strip().startswith("ndkVersion")]
if len(android_blocks) != 1 or len(ndk_lines) > 1:
    raise SystemExit("[FAIL] unexpected Android/NDK Gradle exporter syntax")
if ndk_lines:
    gradle_lines[ndk_lines[0]] = "    " + ndk
else:
    gradle_lines.insert(android_blocks[0] + 1, "    " + ndk)
gr = "\n".join(gradle_lines) + ("\n" if has_final_newline else "")
if gr.count(ndk) != 1:
    raise SystemExit("[FAIL] generated Gradle NDK pin is missing or duplicated")
gradle.write_text(gr)
assert gr.count(new) == 1
print("[PASS] generated JUCE Android JNI links pinned libpd_static, CMake 3.31.6, NDK 26.1.10909125")
