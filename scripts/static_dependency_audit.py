#!/usr/bin/env python3
from __future__ import annotations

import hashlib
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]

REQUIRED = [
    "FLOWER_Standalone.jucer",
    "Source/ParameterIDs.h",
    "Source/Modulation.h",
    "Source/SynthVoice.h",
    "Source/SynthVoice.cpp",
    "Source/PluginProcessor.h",
    "Source/PluginProcessor.cpp",
    "Source/PluginEditor.h",
    "Source/PluginEditor.cpp",
    "Source/FlowerAnimationComponent.h",
    "Source/FlowerAnimationComponent.cpp",
    "Source/RetroLookAndFeel.h",
    "Source/RetroLookAndFeel.cpp",
    "Resources/flower_embedded_atlas.png",
    "Resources/flower_actor_v3_walk_student01.png",
    "scripts/patch_android_native_parallelism.py",
    "scripts/patch_android_smoke_abi.py",
]

UPSTREAM_GIT_BLOBS = {
    "Source/FlowerAnimationComponent.h": "3e64de47666541ef2487b729540c12f96d0a3ae6",
    "Source/FlowerAnimationComponent.cpp": "e7d6ba2cab824fd61e39441b0198337b9a82b6b0",
    "Source/RetroLookAndFeel.h": "84437adc6aca0db95e5eb3407901abf4af44d62a",
    "Source/RetroLookAndFeel.cpp": "8e3f3ad844427da7bc3aefd4b8a16873105da405",
    "Resources/flower_embedded_atlas.png": "ec5873bb022f6efdef9fc72c0099ca71e344887b",
    "Resources/flower_actor_v3_walk_student01.png": "d516c56bef14ae5cc2e73e755b4f221ddf2aa04d",
}

FORBIDDEN_SOURCE_TOKENS = [
    "Dx7Patch",
    "SamplerData",
    "VisualizerComponent",
    "twilightEnabled",
    "TwilightEvent",
    "ParamIDs::oscType",
    "ParamIDs::sampler",
    "NotebookFx",
]

def git_blob_sha(path: Path) -> str:
    data = path.read_bytes()
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()

def fail(message: str) -> None:
    print(f"[FAIL] {message}")
    raise SystemExit(1)

for relative in REQUIRED:
    if not (ROOT / relative).is_file():
        fail(f"required file missing: {relative}")

if (ROOT / "app").exists():
    fail("obsolete Java bootstrap app/ directory still exists")

for obsolete in ["build.gradle.kts", "settings.gradle.kts", "gradle.properties", "ci/gradle-bootstrap.sh"]:
    if (ROOT / obsolete).exists():
        fail(f"obsolete Java bootstrap file still exists: {obsolete}")

for relative, expected in UPSTREAM_GIT_BLOBS.items():
    actual = git_blob_sha(ROOT / relative)
    if actual != expected:
        fail(f"upstream-reuse file changed unexpectedly: {relative} {actual} != {expected}")

source_text = "\n".join(
    p.read_text(encoding="utf-8", errors="strict")
    for p in sorted((ROOT / "Source").glob("*"))
    if p.suffix in {".h", ".cpp"}
)

for token in FORBIDDEN_SOURCE_TOKENS:
    if token in source_text:
        fail(f"MIYAKO-only dependency token found in standalone Source: {token}")

jucer = ROOT / "FLOWER_Standalone.jucer"
root = ET.parse(jucer).getroot()
if root.attrib.get("name") != "FLOWER":
    fail("JUCER project name is not FLOWER")
if root.attrib.get("pluginFormats") != "buildStandalone":
    fail("JUCER is not standalone-only")
if root.attrib.get("bundleIdentifier") != "local.flower.standalone":
    fail("unexpected provisional bundleIdentifier")
if root.attrib.get("pluginIsSynth") != "1" or root.attrib.get("pluginWantsMidiIn") != "1":
    fail("standalone synth/MIDI flags are not enabled")

jucer_text = jucer.read_text(encoding="utf-8")
for required_ref in [
    "Source/FlowerAnimationComponent.cpp",
    "Source/PluginProcessor.cpp",
    "Source/PluginEditor.cpp",
    "Resources/flower_embedded_atlas.png",
]:
    if required_ref not in jucer_text:
        fail(f"JUCER reference missing: {required_ref}")

circle = (ROOT / ".circleci/config.yml").read_text(encoding="utf-8")
if "default: false" not in circle or "run_build" not in circle:
    fail("CircleCI manual build gate is missing")

for required_ci in [
    "patch_android_native_parallelism.py",
    "CMAKE_BUILD_PARALLEL_LEVEL=2",
    "ActiveProcessorCount=2",
    "--max-workers=2",
    "native-job-pools.txt",
]:
    if required_ci not in circle:
        fail(f"CircleCI native parallelism control missing: {required_ci}")

for required_emulator_ci in [
    "android_emulator_smoke:",
    "image: android:default",
    "system-images;android-35;google_apis;x86_64",
    "patch_android_smoke_abi.py",
    "Flower-Standalone-x86_64.apk",
    "circle-android wait-for-boot",
    "adb install -r",
    "ANR in $PACKAGE",
    "screencap -p",
    "smoke-exit-code.txt",
    "requires:\n            - android_build",
]:
    if required_emulator_ci not in circle:
        fail(f"Android emulator smoke gate missing: {required_emulator_ci}")

smoke_patcher = (ROOT / "scripts/patch_android_smoke_abi.py").read_text(encoding="utf-8")
if 'abiFilters("x86_64")' not in smoke_patcher or 'abiFilters\\("arm64-v8a"\\)' not in smoke_patcher:
    fail("Android emulator ABI patcher is incomplete")

patcher = (ROOT / "scripts/patch_android_native_parallelism.py").read_text(encoding="utf-8")
for required_patcher in [
    "-DCMAKE_JOB_POOLS:STRING=compile=2;link=1",
    "-DCMAKE_JOB_POOL_COMPILE:STRING=compile",
    "-DCMAKE_JOB_POOL_LINK:STRING=link",
]:
    if required_patcher not in patcher:
        fail(f"generated Gradle job-pool patcher missing: {required_patcher}")

print("[PASS] Flower standalone static dependency audit")
print("[PASS] MIYAKO-only code dependencies absent")
print("[PASS] Actor v3/LookAndFeel/resources remain byte-identical to extraction source")
print("[PASS] obsolete Java bootstrap removed")
editor_text = (ROOT / "Source/PluginEditor.cpp").read_text(encoding="utf-8")
for required_android_startup in [
    "#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>",
    "juce::Desktop::getInstance().setOrientationsEnabled",
    "getDisplays().getPrimaryDisplay()",
    "setResizable (true, true)",
    "juce::StandalonePluginHolder::getInstance()",
    "holder->deviceManager.closeAudioDevice()",
    "holder->deviceManager.initialise (0, 2, nullptr, true)",
    "holder->startPlaying()",
]:
    if required_android_startup not in editor_text:
        fail(f"Android standalone startup safeguard missing: {required_android_startup}")

print("[PASS] Android standalone startup safeguards present")
print("[PASS] JUCER standalone Android structure present")
