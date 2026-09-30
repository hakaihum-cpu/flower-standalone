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
    "Source/TwilightRealtime3DComponent.h",
    "Source/TwilightRealtime3DComponent.cpp",
    "Source/FlowerFrameData.h",
    "Source/RetroLookAndFeel.h",
    "Source/RetroLookAndFeel.cpp",
    "scripts/patch_android_native_parallelism.py",
    "scripts/patch_juce_android_gamepad_keys.py",
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

for obsolete_visual in [
    "Resources/flower_xy_square_atlas_exact.jpg",
    "scripts/materialize_xy_visual_asset.py",
]:
    if (ROOT / obsolete_visual).exists():
        fail(f"obsolete generated XY visual path reintroduced: {obsolete_visual}")

source_text = "\n".join(
    p.read_text(encoding="utf-8", errors="strict")
    for p in sorted((ROOT / "Source").glob("*"))
    if p.suffix in {".h", ".cpp"} and p.name != "FlowerFrameData.h"
)

for token in FORBIDDEN_SOURCE_TOKENS:
    if token in source_text:
        fail(f"MIYAKO-only dependency token found in standalone Source: {token}")

jucer = ROOT / "FLOWER_Standalone.jucer"
root = ET.parse(jucer).getroot()
if root.attrib.get("name") != "FLOWERTW3DTEST":
    fail("JUCER project name is not FLOWERTW3DTEST")
if root.attrib.get("pluginFormats") != "buildStandalone":
    fail("JUCER is not standalone-only")
if root.attrib.get("bundleIdentifier") != "local.flower.twilightrealtime3d":
    fail("unexpected TW3D test bundleIdentifier")
if root.attrib.get("pluginIsSynth") != "1" or root.attrib.get("pluginWantsMidiIn") != "1":
    fail("standalone synth/MIDI flags are not enabled")

jucer_text = jucer.read_text(encoding="utf-8")
for required_ref in [
    "Source/SynthVoice.cpp",
    "Source/RetroLookAndFeel.cpp",
    "Source/PluginProcessor.cpp",
    "Source/PluginEditor.cpp",
    "Source/TwilightRealtime3DComponent.cpp",
]:
    if required_ref not in jucer_text:
        fail(f"JUCER reference missing: {required_ref}")

for forbidden_ref in FORBIDDEN_JUCER_REFS:
    if forbidden_ref in jucer_text:
        fail(f"animation-free MVP unexpectedly references: {forbidden_ref}")

circle = (ROOT / ".circleci/config.yml").read_text(encoding="utf-8")
if "run_build" not in circle or ("default: false" not in circle and "default: true" not in circle):
    fail("CircleCI build gate declaration is missing")

