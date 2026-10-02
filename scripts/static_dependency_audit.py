#!/usr/bin/env python3
from pathlib import Path
import csv, hashlib, sys, xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[1]
JUCER = R / "RealtimeChordFX_Standalone.jucer"

EXPECTED_MODULES = {
    "juce_audio_basics",
    "juce_audio_devices",
    "juce_audio_formats",
    "juce_audio_plugin_client",
    "juce_audio_processors",
    "juce_audio_processors_headless",
    "juce_audio_utils",
    "juce_core",
    "juce_data_structures",
    "juce_dsp",
    "juce_events",
    "juce_graphics",
    "juce_gui_basics",
    "juce_gui_extra",
}

JUCE_9_0_2_DEPS = {
    "juce_audio_basics": {"juce_core"},
    "juce_audio_devices": {"juce_audio_basics", "juce_events"},
    "juce_audio_formats": {"juce_audio_basics"},
    "juce_audio_plugin_client": {"juce_audio_processors"},
    "juce_audio_processors": {"juce_gui_extra", "juce_audio_processors_headless"},
    "juce_audio_processors_headless": {"juce_audio_basics", "juce_events"},
    "juce_audio_utils": {"juce_audio_processors", "juce_audio_formats", "juce_audio_devices"},
    "juce_core": set(),
    "juce_data_structures": {"juce_events"},
    "juce_dsp": {"juce_audio_formats"},
    "juce_events": {"juce_core"},
    "juce_graphics": {"juce_events"},
    "juce_gui_basics": {"juce_graphics", "juce_data_structures"},
    "juce_gui_extra": {"juce_gui_basics"},
}

def fail(msg):
    raise SystemExit(f"[FAIL] {msg}")

required = [
    "RealtimeChordFX_Standalone.jucer",
    "Source/PluginProcessor.cpp", "Source/PluginEditor.cpp",
    "Source/TheoryEngine.cpp", "Source/YinPitchDetector.cpp",
    "Source/GranularPitchBank.cpp", "Source/FramePack.cpp",
    "Resources/classroom_frames.pack",
    "Resources/classroom_frames_manifest.csv",
    "PROJECT_CONTRACT.md", "BUILD_HISTORY.csv",
    "scripts/materialize_frame_pack.py",
    "scripts/patch_android_native_parallelism.py",
    "scripts/patch_juce_android_gamepad_keys.py",
    "tests/core_test.cpp",
]
for rel in required:
    if not (R / rel).is_file():
        fail(f"missing {rel}")

# Exact user-supplied visual bank.
rows = list(csv.DictReader((R / "Resources/classroom_frames_manifest.csv").open(encoding="utf-8")))
if len(rows) != 300:
    fail(f"expected 300 supplied frames, got {len(rows)}")
pack = (R / "Resources/classroom_frames.pack").read_bytes()
if len(pack) != 4464088:
    fail(f"frame pack size mismatch: {len(pack)}")
sha = hashlib.sha256(pack).hexdigest()
if sha != "1e888dbed69e259c52d2cb2bd192faa5c7f29dcf76eafcda9fdb2a2d03f2d658":
    fail(f"frame pack sha256 mismatch: {sha}")
if pack[:4] != b"CRF1":
    fail("frame pack magic mismatch")

# Product isolation / no synth implementation.
target_source_files = [
    "Source/FramePack.cpp","Source/FramePack.h",
    "Source/GranularPitchBank.cpp","Source/GranularPitchBank.h",
    "Source/ParameterIDs.h",
    "Source/PluginEditor.cpp","Source/PluginEditor.h",
    "Source/PluginProcessor.cpp","Source/PluginProcessor.h",
    "Source/TheoryEngine.cpp","Source/TheoryEngine.h",
    "Source/YinPitchDetector.cpp","Source/YinPitchDetector.h",
]
src = "\n".join((R / name).read_text(errors="ignore") for name in target_source_files)
for bad in ["SynthVoice", "juce::Synthesiser", "SineVoice", "FLOWER", "MIYAKO"]:
    if bad in src:
        fail(f"forbidden cross-project/synth token in product source: {bad}")
for need in ["COMPLEX","BAR","WIDTH","LENGTH","iRig Streamer","DETECTED","● REC",
             "MIDI CONTROL","MIDI SETTINGS","MIDI OUT","X MODE","Y MODE","PRESET","MOTION BARS","MOTION REC","MOTION PLAY","CHORD OUT","CHORD CH"]:
    if need not in src:
        fail(f"UI/requirement token missing: {need}")

editor = (R / "Source/PluginEditor.cpp").read_text()
if "isKeyCurrentlyDown" in editor:
    fail("Android Motion REC must not rely on JUCE Android isKeyCurrentlyDown()")
for need in [
    "#include <juce_core/native/juce_JNIHelpers_android.h>",
    "getAndroidPhysicalInputNames",
    'name.containsIgnoreCase ("RG Rotate")',
    "getProductName",
    "GET_DEVICES_INPUTS",
    "setResizable (true, false)",
    "initialise (1, 2, nullptr, true)",
    "closeAudioDevice",
    "startPlaying",
    "setWantsKeyboardFocus (true)",
    "grabKeyboardFocus",
    "keyPressed",
    "keyStateChanged",
    "KeyPress::F17Key",
    "KeyPress::F18Key",
]:
    if need not in editor:
        fail(f"Android input/startup contract missing: {need}")

motion = (R / "Source/PluginProcessor.cpp").read_text()
for need in [
    "motionTicksPerBar = 96",
    "maxMotionBars = 16",
    "processMotionTick",
    "processInternalMotionClock",
    "ParamID::motionBars",
]:
    if need not in src and need not in motion:
        fail(f"Motion REC contract missing: {need}")

