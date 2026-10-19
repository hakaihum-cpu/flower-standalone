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
if not (r / "third_party/libpd/libpd_wrapper/z_libpd.h").is_file():
    raise SystemExit("[FAIL] libpd sources not materialized")

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
target_link_libraries(${BINARY_NAME} PRIVATE libpd_static m)
target_include_directories(${BINARY_NAME} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/libpd/libpd_wrapper"
    "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/libpd/pure-data/src")
# EFFECTS_PINNED_LIBPD_LINK_END
"""
cm.write_text(txt)
assert txt.count(marker) == 1
print("[PASS] generated JUCE Android JNI target links pinned libpd_static")
