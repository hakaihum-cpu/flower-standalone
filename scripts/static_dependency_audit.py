#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]

# FLOWER XY MVP deliberately excludes the retired animation/PNG path from the
# generated target.  Audit only dependencies required by this branch.
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
    "Source/RetroLookAndFeel.h",
    "Source/RetroLookAndFeel.cpp",
    "scripts/patch_android_native_parallelism.py",
    "scripts/materialize_xy_visual_asset.py",
    "scripts/patch_juce_android_gamepad_keys.py",
    "Resources/xy_sheet_01.b64.part00",
    "Resources/xy_sheet_01.b64.part01",
    "Resources/xy_sheet_01.b64.part02",
    "Resources/xy_sheet_01.b64.part03",
    "Resources/xy_sheet_01.b64.part04",
    "Resources/xy_sheet_01.b64.part05",
    "Resources/xy_sheet_01.b64.part06",
    "Resources/xy_sheet_01.b64.part07",
    "Resources/xy_sheet_01.b64.part08",
]

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

FORBIDDEN_JUCER_REFS = [
    "Source/FlowerAnimationComponent.cpp",
    "Source/FlowerAnimationComponent.h",
    "Resources/flower_embedded_atlas.png",
    "Resources/flower_actor_v3_walk_student01.png",
]

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
    "Source/SynthVoice.cpp",
    "Source/RetroLookAndFeel.cpp",
    "Source/PluginProcessor.cpp",
    "Source/PluginEditor.cpp",
    "Resources/flower_xy_sheet_01.jpg",
]:
    if required_ref not in jucer_text:
        fail(f"JUCER reference missing: {required_ref}")

for forbidden_ref in FORBIDDEN_JUCER_REFS:
    if forbidden_ref in jucer_text:
        fail(f"animation-free MVP unexpectedly references: {forbidden_ref}")

circle = (ROOT / ".circleci/config.yml").read_text(encoding="utf-8")
if "default: false" not in circle or "run_build" not in circle:
    fail("CircleCI manual build gate is missing")

for required_ci in [
    "patch_juce_android_gamepad_keys.py JUCE",
    "materialize_xy_visual_asset.py",
    "test -s Resources/flower_xy_sheet_01.jpg",
    "patch_android_native_parallelism.py",
    "CMAKE_BUILD_PARALLEL_LEVEL=2",
    "ActiveProcessorCount=2",
    "--max-workers=2",
    "native-job-pools.txt",
]:
    if required_ci not in circle:
        fail(f"CircleCI native parallelism control missing: {required_ci}")

patcher = (ROOT / "scripts/patch_android_native_parallelism.py").read_text(encoding="utf-8")
for required_patcher in [
    "-DCMAKE_JOB_POOLS:STRING=compile=2;link=1",
    "-DCMAKE_JOB_POOL_COMPILE:STRING=compile",
    "-DCMAKE_JOB_POOL_LINK:STRING=link",
]:
    if required_patcher not in patcher:
        fail(f"generated Gradle job-pool patcher missing: {required_patcher}")

editor_header = (ROOT / "Source/PluginEditor.h").read_text(encoding="utf-8")
editor_text = (ROOT / "Source/PluginEditor.cpp").read_text(encoding="utf-8")
processor_text = (ROOT / "Source/PluginProcessor.cpp").read_text(encoding="utf-8")

if "juce::Desktop::getInstance().setOrientationsEnabled" in editor_text:
    fail("Android orientation must not be forced during editor startup")

for forbidden_visual_token in [
    "FlowerAnimationComponent",
    "flowerAnimation",
    "decodeAndroidPngWithStb",
    "STB_IMAGE_IMPLEMENTATION",
]:
    if forbidden_visual_token in editor_header or forbidden_visual_token in editor_text:
        fail(f"animation/image path leaked into XY editor: {forbidden_visual_token}")

for forbidden_visible_control in [
    "juce::TextButton",
    "juce::ComboBox",
    "juce::Slider",
]:
    if forbidden_visible_control in editor_header:
        fail(f"fullscreen XY MVP contains visible control declaration: {forbidden_visible_control}")

for required_visual_ui in [
    "BinaryData::flower_xy_sheet_01_jpg",
    "BinaryData::flower_xy_sheet_01_jpgSize",
    "tileColumns = 10",
    "tileRows = 10",
    "(1.0f - yValue)",
    "VISUAL ASSET ERROR",
    "highResamplingQuality",
    "physicalPointerVisible",
    "destWidth",
    "destHeight",
    "faceZoom = 1.15f",
    "onTapStopRequested",
]:
    if required_visual_ui not in editor_text and required_visual_ui not in editor_header:
        fail(f"XY tile visual contract missing: {required_visual_ui}")

