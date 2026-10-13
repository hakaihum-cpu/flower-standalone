cmake_minimum_required(VERSION 3.22.1)
project(amps_audio LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Existing CircleCI Android/NDK worker: keep Ninja parallelism bounded.
set_property(GLOBAL PROPERTY JOB_POOLS amps_compile=2)
set(CMAKE_JOB_POOL_COMPILE amps_compile)

# libusb-cmake includes a VENDORED copy of libusb, not a Git submodule.
# Immutable upstream commit for repeatable builds; NO GitHub Actions,
# new CI workflow or alternate Android signing/build machinery.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)
set(LIBUSB_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(amps_libusb
  GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
  GIT_TAG bea6567b63796ab27c1c33257d230284fb2d8316
  GIT_SHALLOW FALSE
  GIT_PROGRESS FALSE
)
FetchContent_MakeAvailable(amps_libusb)
set_target_properties(usb-1.0 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Superpowered private evaluation SDK 2.8.2 (2026-09-09); SDK binaries are
# fetched from upstream at build-time, never redistributed in this repository.
# Use AndroidUSB (actual USB host stack), not SuperpoweredAndroidAudioIO (AAudio).
FetchContent_Declare(amps_superpowered
  GIT_REPOSITORY https://github.com/superpoweredSDK/Low-Latency-Android-iOS-Linux-Windows-tvOS-macOS-Interactive-Audio-Platform.git
  GIT_TAG 7f3d2f1ec15334b2424e2a60e0af457aa3f76620
  GIT_SHALLOW FALSE
  GIT_PROGRESS FALSE
)
FetchContent_MakeAvailable(amps_superpowered)
add_library(amps_superpowered_static STATIC IMPORTED)
set_target_properties(amps_superpowered_static PROPERTIES
  IMPORTED_LOCATION "${amps_superpowered_SOURCE_DIR}/Superpowered/libSuperpoweredAndroid${ANDROID_ABI}.a"
)
# AAudio remains available solely for the explicitly selected AAudio mode.
# UsbPcmEngine.cpp is deliberately NOT linked: its legacy USBFS JNI symbol
# would collide with UsbLibusbHost.cpp and reintroduce old direct transport.
add_library(amps_audio SHARED
  AAudioEngine.cpp
  UsbIsoTester.cpp
  UsbLibusbHost.cpp
  SuperpoweredUsbEngine.cpp
)
target_compile_options(amps_audio PRIVATE -O3 -Wall -Wextra)
target_include_directories(amps_audio PRIVATE "${amps_superpowered_SOURCE_DIR}/Superpowered")
target_link_libraries(amps_audio PRIVATE aaudio log android OpenSLES libusb::usb-1.0 amps_superpowered_static)