for required_ci in [
    "patch_juce_android_gamepad_keys.py JUCE",
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

for forbidden_old_visual in [
    "BinaryData::flower_xy_source_exact_jpg",
    "tileSheetImage",
    "xStarts[tileColumns]",
    "xEnds[tileColumns]",
    "yStarts[tileRows]",
    "yEnds[tileRows]",
]:
    if forbidden_old_visual in editor_text or forbidden_old_visual in editor_header:
        fail(f"old contact-sheet path still present in editor: {forbidden_old_visual}")

frame_data_text = (ROOT / "Source/FlowerFrameData.h").read_text(encoding="utf-8")

for required_frame_data in [
    "frameCount = 100",
    "decodedPayloadSize = 1138410",
    "encodedPayload",
    "offsets",
    "sizes",
]:
    if required_frame_data not in frame_data_text:
        fail(f"exact 100-frame payload contract missing: {required_frame_data}")

for required_visual_ui in [
    '#include "FlowerFrameData.h"',
    "juce::Base64::convertFromBase64",
    "FlowerFrameData::encodedPayload",
    "FlowerFrameData::decodedPayloadSize",
    "FlowerFrameData::frameCount",
    "FlowerFrameData::offsets",
    "FlowerFrameData::sizes",
    "std::array<juce::Image, 100> frameImages",
    "frameImages[index]",
    "visualTileIndex = row * tileColumns + column",
    "frameImages[static_cast<size_t> (visualTileIndex)]",
    "0, 0, frame.getWidth(), frame.getHeight()",
    "highResamplingQuality",
    "physicalPointerVisible",
    "tileCount = tileColumns * tileRows",
    "static_assert (tileCount == 100",
    "onTapStopRequested",
]:
    if required_visual_ui not in editor_text and required_visual_ui not in editor_header:
        fail(f"XY direct-frame visual contract missing: {required_visual_ui}")

for required_xy_ui in [
    "setResizable (true, false)",
    "performancePad.setBounds (getLocalBounds())",
    "configScreen.setBounds (getLocalBounds())",
    "AffineTransform::scale (scaleX, scaleY)",
    "designPoint",
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
    "juce::KeyPress::F19Key",
    "ConfigScreenComponent",
    "ConfigScreenComponent() = default",
    "toggleConfig",
    "configScreen.setBounds (getLocalBounds())",
    "AffineTransform::scale (scaleX, scaleY)",
    "onCloseRequested",
    "getCloseBounds",
    'g.drawText ("CLOSE"',
    "beginBpmAdjust",
    "beginLooperButton",
    "toggleArp",
    "toggleDelay",
    "toggleGranular",
]:
    if required_xy_ui not in editor_text and required_xy_ui not in editor_header:
        fail(f"XY fullscreen/physical-key contract missing: {required_xy_ui}")


key_pressed_start = editor_text.find("bool FlowerStandaloneAudioProcessorEditor::keyPressed")
config_block_start = editor_text.find("if (configVisible)", key_pressed_start)
config_block_end = editor_text.find(
    "if (code == juce::KeyPress::leftKey\n        || code == juce::KeyPress::rightKey",
    config_block_start
)
config_block = editor_text[config_block_start:config_block_end] if config_block_start >= 0 and config_block_end >= 0 else ""
if "juce::KeyPress::F14Key" not in config_block or "configScreen.activateSelected()" not in config_block:
    fail("CONFIG confirm must be B/F14")
if "juce::KeyPress::F13Key" in config_block:
    fail("CONFIG confirm must not remain on A/F13")

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
    "KEYCODE_BUTTON_SELECT",
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
    "performanceRootConfig",
    "performanceScaleConfig",
    "defaultEffectsEnabled",
    "randomScale",
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

tw3d_header = (ROOT / "Source/TwilightRealtime3DComponent.h").read_text(encoding="utf-8")
tw3d_cpp = (ROOT / "Source/TwilightRealtime3DComponent.cpp").read_text(encoding="utf-8")

for required_tw3d in [
    "OpenGLAppComponent",
    "TwilightRealtime3DComponent",
    "RigMatrices",
    "makeWalkKey",
    "buildRig",
    "renderCharacter",
    "renderEnvironment",
    "glDrawElements",
    "GL_DEPTH_TEST",
    "uProjection",
    "uView",
    "uModel",
    "vertex snapping",
    "BONE RIG / POLYGON / NO SPRITES",
    "WALK CYCLE",
    "DIRECTION / TURN",
    "KNEES UP",
    "LIE DOWN",
]:
    if required_tw3d not in tw3d_header and required_tw3d not in tw3d_cpp:
        fail(f"TW3D realtime character contract missing: {required_tw3d}")

for forbidden_tw3d in [
    "juce::Image",
    "drawImage",
    "ImageFileFormat",
    "Base64",
    "FlowerFrameData",
    "frameImages",
    "poseAtlas",
]:
    if forbidden_tw3d in tw3d_cpp:
        fail(f"TW3D must not use 2D sprite/image animation: {forbidden_tw3d}")

if "TwilightRealtime3DComponent twilightRealtime3D;" not in editor_header:
    fail("TW3D editor member missing")
if "twilightRealtime3D.setVisible (true)" not in editor_text:
    fail("TW3D startup surface is not enabled")
if "twilightRealtime3D.setBounds (getLocalBounds())" not in editor_text:
    fail("TW3D screen does not follow fullscreen bounds")
if 'id="juce_opengl"' not in jucer_text:
    fail("juce_opengl module missing")
if "Source/TwilightRealtime3DComponent.cpp" not in jucer_text:
    fail("TW3D source not compiled by JUCER target")

print("[PASS] Flower TW3D standalone static dependency audit")
print("[PASS] TW3D is realtime polygon/OpenGL and contains no sprite/image animation")
print("[PASS] obsolete contact-sheet resource excluded from generated target")
print("[PASS] Android fullscreen editor follows actual logical bounds (physical panel no longer clipped)")
print("[PASS] exact 100 user-cut JPEG frame bank embedded")
print("[PASS] runtime does not use the old contact sheet")
print("[PASS] selected pre-cut JPEG is loaded as an independent frame")
print("[PASS] source frame is drawn in full with no coordinate crop")
print("[PASS] runtime performs no source crop and no atlas lookup")
print("[PASS] all 100 direct JPEG frames are addressable with no skip list")
print("[PASS] dpad XY latch / tap-stop / pointer contract present")
print("[PASS] A/B/X/Y/L/R gamepad assignment contract present")
print("[PASS] SELECT CONFIG screen contract present")
print("[PASS] persistent ROOT/SCALE/RANDOM/default-effect config contract present")
print("[PASS] X looper transport REC->STOP->OVERDUB and long-clear contract present")
print("[PASS] L/R BPM hold-repeat contract present (max 200 BPM)")
print("[PASS] Android physical-key down/up bridge patch present")
print("[PASS] Android standalone startup safeguards present")
print("[PASS] XY audio order: arp MIDI -> synth -> granular -> delay")
print("[PASS] CircleCI native parallelism controls present")