for required_xy_ui in [
    "constexpr int canvasSize = 720",
    "setSize (canvasSize, canvasSize)",
    "setResizable (false, false)",
    "setResizeLimits (canvasSize, canvasSize, canvasSize, canvasSize)",
    "performancePad.setBounds (getLocalBounds())",
    "bool FlowerStandaloneAudioProcessorEditor::keyPressed",
    "juce::KeyPress::leftKey",
    "juce::KeyPress::rightKey",
    "juce::KeyPress::upKey",
    "juce::KeyPress::downKey",
    "beginDpadControl",
    "endDpadControl",
    "keyStateChanged",
    "nudgeFromPhysicalKey",
    "juce::KeyPress::F13Key",
    "juce::KeyPress::F14Key",
    "juce::KeyPress::F15Key",
    "juce::KeyPress::F16Key",
    "juce::KeyPress::F17Key",
    "juce::KeyPress::F18Key",
    "beginBpmAdjust",
    "beginLooperButton",
    "toggleArp",
    "toggleDelay",
    "toggleGranular",
]:
    if required_xy_ui not in editor_text:
        fail(f"XY fullscreen/physical-key contract missing: {required_xy_ui}")

for required_android_startup in [
    "#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>",
    "juce::StandalonePluginHolder::getInstance()",
    "holder->deviceManager.closeAudioDevice()",
    "holder->deviceManager.initialise (0, 2, nullptr, true)",
    "holder->startPlaying()",
]:
    if required_android_startup not in editor_text:
        fail(f"Android standalone startup safeguard missing: {required_android_startup}")

android_key_patch = (ROOT / "scripts/patch_juce_android_gamepad_keys.py").read_text(encoding="utf-8")
for required_key_patch in [
    "KEYCODE_BUTTON_A",
    "KEYCODE_BUTTON_B",
    "KEYCODE_BUTTON_X",
    "KEYCODE_BUTTON_Y",
    "KEYCODE_BUTTON_L1",
    "KEYCODE_BUTTON_R1",
    "handleKeyUpOrDown (true)",
    "handleKeyUpOrDown (false)",
]:
    if required_key_patch not in android_key_patch:
        fail(f"Android physical-key bridge patch missing: {required_key_patch}")

for required_engine in [
    "generatePerformanceMidi (midiMessages",
    "synthesiser.renderNextBlock",
    "processFlower (buffer)",
    "processPerformanceDelay (buffer)",
    "setPerformancePad",
    "setPerformanceHold",
    "setPerformanceArpEnabled",
    "setPerformanceDelayEnabled",
    "setPerformanceGranularEnabled",
    "cycleFlowerTransport",
    "performanceArpEnabled",
    "performanceDelayEnabled",
    "performanceGranularEnabled",
    "flowerTransportState",
    "constexpr int stepsPerBeat = 2",
    "juce::jlimit (50.0f, 200.0f, bpm)",
]:
    if required_engine not in processor_text:
        fail(f"XY performance engine contract missing: {required_engine}")

order = [
    processor_text.find("generatePerformanceMidi (midiMessages"),
    processor_text.find("synthesiser.renderNextBlock"),
    processor_text.find("processFlower (buffer)"),
    processor_text.find("processPerformanceDelay (buffer)"),
]
if any(position < 0 for position in order) or order != sorted(order):
    fail("XY audio order must be arp MIDI -> synth -> granular -> delay")

print("[PASS] Flower XY standalone static dependency audit")
print("[PASS] animation code/resources excluded from generated MVP target")
print("[PASS] 720x720 fullscreen XY pad contract present")
print("[PASS] 10x10 XY contact-sheet visual contract present")
print("[PASS] visual asset materializer is wired before Projucer")
print("[PASS] face-centred fullscreen visual contract present")
print("[PASS] dpad XY latch / tap-stop / pointer contract present")
print("[PASS] A/B/X/Y/L/R gamepad assignment contract present")
print("[PASS] X looper transport REC->STOP->OVERDUB and long-clear contract present")
print("[PASS] L/R BPM hold-repeat contract present (max 200 BPM)")
print("[PASS] Android physical-key down/up bridge patch present")
print("[PASS] Android standalone startup safeguards present")
print("[PASS] XY audio order: arp MIDI -> synth -> granular -> delay")
print("[PASS] CircleCI native parallelism controls present")