chord_midi = (R / "Source/PluginProcessor.cpp").read_text()
for need in [
    "ParamID::chordMidiOut",
    "ParamID::chordMidiChannel",
    "processChordMidi",
    "stopActiveChordMidi",
    "chordMidiGateOpen",
    "chordMidiRefreshRequested",
    "chordMidiStopRequested",
]:
    if need not in src and need not in chord_midi:
        fail(f"Chord MIDI contract missing: {need}")

if 'std::make_unique<juce::AudioParameterBool> (ParamID::chordMidiOut, "CHORD MIDI OUT", false)' not in chord_midi:
    fail("CHORD MIDI OUT must default OFF")

gamepad_patch = (R / "scripts/patch_juce_android_gamepad_keys.py").read_text()
for need in [
    "KEYCODE_BUTTON_L1",
    "KeyPress::F17Key",
    "KEYCODE_BUTTON_R1",
    "KeyPress::F18Key",
    "handleKeyUpOrDown (true)",
    "handleKeyUpOrDown (false)",
]:
    if need not in gamepad_patch:
        fail(f"physical-key bridge contract missing: {need}")

# JUCER: exact module set proven by FLOWER Golden and complete JUCE 9.0.2 dependency closure.
root = ET.parse(JUCER).getroot()
if root.attrib.get("pluginFormats") != "buildStandalone":
    fail("standalone-only contract broken")
if root.attrib.get("pluginIsSynth") != "0":
    fail("synth flag must be off")
if root.attrib.get("pluginWantsMidiIn") != "1":
    fail("MIDI input required")
if root.attrib.get("pluginProducesMidiOut") != "1":
    fail("MIDI controller output must be enabled")
if root.attrib.get("pluginChannelConfigs") != "{1,2}":
    fail("expected mono input / stereo output channel config {1,2}")

android = root.find("./EXPORTFORMATS/ANDROIDSTUDIO")
if android is None:
    fail("Android exporter missing")
if android.attrib.get("androidMinimumSDK") != "24":
    fail("Android min SDK drift")
if android.attrib.get("microphonePermissionNeeded") != "1":
    fail("Android RECORD_AUDIO manifest permission not enabled")

modules_node = root.find("./MODULES")
module_paths_node = android.find("./MODULEPATHS")
if modules_node is None or module_paths_node is None:
    fail("JUCER module nodes missing")
mods = {n.attrib.get("id") for n in modules_node.findall("MODULE")}
paths = {n.attrib.get("id") for n in module_paths_node.findall("MODULEPATH")}
if mods != EXPECTED_MODULES:
    fail(f"MODULES differs from proven Golden set: missing={sorted(EXPECTED_MODULES-mods)} extra={sorted(mods-EXPECTED_MODULES)}")
if paths != EXPECTED_MODULES:
    fail(f"MODULEPATHS differs from proven Golden set: missing={sorted(EXPECTED_MODULES-paths)} extra={sorted(paths-EXPECTED_MODULES)}")
for module in sorted(mods):
    missing = JUCE_9_0_2_DEPS[module] - mods
    if missing:
        fail(f"JUCE 9.0.2 dependency closure broken for {module}: {sorted(missing)}")

jucer_text = JUCER.read_text()
if "Resources/classroom_frames.pack" not in jucer_text:
    fail("supplied frame pack not embedded")
for bad_ref in ["FLOWER_Standalone.jucer","SynthVoice","Carnival","flower_"]:
    if bad_ref in jucer_text:
        fail(f"cross-project JUCER reference found: {bad_ref}")

# CI: preserve the known-good AN-10..AN-15 controls instead of simplifying them.
ci = (R / ".circleci/config.yml").read_text()
for need in [
    "run_build:", "default: false",
    "Materialize exact supplied frame pack",
    "no_output_timeout: 30m",
    "CMAKE_BUILD_PARALLEL_LEVEL=2",
    "--max-workers=2",
    "-XX:ActiveProcessorCount=2",
    "-DCMAKE_JOB_POOLS:STRING=compile=2;link=1",
    "-DCMAKE_JOB_POOL_COMPILE:STRING=compile",
    "-DCMAKE_JOB_POOL_LINK:STRING=link",
    "[heartbeat]",
    "memory.events",
    "clang_processes=",
    "SOURCE_COMMIT.txt",
    "Propagate Gradle build result",
]:
    if need not in ci:
        fail(f"known-good CircleCI control missing: {need}")

patch = (R / "scripts/patch_android_native_parallelism.py").read_text()
for need in [
    "-DCMAKE_JOB_POOLS:STRING=compile=2;link=1",
    "-DCMAKE_JOB_POOL_COMPILE:STRING=compile",
    "-DCMAKE_JOB_POOL_LINK:STRING=link",
    "expected exactly one defaultConfig CMake arguments line",
    "partial CMake job-pool injection detected",
]:
    if need not in patch:
        fail(f"known-good native parallelism patch drift: {need}")

print("[PASS] exact 300-frame visual bank SHA/size verified")
print("[PASS] product source isolated; synth/cross-project tokens absent")
print("[PASS] Android input/JNI/startup contract verified")
print("[PASS] JUCER module set matches proven FLOWER Golden")
print("[PASS] JUCE 9.0.2 dependency closure verified")
print("[PASS] Android RECORD_AUDIO exporter contract verified")
print("[PASS] known-good AN-10..AN-15 CircleCI controls preserved")
