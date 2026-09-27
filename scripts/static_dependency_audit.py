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
    "Source/third_party/stb_image.h",
    "Resources/flower_embedded_atlas.png",
    "Resources/flower_actor_v3_walk_student01.png",
    "scripts/patch_android_native_parallelism.py",
]

UPSTREAM_GIT_BLOBS = {
    "Source/FlowerAnimationComponent.h": "ee1bd69db24162b1119fb2ebece1ee6fe9220618",
    "Source/FlowerAnimationComponent.cpp": "f2ad3206d7228ab0f406fcc4510e041029b9fd32",
    "Source/RetroLookAndFeel.h": "84437adc6aca0db95e5eb3407901abf4af44d62a",
    "Source/RetroLookAndFeel.cpp": "8e3f3ad844427da7bc3aefd4b8a16873105da405",
    "Source/third_party/stb_image.h": "9eedabedc45b3e6fd88fae6f14a160b4d53272ec",
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
print("[PASS] Flower visual sources/resources match the AN-21 stb-decoder snapshot")
print("[PASS] obsolete Java bootstrap removed")
editor_text = (ROOT / "Source/PluginEditor.cpp").read_text(encoding="utf-8")
if "juce::Desktop::getInstance().setOrientationsEnabled" in editor_text:
    fail("AN-20 must not force Android orientation during editor startup")

for forbidden_an21_worker in [
    "std::thread",
    "detach()",
    "AsyncVisualLoadState",
]:
    if forbidden_an21_worker in editor_text:
        fail(f"unsafe AN-21 background visual loader still present: {forbidden_an21_worker}")

for required_an21_deferred_load in [
    "androidVisualLoadAttempted",
    "androidStartupTicks >= 4",
    "isShowing()",
    "decodeAndroidPngWithStb",
    "flowerAnimation.loadDecodedAtlas",
    "flowerAnimation.loadDecodedHighResWalkStrip",
]:
    if required_an21_deferred_load not in editor_text:
        fail(f"AN-21 post-window visual load guard missing: {required_an21_deferred_load}")

for forbidden_fixed_logical_720 in [
    "setResizeLimits (androidCanvasSize, androidCanvasSize",
    "constexpr int androidCanvasSize = 720",
]:
    if forbidden_fixed_logical_720 in editor_text:
        fail(f"AN-22 must not force a 720-logical-pixel Android editor: {forbidden_fixed_logical_720}")

for forbidden_editor_audio_reinit in [
    "juce::StandalonePluginHolder::getInstance()",
    "holder->deviceManager.closeAudioDevice()",
    "holder->deviceManager.initialise (0, 2, nullptr, true)",
    "holder->startPlaying()",
]:
    if forbidden_editor_audio_reinit in editor_text:
        fail(f"AN-30 editor-side Android audio reinitialisation reintroduced: {forbidden_editor_audio_reinit}")

for required_android_startup in [
    "logicalCanvasSide",
    "720.0 / juce::jmax",
    "display->scale",
    "display->userBounds.getWidth()",
    "display->userBounds.getHeight()",
    "setSize (logicalCanvasSide, logicalCanvasSide)",
    "setResizable (false, false)",
    "#define STB_IMAGE_IMPLEMENTATION",
    "#include \"third_party/stb_image.h\"",
    "decodeAndroidPngWithStb",
    "flowerAnimation.loadDecodedAtlas",
    "flowerAnimation.loadDecodedHighResWalkStrip",
]:
    if required_android_startup not in editor_text:
        fail(f"Android standalone startup safeguard missing: {required_android_startup}")

for required_720_ui in [
    "androidReferencePixels = 720.0f",
    "androidReferenceScale()",
    "static_cast<float> (juce::jmin (getWidth(), getHeight())) / androidReferencePixels",
    "auto square = getLocalBounds().withSizeKeepingCentre",
    "flowerPanel.setBounds (square.reduced (px (8.0f)))",
    "auto animationArea = flower.removeFromTop (px (336.0f))",
    "auto waveformArea = flower.removeFromTop (px (84.0f))",
    "placeFour (flowerRow1",
    "flowerPosition, flowerSize, flowerDensity, flowerSpread",
    "placeFour (flowerRow2",
    "flowerHold, flowerPitch, flowerMix, flowerFeedback",
    "auto keyboardArea = synth.removeFromBottom (px (118.0f))",
    "androidMetric (72.0f), androidMetric (20.0f)",
    "11.0f * scale",
]:
    if required_720_ui not in editor_text:
        fail(f"AN-22 fixed-720 UI guard missing: {required_720_ui}")

animation_text = (ROOT / "Source/FlowerAnimationComponent.cpp").read_text(encoding="utf-8")
for required_an24_highres in [
    "deriveHighResActorCoreFromPrimary()",
    "makeHighResIdentityVariant",
    "highResWalkRightReady[static_cast<size_t> (actor)] = true",
    "highResWalkLeftReady[static_cast<size_t> (actor)] = true",
    "frame = (actorTick + state.phaseOffset) % walkFrameCount",
    "drawHighResPoseVariation",
]:
    if required_an24_highres not in animation_text:
        fail(f"AN-24 high-resolution actor core guard missing: {required_an24_highres}")

if "((actorTick + state.phaseOffset) / 2) % walkFrameCount" in animation_text:
    fail("AN-24 walk animation must not retain the old 8 fps held-frame cadence")

print("[PASS] Android standalone startup safeguards present")
print("[PASS] JUCER standalone Android structure present")
