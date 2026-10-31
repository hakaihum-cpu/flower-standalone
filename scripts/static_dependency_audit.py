#!/usr/bin/env python3
from pathlib import Path
import base64, csv, hashlib, re, sys, xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[1]
JUCER = R / "RealtimeChordFX_Standalone.jucer"
MODE_CHOICE_TOKEN = '"MODE", juce::StringArray { "CHORD", "DREAMY", "EUREKA", "CHORDBOT", "CLOUDS", "ULTRASHEER", "ROSETONES", "SOLARFAZE", "POLYPHRASE", "PATCH", "MOOD MKII" }, 0'

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
    "Source/PluginProcessor.cpp", "Source/PluginEditor.cpp", "Source/ChordRenderers.h",
    "Source/HazeProcessor.h", "Source/EffectsCloudsEngine.h",
    "Source/VongonInspiredProcessor.h", "Source/EffectsPureDataEngine.h",
    "scripts/materialize_effects_libpd.sh", "scripts/patch_android_effects_libpd.py",
    "patches/Effects_Simple_Stereo_Delay.pd",
    "patches/Effects_MIDI_XY_Transport_Test_Synth.pd",
    "Source/TheoryEngine.cpp", "Source/YinPitchDetector.cpp",
    "Source/VideoInteraction.h",
    "Source/GranularPitchBank.cpp", "Source/PsolaHarmonyBank.h", "Source/FramePack.cpp",
    "Resources/classroom_frames.pack",
    "Resources/classroom_frames_manifest.csv",
    "Resources/eureka_frames.pack",
    "Resources/vongon_frames.pack",
    "Resources/vongon_frames_sources.txt",
    "Resources/effects_app_icon.jpg",
    "Resources/effects_app_icon.b64",
    "PROJECT_CONTRACT.md", "BUILD_HISTORY.csv", "THIRD_PARTY_NOTICES.md",
    "scripts/materialize_frame_pack.py",
    "scripts/materialize_effects_clouds.py",
    "scripts/patch_android_native_parallelism.py",
    "scripts/patch_juce_android_gamepad_keys.py",
    "tests/effects_pd_static_check.py", "tests/effects_pd_audio_regression.cpp",
    "tests/effects_pd_midi_xy_regression.cpp",
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

eureka_chunks = sorted((R / "Resources/eureka_frame_chunks").glob("*.b64"))
if len(eureka_chunks) != 21:
    fail(f"expected 21 EUREKA source chunks, got {len(eureka_chunks)}")

# The four original clips must survive byte-for-byte; seven new clips append
# 252 ordered frames without replacing the original EUREKA source bank.
eureka_original = base64.b64decode(''.join(
    chunk.read_text().strip()
    for chunk in eureka_chunks), validate=True)
if len(eureka_original) != 1872671 or hashlib.sha256(eureka_original).hexdigest() != (
        "827b5aa6a339927cb909a5ffe1b129513d946911cc78ea96d34aa110e6cd1883"):
    fail("original four EUREKA clips changed")

eureka_added_chunks = sorted(
    (R / "Resources/eureka_added_frame_chunks").glob("part_*.b64"))
if len(eureka_added_chunks) != 30:
    fail(f"expected 30 additional EUREKA clip chunks, got {len(eureka_added_chunks)}")
eureka_added = base64.b64decode(''.join(
    chunk.read_text().strip()
    for chunk in eureka_added_chunks), validate=True)
if len(eureka_added) != 2475138 or hashlib.sha256(eureka_added).hexdigest() != (
        "be0772700a74386c87815f53949e7e925b36e93259397ff282ac300bf6eb0b0b"):
    fail("new EUREKA user-video asset digest mismatch")
eureka_sizes = [
    int(x) for x in (R / "Resources/eureka_added_frame_sizes.txt").read_text().splitlines()
    if x.strip()]
if len(eureka_sizes) != 252 or sum(eureka_sizes) != 2475138:
    fail("additional EUREKA frame-size index mismatch")
source_manifest = (R / "Resources/eureka_added_sources.txt").read_text(encoding="utf-8")
for source in [
    "Person_walking_and_smiling_20261017222551.mp4",
    "Woman_leaning_against_classroom_…_20261017222853.mp4",
    "Woman_putting_on_glasses_20261017222903.mp4",
    "Woman_reading_book_in_library_20261017222903.mp4",
    "Woman_standing_in_school_hallway_20261017222145.mp4",
    "Woman_standing_on_school_rooftop_20261017222546.mp4",
    "Woman_watching_cat_near_gate_20261017222554.mp4",
]:
    if source not in source_manifest:
        fail(f"EUREKA video source missing: {source}")

# Added five user MP4 clips must remain complete and separately verifiable.
# Earlier 11 clips (144 original + 252 appended frames) are immutable.
eureka_more_chunks = sorted(
    (R / "Resources/eureka_more_frame_chunks").glob("part_*.b64"))
if len(eureka_more_chunks) != 31:
    fail(f"expected 31 EUREKA five-clip chunks, got {len(eureka_more_chunks)}")
eureka_more = base64.b64decode(''.join(
    chunk.read_text().strip()
    for chunk in eureka_more_chunks), validate=True)
if len(eureka_more) != 2496161 or hashlib.sha256(eureka_more).hexdigest() != (
        "eea7aae42c68680718cbb236c625f164b405f551a13ba1926e5a6b98d96eb27e"):
    fail("five newly supplied EUREKA MP4 frames missing or altered")
eureka_more_sizes = [
    int(x) for x in (R / "Resources/eureka_more_frame_sizes.txt").read_text().splitlines()
    if x.strip()]
if len(eureka_more_sizes) != 180 or sum(eureka_more_sizes) != len(eureka_more):
    fail("five newly supplied EUREKA JPEG frame index mismatch")
more_sources = (R / "Resources/eureka_more_sources.txt").read_text(encoding="utf-8")
for source in [
    "436441182_1792311285651534.mp4",
    "1.mp4",
    "2.mp4",
    "881269787_1792311671806683.mp4",
    "131027485_1792311397114360.mp4",
]:
    if source not in more_sources:
        fail(f"new EUREKA MP4 source missing: {source}")
eureka_pack = (R / "Resources/eureka_frames.pack").read_bytes()
# Two square user videos replace the two portrait slots only.
replacement_chunks = sorted(
    (R / "Resources/eureka_replacement_frame_chunks").glob("part_*.b64"))
if len(replacement_chunks) != 20:
    fail(f"EUREKA square 720x720 source must contain 20 chunks, got {len(replacement_chunks)}")
replacement = base64.b64decode(''.join(
    chunk.read_text().strip() for chunk in replacement_chunks), validate=True)
if len(replacement) != 1613885 or hashlib.sha256(replacement).hexdigest() != (
        "fb95820d9cdd35b8ea4f197aa87a35af538dfd4421c05be691d58cd794eeff0d"):
    fail("two new EUREKA square video files are missing or altered")
replacement_sizes = [
    int(x) for x in (R / "Resources/eureka_replacement_frame_sizes.txt")
    .read_text().splitlines() if x.strip()]
if len(replacement_sizes) != 72 or sum(replacement_sizes) != len(replacement):
    fail("EUREKA video swap must have 72 correctly indexed JPEG frames")
# The two earlier portrait slots represented 266088 and 288551 bytes.
expected_pack_size = 6849154 - (266088 + 288551) + 1613885
if len(eureka_pack) != expected_pack_size:
    fail(f"complete EUREKA frame pack size mismatch: {len(eureka_pack)}")
if eureka_pack[:4] != b"CRF1":
    fail("EUREKA frame pack magic mismatch")
eureka_count = int.from_bytes(eureka_pack[4:8], "little")
if eureka_count != 576:
    fail(f"EUREKA frame count mismatch: {eureka_count}")
old_table_end = 8 + 144 * 12
new_table_end = 8 + 576 * 12
orig_payload = eureka_original[old_table_end:]
existing_payload = orig_payload + eureka_added
if eureka_pack[new_table_end:new_table_end + len(existing_payload)] != existing_payload:
    fail("previous 11 EUREKA clips were modified")
# Compare every one of the five added clips in output: 3 untouched clips
# must be byte-for-byte identical, and ONLY the 2 square slots change.
# The existing CI materializer also rejects any JPEG not exactly 720x720.
materializer = (R / "scripts/materialize_frame_pack.py").read_text()
for need in [
    "EUREKA_SWAP_PARTS = 20",
    "EUREKA_SWAP_BYTES = 1613885",
    "jpeg_dimensions(frame) != (720, 720)",
]:
    if need not in materializer:
        fail(f"EUREKA 720x720 aspect / no-black-border contract missing: {need}")
old_offsets = [0]
for size in eureka_more_sizes:
    old_offsets.append(old_offsets[-1] + size)
swap_offsets = [0]
for size in replacement_sizes:
    swap_offsets.append(swap_offsets[-1] + size)
for clip_frame in range(180):
    index = 396 + clip_frame
    pos = 8 + index * 12
    off = int.from_bytes(eureka_pack[pos:pos + 4], "little")
    size = int.from_bytes(eureka_pack[pos + 8:pos + 12], "little")
    frame = eureka_pack[off:off + size]
    if 36 <= clip_frame < 108:
        swap_idx = clip_frame - 36
        expected = replacement[swap_offsets[swap_idx]:swap_offsets[swap_idx + 1]]
    else:
        expected = eureka_more[old_offsets[clip_frame]:old_offsets[clip_frame + 1]]
    if frame != expected:
        fail(f"EUREKA frame {index} differs from selected videos")
if "162265794_1792312586022108.mp4" in more_sources or (
        "652788400_1792312226633408.mp4" in more_sources):
    fail("old portrait files still identified as EUREKA sources")
if eureka_count != 36 * (4 + 7 + 5):
    fail("EUREKA clip boundary count differs from 16 x 36")
# Inspect offsets and verify that every packed frame is a complete JPEG.
for frame_idx in range(eureka_count):
    pos = 8 + frame_idx * 12
    offset = int.from_bytes(eureka_pack[pos:pos + 4], "little")
    size = int.from_bytes(eureka_pack[pos + 8:pos + 12], "little")
    if offset < new_table_end or size < 64 or offset + size > len(eureka_pack):
        fail(f"EUREKA frame table entry invalid: {frame_idx}")
    if eureka_pack[offset:offset+2] != bytes([255,216]) or eureka_pack[offset+size-2:offset+size] != bytes([255,217]):
        fail(f"EUREKA JPEG integrity failure: {frame_idx}")

# Vongon's original full-rate video bank and effects remain unchanged.
vongon_pack = (R / "Resources/vongon_frames.pack").read_bytes()
if len(vongon_pack) != 12151319:
    fail(f"Vongon frame pack size mismatch: {len(vongon_pack)}")
vongon_sha = hashlib.sha256(vongon_pack).hexdigest()
if vongon_sha != "1de677ca54121e613375cbfcbb50852d9c64ee277357fed0d7498d36a0b6e131":
    fail(f"Vongon frame pack sha256 mismatch: {vongon_sha}")
if vongon_pack[:4] != b"CRF1":
    fail("Vongon frame pack magic mismatch")
vongon_count = int.from_bytes(vongon_pack[4:8], "little")
if vongon_count != 1728:
    fail(f"Vongon frame count mismatch: {vongon_count}")

vongon_manifest = (R / "Resources/vongon_frames_sources.txt").read_text()
for need in [
    "346654116_1792073109459725.mp4",
    "692753445_1792072792221934.mp4",
    "981652867_1792072629589643.mp4",
    "389663512_1792072607026324.mp4",
    "721211298_1792072531100205.mp4",
    "989951027_1792072484719362.mp4",
    "Woman_crying_during_everyday_act…_20261017115051.mp4",
    "Woman_sitting_and_thinking_20261017114947.mp4",
    "Woman_walking_on_beach_20261017114145.mp4",
    "24 fps",
    "1728 frames",
    "EUREKA visual bank is unchanged",
    "DOT mode consumes the same current Vongon frame",
]:
    if need not in vongon_manifest:
        fail(f"Vongon user-video provenance missing: {need}")

icon = (R / "Resources/effects_app_icon.jpg").read_bytes()
if len(icon) != 27540:
    fail(f"launcher icon size mismatch: {len(icon)}")
icon_sha = hashlib.sha256(icon).hexdigest()
if icon_sha != "f4d9d841e5f4789588c53c8babcc849663911583907b259436e683c8268ee703":
    fail(f"launcher icon sha256 mismatch: {icon_sha}")
if not icon.startswith(b"\xff\xd8"):
    fail("launcher icon is not JPEG")

jucer_text = JUCER.read_text()
for need in [
    'name="eureka_frames.pack" compile="0" resource="1" file="Resources/eureka_frames.pack"',
    'name="vongon_frames.pack" compile="0" resource="1" file="Resources/vongon_frames.pack"',
    'name="VongonInspiredProcessor.h" compile="0" resource="0" file="Source/VongonInspiredProcessor.h"',
    'name="effects_app_icon.jpg" compile="0" resource="0" file="Resources/effects_app_icon.jpg"',
    'name="EffectsCloudsEngine.h" compile="0" resource="0" file="Source/EffectsCloudsEngine.h"',
    'file="Source/clouds/dsp/granular_processor.cc"',
    'file="Source/clouds/resources.cc"',
    'file="Source/stmlib/utils/random.cc"',
    'headerPath="../../Source"',
    'smallIcon="appic1"',
    'bigIcon="appic1"',
]:
    if need not in jucer_text:
        fail(f"EUREKA supplied-image resource contract missing: {need}")

if jucer_text.count('headerPath="../../Source"') != 2:
    fail("Android Clouds include-root contract missing from Debug/Release configurations")

# Compile-time duplicate definition guard for processor/editor member functions.
# This catches accidental repeated qualified definitions before CircleCI reaches clang.
qualified_method_pattern = re.compile(
    r"^(?:[\w:<>&*~]+\s+)+"
    r"(RealtimeChordFxAudioProcessor(?:Editor)?::[A-Za-z_][A-Za-z0-9_]*)\s*\(",
    re.MULTILINE,
)
for rel in ["Source/PluginProcessor.cpp", "Source/PluginEditor.cpp"]:
    source_text = (R / rel).read_text()
    names = qualified_method_pattern.findall(source_text)
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        fail(f"duplicate qualified method definition in {rel}: {duplicates}")

# Exact one-definition guards for newly-added methods whose accidental
# duplication is a hard C++ compile error.
for rel, token in [
    ("Source/PluginEditor.cpp",
     "RealtimeChordFxAudioProcessorEditor::updateVongonXYPoint ("),
    ("Source/PluginEditor.cpp",
     "RealtimeChordFxAudioProcessorEditor::paintVongonMode ("),
    ("Source/PluginProcessor.cpp",
     "RealtimeChordFxAudioProcessor::setVongonXY ("),
    ("Source/PluginProcessor.cpp",
     "RealtimeChordFxAudioProcessor::saveUserPreset (int slot)"),
    ("Source/PluginProcessor.cpp",
     "RealtimeChordFxAudioProcessor::loadUserPreset (int slot)"),
    ("Source/PluginProcessor.cpp",
     "RealtimeChordFxAudioProcessor::hasUserPreset (int slot)"),
]:
    source_text = (R / rel).read_text()
    count = source_text.count(token)
    if count != 1:
        fail(f"expected exactly one definition token in {rel}: {token} count={count}")

# Product isolation / no synth implementation.
target_source_files = [
    "Source/FramePack.cpp","Source/FramePack.h",
    "Source/GranularPitchBank.cpp","Source/GranularPitchBank.h",
    "Source/ParameterIDs.h",
    "Source/VideoInteraction.h",
    "Source/ChordRenderers.h",
    "Source/HazeProcessor.h",
    "Source/EffectsCloudsEngine.h",
    "Source/VongonInspiredProcessor.h",
    "Source/EffectsPureDataEngine.h",
    "Source/PsolaHarmonyBank.h",
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
             "MODE","CHORD","DREAMY","EUREKA","CHORDBOT","CLOUDS","ULTRASHEER","ROSETONES","SOLARFAZE","POLYPHRASE","MIDI CONTROL","MIDI SETTINGS","MIDI OUT","X MODE","Y MODE","PRESET","MOTION BARS","MOTION REC","MOTION PLAY","CHORD OUT","CHORD CH"]:
    if need not in src:
        fail(f"UI/requirement token missing: {need}")

editor = (R / "Source/PluginEditor.cpp").read_text()
main_start = editor.find("void RealtimeChordFxAudioProcessorEditor::paintMain")
main_end = editor.find("juce::String RealtimeChordFxAudioProcessorEditor::currentInputName", main_start)
if main_start < 0 or main_end < 0:
    fail("EFFECTS main-screen function boundaries missing")
main_screen = editor[main_start:main_end]
for need in [
    'effectMode == 0',
    '"IN  " + noteText',
    '"COMPLEX"',
    '"BAR"',
    '"WIDTH"',
    '"LENGTH"',
    '"DREAMY"',
    '"EUREKA"',
    '"CHORDBOT"',
    '"CLOUDS"',
    '"ULTRASHEER"',
    '"ROSETONES"',
    '"SOLARFAZE"',
    '"POLYPHRASE"',
    "paintHaze (g)",
    "paintClouds (g)",
    "paintVongonMode (g, effectMode)",
    "paintChordBot (g)",
    '"X " + juce::String',
    '"CONFIG"',
    'u8"● REC"',
]:
    if need not in main_screen:
        fail(f"CHORD/DREAMY mode-specific main UI missing: {need}")
chord_start = main_screen.find("if (effectMode == 0)")
chord_return = main_screen.find("return;", chord_start)
if chord_start < 0 or chord_return < 0:
    fail("CHORD main-screen return boundary missing")
chord_screen = main_screen[chord_start:chord_return]
for bad in [
    '"CHORD-A"',
    '"CHORD-B"',
    '"MOTION REC"',
    '"MOTION PLAY"',
    '"X " + juce::String',
]:
    if bad in chord_screen:
        fail(f"CHORD main UI drifted from approved pre-MIDI-controller baseline: {bad}")

for need in [
    '"HOLD"',
    '"EFFECT"',
    '"REVERB"',
    "reverbSteps",
    "std::pow (dreamyX * dreamyY, 1.35f)",
]:
    if need not in main_screen:
        fail(f"DREAMY reverb indicator missing: {need}")

haze = (R / "Source/HazeProcessor.h").read_text()
for need in [
    "class HazeProcessor",
    "captureBlock",
    "renderBlock",
    "10.0 * effectiveTime",
    "params.echo ? 0.5f * old + 0.5f * input : input",
    "params.lock",
    "params.bypass",
    "params.transpose",
    "params.og",
    "params.warble",
    "params.reverb",
    "hallReverb",
    "hall.roomSize = 0.88f",
    "0.52f * smoothedReverb",
    "params.loops == 2 ? 2.0 : 1.0",
    "params.og ? 0.0005 : 0.010",
    "headPosition",
]:
    if need not in haze:
        fail(f"HAZE processor contract missing: {need}")

for need in [
    "paintHaze",
    '"DUAL LOOP / GLITCH / LO-FI"',
    '"MIX"',
    '"TIME"',
    '"HAZE"',
    '"FILTER"',
    '"REPEAT"',
    '"MOD"',
    '"SPEED"',
    '"LOOPS"',
    '"WARBLE"',
    '"TRANSPOSE"',
    '"ECHO"',
    '"OG"',
    '"LOCK"',
    '"BYPASS"',
    '"GAIN"',
    '"CLEAR"',
    '"SHUFFLE"',
    '"REROLL"',
    '"PANEL"',
    '"CLOSE"',
    '"REVERB / HALL"',
]:
    if need not in editor:
        fail(f"HAZE UI contract missing: {need}")

processor = (R / "Source/PluginProcessor.cpp").read_text()
for need in [
    "hazeProcessor.prepare",
    "hazeProcessor.captureBlock",
    "hazeProcessor.renderBlock",
    "cloudsEngine.prepare",
    "cloudsEngine.process",
    "effectMode == 4",
    "setCloudsXY",
    "vongonProcessor.prepare",
    "vongonProcessor.process",
    "setVongonXY",
    "hazeParamsFromState",
    "hazeClearRequested.exchange",
    "hazeShuffleRequested.exchange",
    "hazeProcessor.forceShuffle",
    "effectMode == 2",
    "selectedEffectMode == 1",
    "processChordBotMidi (midi)",
    "regenerateChordBotSuggestions",
    "setChordBotSlot",
    "triggerChordBotPad",
    "chordBotLayout",
    "selectedEffectMode == 3",
    MODE_CHOICE_TOKEN,
]:
    if need not in processor:
        fail(f"HAZE processor integration missing: {need}")

if 'if (effectMode == 3)' not in processor or "effectMode == 2" not in processor:
    fail("CHORDBOT-only bypass / EUREKA output-safety routing missing")
if "effectMode >= 4" not in processor or "applyOutputSafety (buffer, outputActive)" not in processor:
    fail("audible mode output-safety routing missing")

for need in [
    "ParamID::hazeMix",
    "ParamID::hazeTime",
    "ParamID::hazeAmount",
    "ParamID::hazeFilter",
    "ParamID::hazeRepeat",
    "ParamID::hazeMod",
    "ParamID::hazeReverb",
    "ParamID::hazeSpeed",
    "ParamID::hazeLoops",
    "ParamID::hazeWarble",
    "ParamID::hazeTranspose",
    "ParamID::hazeEcho",
    "ParamID::hazeOg",
    "ParamID::hazeLock",
    "ParamID::hazeBypass",
    "ParamID::hazeGain",
    "ParamID::hazePath",
]:
    if need not in processor:
        fail(f"HAZE parameter contract missing: {need}")

for need in [
    "paintChordBot",
    "chordBotPadBounds",
    '"TOP-LEFT = THEORY ROOT"',
    '"EDIT"',
    '"SAVE"',
    "processor.triggerChordBotPad",
    "processor.setChordBotSlot",
]:
    if need not in editor:
        fail(f"CHORDBOT UI contract missing: {need}")

for need in [
    "ChordBotChoice",
    "chordBotSuggestions",
    "transition[0][degree]",
]:
    if need not in src:
        fail(f"CHORDBOT theory contract missing: {need}")

core_test = (R / "tests/core_test.cpp").read_text()
for need in [
    "botMajor",
    "foundG7",
    "botMinor",
    "foundE7",
    "SineArpeggiator::midiFrequency",
    "quarterSemitone",
]:
    if need not in core_test:
        fail(f"CHORDBOT regression test missing: {need}")

editor_header = (R / "Source/PluginEditor.h").read_text()
for need in [
    "BinaryData::eureka_frames_pack",
    "eurekaFrames",
    "currentEurekaFrame",
    "loadedEurekaFrame",
    "eurekaFrames.getFrameCount()",
    "eurekaFrameIndex",
    "nextVisualFrame",
    "chordVisualCooldown",
    "eurekaVisualCooldown",
    "processor.getInputPeakRaw()",
    "processor.getInputRmsRaw()",
    "eurekaPanelVisible",
    "eurekaXYTouchDown",
    "updateEurekaXYPoint",
    "ParamID::hazeRepeat",
    "ParamID::hazeTime",
    "ParamID::hazeReverb",
]:
    if need not in editor and need not in editor_header:
        fail(f"EUREKA frame/XY contract missing: {need}")

for removed in [
    "eurekaMotionSteps",
    "eurekaMotionArmed",
    "eurekaMotionRecording",
    "eurekaMotionPlaying",
    "updateEurekaMotionPoint",
    "DRAG TO RECORD 3 SEC",
    "TAP IMAGE TO ARM MOTION",
]:
    if removed in editor or removed in editor_header:
        fail(f"obsolete EUREKA Motion REC remains: {removed}")

if "isKeyCurrentlyDown" in editor:
    fail("Android Motion REC must not rely on JUCE Android isKeyCurrentlyDown()")

# MOOD visual events are driven by input onsets, MIDI notes or XY movement;
# reject unconditional timer-driven image changes (7.5 Hz regression).
mood_visual_begin = editor.find("if (effectMode == 10 && moodFrames.getFrameCount() > 0)")
mood_visual_end = editor.find("const bool eurekaEntered", mood_visual_begin)
if mood_visual_begin < 0 or mood_visual_end <= mood_visual_begin:
    fail("MOOD visual timer boundaries missing")
mood_visual = editor[mood_visual_begin:mood_visual_end]
for required_event in (
    "note != moodPreviousNote",
    "const bool onset = level > 0.09f && moodPreviousActivity < 0.04f;",
    "xyChange > 0.055f",
    "currentMoodFrame = moodFrames.getFrame (moodLoadedFrame);",
):
    if required_event not in mood_visual:
        fail(f"MOOD audio/MIDI/XY-driven frame switching missing: {required_event}")
if "moodTick" in mood_visual or "moodTick" in editor_header:
    fail("MOOD unconditional periodic frame switching reintroduced")
if "if (note != moodPreviousNote || onset || xyChange > 0.055f)" not in mood_visual:
    fail("MOOD frame switching is no longer exclusively event-driven")

processor_controls = (R / "Source/PluginProcessor.cpp").read_text()
processor_header = (R / "Source/PluginProcessor.h").read_text()
# MOOD controls an external Chase Bliss pedal on the global MIDI OUT channel.
# Regression guard: no local DSP, no input gain, CC102 capture and real CC/notes.
for need in [
    "if (effectMode != 10)",
    "if (effectMode == 10)\n    {\n        buffer.clear();\n        return;",
    "processMoodMidi (midi, midiInputChannel, enabled)",
    "moodLoopEngaged.load (std::memory_order_acquire) ? 127 : 0",
    "juce::MidiMessage::controllerEvent (channel, cc, value)",
    "juce::MidiMessage::noteOn (",
    "juce::MidiMessage::noteOff (",
]:
    if need not in processor:
        fail(f"MOOD MIDI-only/audio-bypass contract missing: {need}")
if "moodProcessor.process (buffer)" in processor:
    fail("MOOD local audio DSP must not be called")
# Validate the current functional UI contract, not obsolete help copy.
for need in [
    "MIDI ONLY - INPUT GAIN OFF",
    "processor.getMoodAxisCc (true)",
    "processor.getMoodAxisCc (false)",
    "processor.getMoodAxisValue (true)",
    "processor.getMoodAxisValue (false)",
    "TAP PAD: REC/PLAY  HOLD: CLEAR  DRAG: XY",
    "moodMidiEnabled",
    "processor.moodHasCapture() ? \"LOOP OFF\" : \"CAPTURE\"",
]:
    if need not in editor:
        fail(f"MOOD MIDI UI contract missing: {need}")
for need in [
    "moodCaptureDirty",
    "moodLoopEngaged",
    "moodUiNote",
    "moodExternalNote",
    "void processMoodMidi (juce::MidiBuffer&, int channel, bool enabled);",
]:
    if need not in processor_header:
        fail(f"MOOD external MIDI queue missing: {need}")

parameter_ids = (R / "Source/ParameterIDs.h").read_text()
for need in [
    "ParamID::wet",
    "ParamID::boostDb",
    "saveUserPreset (int slot)",
    "loadUserPreset (int slot)",
    "hasUserPreset (int slot)",
    '"userPresetXml" + juce::String (slot)',
    "globalPresetSlot",
    "buffer.applyGain (wet)",
    "decibelsToGain",
]:
    if need not in src and need not in processor_controls and need not in processor_header:
        fail(f"WET/BOOST/SAVE-LOAD contract missing: {need}")
# Preserve wet-only individual effect engines: CHORD/DREAMY/EUREKA etc.
# must not introduce hidden dry passthrough inside their own DSP.
# A user-controlled, default-zero global DRY monitor is permitted only
# through the isolated post-FX input tap audited below.
for need in [
    "buffer.applyGain (wet)",
    "buffer.clear();",
    "wetSample * wetMix",
]:
    if need not in processor_controls:
        fail(f"dry-free master output contract missing: {need}")
for blocked in [
    "wetDryScratch",
    "dryLeft * 0.78f",
    "dryLeft * 0.68f",
    "dry[channel] + (wetSample - dry[channel])",
]:
    if blocked in processor_controls or blocked in processor_header:
        fail(f"unwanted DRY audio leak remains: {blocked}")
# ROSETONES uses the exact user-selected 48-filter algorithm and
# resonator-only output. This guard must not force the older #191 dry blend.
if "const float out = roseToneLow[(size_t) ch]" not in (R / "Source/VongonInspiredProcessor.h").read_text():
    fail("ROSETONES selected reference wet-only output missing")
if "float outL = wetL * activeMix" not in (R / "Source/HazeProcessor.h").read_text():
    fail("EUREKA wet-only output regression")
if "left[n] = softProtect (wetL * wetMix)" not in (R / "Source/EffectsCloudsEngine.h").read_text():
    fail("CLOUDS dry passthrough regression")
if not (R / "tests/effects_audio_regression.cpp").is_file() or not (R / "tests/dsp_juce_shim/JuceHeader.h").is_file():
    fail("native DSP WAV regression suite missing")

for need in [
    'const char* wet = "wet"',
    'const char* boostDb = "boostDb"',
]:
    if need not in parameter_ids:
        fail(f"WET/BOOST parameter id missing: {need}")
for need in [
    "#include <juce_core/native/juce_JNIHelpers_android.h>",
    "getAndroidPhysicalInputNames",
    'name.containsIgnoreCase ("RG Rotate")',
    "ROUTE IN:",
    "ROUTE OUT:",
    "dBFS",
    "getActiveInputChannels",
    "getProductName",
    "GET_DEVICES_INPUTS",
    "setResizable (true, false)",
    "initialise (2, 2, nullptr, true)",
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

dreamy = (R / "Source/PluginProcessor.cpp").read_text()
for need in [
    "dreamyBuffer",
    "dreamyVoiceCount = 2",
    "1.3348398f, 2.0f",
    "currentSampleRate * 2.5",
    "currentSampleRate * 0.75",
    "0.24f + y * 0.34f",
    "0.90f + x * 0.10f",
    "processDreamy (buffer)",
    "getInputPeakRaw",
    "getInputRmsRaw",
    "getInputNonZeroRatio",
]:
    if need not in src and need not in dreamy:
        fail(f"Dreamy/live-input contract missing: {need}")
if "pitchBank" in dreamy or "GranularPitchBank" in (R / "Source/PluginProcessor.h").read_text():
    fail("obsolete generated-audio pitch shifter must not remain in Processor")

renderers = (R / "Source/ChordRenderers.h").read_text()

# CHORD-A is frozen to the approved HOLD baseline behavior. Noise hardening must
# stay in CHORD-B/DREAMY and must not alter the accepted sampled-input renderer.
a0 = renderers.find("class SampleChordRenderer")
a1 = renderers.find("class SineArpeggiator", a0)
if a0 < 0 or a1 < 0:
    fail("CHORD-A renderer boundaries missing")
chord_a_renderer = renderers[a0:a1]
for need in [
    "phase[(size_t) i] = 0.0;",
    "phase.fill (0.0);",
    "return sum / std::sqrt",
]:
    if need not in chord_a_renderer:
        fail(f"CHORD-A approved renderer drift: {need}")
for bad in [
    "recaptureTransitionRemaining",
    "recaptureStartOutput",
    "lastRenderedOutput",
]:
    if bad in chord_a_renderer:
        fail(f"CHORD-A must remain on approved renderer; unexpected token: {bad}")

for need in [
    "class SampleChordRenderer",
    "captureRecentPhrase",
    "void setHold (float amount01)",
    "0.240f + hold * 1.760f",
    "0.020f + hold * 0.140f",
    "class SineArpeggiator",
    "triggerRandomNote",
    "consumeNoteTrigger",
    "setTuningOffsetSemitones",
    "midiFrequency",
    "tuningOffsetSemitones",
    "class ChordBRandomFx",
    "shortDelay",
    "longDelay",
    "tapeDelay",
    "tapeSim",
    "bitcrush",
    "chorus",
    "reverb",
    "chooseForNote",
    "transitionSamplesRemaining",
    "currentMidiNote",
    "std::abs (notes[(size_t) i] - currentMidiNote) <= 7",
    "tailReturn = 0.22f",
    "softProtect (float sample)",
    "std::isfinite (sample)",
    "softProtect (input + dl * feedback)",
    "softProtect (input + (wetL * 0.38f + wetR * 0.14f))",
]:
    if need not in renderers:
        fail(f"CHORD A/B renderer contract missing: {need}")

processor_header = (R / "Source/PluginProcessor.h").read_text()

pa0 = dreamy.find("void RealtimeChordFxAudioProcessor::processChordAudio")
pa1 = dreamy.find("void RealtimeChordFxAudioProcessor::processChordB", pa0)
if pa0 < 0 or pa1 < 0:
    fail("CHORD-A processor boundaries missing")
chord_a_audio = dreamy[pa0:pa1]
# User-approved 100% generated CHORD-A output supersedes the older
# DRY+CHORD mix. Prevent accidental regression to an unprocessed signal.
if "softProtectBuffer (buffer)" in chord_a_audio:
    fail("CHORD-A output-safety contract drift")
for need in [
    "sampleChordRenderer.renderSample (",
    "audible || chordAOutputGain > 0.0f",
    "const float output = chord * (0.65f * chordAOutputGain);",
    "buffer.setSample (0, i, output)",
    "processChordReverb (buffer)",
]:
    if need not in chord_a_audio:
        fail(f"CHORD-A wet-only smooth gate contract missing: {need}")
for forbidden in [
    "const float output = audible ? chord * 0.65f : 0.0f;",
    "sampleChordRenderer.canCapture()",
]:
    if forbidden in chord_a_audio:
        fail(f"CHORD-A audible discontinuity or old capture path returned: {forbidden}")
for rejected in [
    "dryLeft * 0.78f + chord * 0.46f",
    "dryRight * 0.78f + chord * 0.46f",
]:
    if rejected in chord_a_audio:
        fail(f"CHORD-A unwanted dry input path: {rejected}")

switch0 = dreamy.find("if (effectMode == 0 && chordMode != lastChordMode)")
switch1 = dreamy.find("handleMotionCommand()", switch0)
if switch0 < 0 or switch1 < 0 or "chordReverb.reset();" not in dreamy[switch0:switch1]:
    fail("CHORD-A/B approved switch behavior drift: chordReverb.reset missing")

for need in [
    "ParamID::effectMode",
    MODE_CHOICE_TOKEN,
    "ParamID::chordMode",
    '"CHORD ENGINE", juce::StringArray { "A", "B" }, 0',
    "processChordAudio (buffer)",
    "processChordB (buffer)",
    "processDreamy (buffer)",
    "sampleChordRenderer.pushInput",
    "ParamID::hold",
    "ParamID::effect",
    "chordBRandomFx.setProbability",
    "sineArpeggiator.setTuningOffsetSemitones",
    "chordBRandomFx.chooseForNote",
    "chordBRandomFx.processSample",
    "softProtectBuffer (buffer)",
    "dreamyDelaySamplesSmoothed",
    "dreamyAmbienceSmoothed",
    "dreamyPostWasEnabled",
    "buffer.applyGain (0.80f)",
    "dreamyVisualEnvelope",
    "dreamyVisualSequence",
    "changePeak",
    "static constexpr int dx[8]",
    "static constexpr int dy[8]",
    "currentSampleRate / 12.0",
    "softProtectSample (",
    "const float delayWrite",
    "const float mixed",
    "applyOutputSafety (buffer, outputActive)",
    "currentSampleRate * 0.008f",
    "outputSafetyGain",
]:
    if need not in dreamy:
        fail(f"CHORD/DREAMY mode contract missing: {need}")

mode_switch = dreamy.find("if (effectMode == 3)")
if mode_switch < 0:
    fail("CHORDBOT MIDI transition ordering missing")
mode_switch_end = dreamy.find("// Dreamy always receives", mode_switch)
mode_switch_block = dreamy[mode_switch:mode_switch_end]
if mode_switch_block.find("processChordMidi (midi);") < 0 or mode_switch_block.find("processChordBotMidi (midi);") < 0:
    fail("CHORDBOT MIDI transition ordering missing")
if mode_switch_block.find("processChordMidi (midi);") > mode_switch_block.find("processChordBotMidi (midi);"):
    fail("CHORDBOT entry must stop CHORD MIDI before CHORDBOT Note On")

if "notifyChordAHoldChanged" not in processor_header:
    fail("CHORD/DREAMY mode contract missing: notifyChordAHoldChanged")
for bad in [
    "PsolaHarmonyBank",
    "psolaTargetPeriod",
    "psolaCurrentPeriod",
    "updateChordRatios",
]:
    if bad in dreamy or bad in processor_header:
        fail(f"obsolete PSOLA path remains in active processor: {bad}")

if 'if (effectMode == 0)' not in dreamy:
    fail("CHORD and DREAMY audible paths must remain mode-separated")
for need in [
    "dreamyDelayBuffer",
    "dreamyReverb",
    "std::pow (x * y, 1.35f)",
    "0.46f * ambience",
    "0.26f * ambience",
]:
    if need not in src and need not in dreamy:
        fail(f"Dreamy anti-fizz/ambience contract missing: {need}")
if "dreamyWetLowpass" in dreamy:
    fail("FLOWER Master Dreamy core must not be modified by in-core low-pass")
if "DspTap" not in (R / "THIRD_PARTY_NOTICES.md").read_text():
    fail("DspTap MIT attribution missing")

theory = (R / "Source/TheoryEngine.cpp").read_text()
for need in [
    "degreeContainsAnchor",
    "the live note is already a chord member",
    "Chromatic input",
]:
    if need not in theory:
        fail(f"anchor-constrained harmony contract missing: {need}")

clouds_engine = (R / "Source/EffectsCloudsEngine.h").read_text()
for need in [
    "class EffectsCloudsEngine",
    "clouds::GranularProcessor",
    "PLAYBACK_MODE_GRANULAR",
    "params->position",
    "params->size",
    "params->density",
    "params->texture",
    "pitchSemitones",
    "smoothedX < 0.10f ? -12.0f",
    "smoothedX < 0.22f ? -7.0f",
    "smoothedX > 0.78f ? 7.0f",
    "params->pitch",
    "params->stereo_spread",
    "params->feedback",
    "params->reverb",
    "params->dry_wet = juce::jlimit",
    "params->freeze = freezeCc < 0 ? freezeZone : freezeCc >= 64",
    "params->trigger = (touchDown && ! previousTouchDown)",
    "params->gate = gateCc < 0 ? touchDown : gateCc >= 64",
    "wetMix",
]:
    if need not in clouds_engine:
        fail(f"Clouds XY/DSP contract missing: {need}")

if "cloudsEnabled" in parameter_ids or "ParamID::cloudsEnabled" in processor:
    fail("obsolete global CLOUDS enable parameter must not remain; CLOUDS is MODE=4")

for need in [
    'effectMode == 4',
    '"CLOUDS"',
    "paintClouds (g)",
    "updateCloudsXYPoint",
    "cloudsXYTouchDown",
]:
    if need not in editor and need not in processor:
        fail(f"CLOUDS dedicated-mode UI contract missing: {need}")

for rel in [
    "Source/clouds/dsp/granular_processor.cc",
    "Source/clouds/resources.cc",
    "Source/stmlib/dsp/dsp.h",
    "Source/stmlib/utils/random.cc",
]:
    if not (R / rel).is_file():
        fail(f"materialized Clouds dependency missing: {rel}")

notices = (R / "THIRD_PARTY_NOTICES.md").read_text()
if "Mutable Instruments Clouds" not in notices or "Emilie Gillet" not in notices:
    fail("Clouds MIT attribution missing")
if "Vongon-inspired EFFECTS modes" not in notices or "No Vongon firmware" not in notices:
    fail("independent Vongon-inspired implementation notice missing")

haze = (R / "Source/HazeProcessor.h").read_text()
for need in [
    "smoothedTime",
    "smoothedRepeat",
    "smoothedReverb",
    "sampleRate * 0.090",
    "params.og ? 0.0005 : 0.010",
    "sampleRate * 0.016",
    "lane.pendingStutterStartValid",
    "lane.playPosition += (double) seam",
    "softProtect (out[i])",
]:
    if need not in haze:
        fail(f"EUREKA click-hardening contract missing: {need}")

vongon = (R / "Source/VongonInspiredProcessor.h").read_text()
for need in [
    "class VongonInspiredProcessor",
    "ultrasheer = 5",
    "rosetones = 6",
    "solarfaze = 7",
    "polyphrase = 8",
    "processUltrasheer",
    "processRosetones",
    "processSolarfaze",
    "processPolyphrase",
    "polyCrossfadeRemaining",
    "secondsTable[9]",
    "0.085, 0.170, 0.341, 0.682, 1.3, 2.7, 5.5, 11.0, 22.0",
    "std::array<std::array<ResonatorState, 12>, 2>",
    "std::array<RoseDetectorState, 48>",
    "float tick (float drive",
    "ultraInputDiff[0].allpass",
    "ultraFeedback[(size_t) (1 - ch)]",
    "solarBands[(size_t) ch][(size_t) band].tune",
    "solarRatio[1] = 1.0 / solarRatio[0]",
    "const float depth = std::abs (2.0f * depthKnob - 1.0f)",
    "void setDirectCc (",
]:
    if need not in vongon:
        fail(f"Vongon-inspired DSP contract missing: {need}")

# EUREKA click-hardening remains intact; ROSETONES alone must match the
# selected fix/effects-audio-rosetones-vongon-ui-rgrotate implementation.
for need in [
    "sampleRate * 0.090",
    "sampleRate * 0.016",
    "const float t = linear * linear",
]:
    if need not in haze:
        fail(f"EUREKA right-XY smooth stutter contract missing: {need}")
rose_start = vongon.find("void processRosetones (")
rose_end = vongon.find("void processSolarfaze (", rose_start)
if rose_start < 0 or rose_end < 0:
    fail("ROSETONES selected reference DSP boundaries missing")
rose_code = vongon[rose_start:rose_end]
for need in [
    "48 chromatic input filters C2..B5",
    "const float slider = ccValue (rosetones, 87, x)",
    "const float feedbackGain = juce::jlimit",
    "const float noteDrive = gate",
    "pitchSigned[(size_t) note]",
    "roseDetectors[(size_t) bandIndex]",
    "roseFeedbackSmoothed",
    "const float out = roseToneLow[(size_t) ch]",
]:
    if need not in rose_code:
        fail(f"ROSETONES selected 48-filter algorithm missing: {need}")
if "roseLegacyState" in vongon or "roseLegacyToneLow" in vongon:
    fail("ROSETONES older #191 resonator state unexpectedly reintroduced")
# The explicitly restored Oct 16 dry-free CHORD-A baseline resets voice
# playback phases on progression; no Oct 17 capture-ready modification.
for forbidden in [
    "bool canCapture() const noexcept",
    "if (recapture || captureLength <= 0)\n                phase[(size_t) i]",
]:
    if forbidden in chord_a_renderer:
        fail(f"CHORD-A reverted renderer contract violated: {forbidden}")
if renderers.count("phase[(size_t) i] = 0.0;") < 1:
    fail("CHORD-A approved voice phase reset missing")

# CHORD-A/DREAMY/EUREKA share quarter-rate YIN for audio-thread
# budget. The Vongon DSP and wet samples must remain independent.
for need in [
    "pitchDetector.prepare (currentSampleRate / 4.0, 768, 128)",
    "chordPitchAccumulator += mono",
    "if (++chordPitchDecimation == 4)",
    "pitchDetector.pushSample (decimated)",
    "const bool analyseChordPitch = effectMode == 0",
    "apvts.getRawParameterValue (ParamID::chordMidiOut)->load() >= 0.5f",
    "if (analyseChordPitch)",
    "chordPitchAnalysisActive = analyseChordPitch",
]:
    if need not in processor:
        fail(f"CHORD/DREAMY/EUREKA low-rate pitch path missing: {need}")
for need in ["chordPitchAccumulator", "chordPitchDecimation", "chordPitchAnalysisActive"]:
    if need not in processor_header:
        fail(f"CHORD-family YIN decimation state missing: {need}")
for need in [
    "eurekaReactiveFrameRate (float inputActivity)",
]:
    if need not in (R / "Source/VideoInteraction.h").read_text():
        fail(f"EUREKA audiovisual reaction helper missing: {need}")
# EUREKA's 11 user-provided clips must all be reachable DURING playback.
# No sequential clip list, no mandatory wait until frame 35, and no
# fixed-interval slideshow. Leave the existing audio DSP and visual bank alone.
eureka_begin = editor.find("else if (effectMode == 2 && eurekaFrames.getFrameCount() > 0)")
eureka_end = editor.find("else if (effectMode >= 5", eureka_begin)
if eureka_begin < 0 or eureka_end <= eureka_begin:
    fail("EUREKA timer rendering block missing")
eureka_playback = editor[eureka_begin:eureka_end]
for need in [
    "const int clips = total / clipFrames",
    "const bool randomSustainCut",
    "const bool switchNow",
    "eurekaEntered",
    "attack || randomSustainCut",
    "const int randomClip = nextVisualFrame (clips, previousClip)",
    "const int randomStart = nextVisualFrame (clipFrames, -1)",
    "eurekaFrameIndex = randomClip * clipFrames + randomStart",
    "eurekaClipCooldown = 18",
    "const float targetFps = effects_video::eurekaReactiveFrameRate (activity)",
    "(position + 1) % clipFrames",
]:
    if need not in eureka_playback:
        fail(f"EUREKA live random clip cut contract missing: {need}")
for forbidden in [
    "eurekaSwitchClipPending",
    "else if (position == clipFrames - 1)",
    "eurekaFrameIndex = nextClip * clipFrames",
    "(clip + 1) % clips",
]:
    if forbidden in eureka_playback:
        fail(f"EUREKA sequential/end-only clip switch reintroduced: {forbidden}")
for need in [
    "int eurekaClipCooldown = 0",
    "bool eurekaWasActive = false",
]:
    if need not in editor_header:
        fail(f"EUREKA random clip selection state missing: {need}")
for need in [
    "const bool eurekaEntered = effectMode == 2 && ! eurekaWasActive",
    "eurekaWasActive = effectMode == 2",
]:
    if need not in editor:
        fail(f"EUREKA random initial clip choice missing: {need}")
if "eurekaFramePhase += 4.5f / 30.0f" in editor:
    fail("EUREKA returned to passive fixed-rate video playback")
if "paintVideoEffect (g, currentEurekaFrame" not in editor:
    fail("EUREKA video rendering path unexpectedly changed")
if "mode < 5 || mode > 8" not in editor:
    fail("Vongon-only overlay boundary unexpectedly changed")

# CHORD-A must NOT force the live chord waveform to zero on gate-off:
# the linear release keeps the captured loop alive until the 8ms gain fade ends.
for need in [
    "chordAOutputGain = juce::jlimit (0.0f, 1.0f,",
    "chordAOutputGain + (audible ? fadeStep : -fadeStep)",
    "audible || chordAOutputGain > 0.0f",
    "chord * (0.65f * chordAOutputGain)",
]:
    if need not in processor:
        fail(f"CHORD-A anti-click gate envelope missing: {need}")
if "audible ? chord * 0.65f : 0.0f" in processor:
    fail("CHORD-A instantaneous wet output gate reintroduced")
if "chordAOutputGain" not in processor_header:
    fail("CHORD-A gate smoothing state missing")

# Regression guards for the reported EUREKA/POLYPHRASE audio cuts and
# ROSETONES upper-XY silence. Check the structures rather than mute failures.
for need in [
    "spanFramesUntilRefresh = 64",
    "lane.stutterStart = lane.pendingStutterStartValid",
    "lane.pendingStutterStart = nextStart",
    "readSpan (",
    "headPosition, lane.pendingStutterStart",
    "lane.playPosition += (double) seam",
    "params.og && leftSide",
    "const float activeMix = params.mix * historyReady",
]:
    if need not in haze:
        fail(f"EUREKA anti-dropout path missing: {need}")
for need in [
    "targetSamples > (double) polySamplesWritten",
    "if (polyCrossfadeRemaining[(size_t) channel] > 0)",
    "const float validFade",
    "wetTarget * history - polyWetSmoothed",
    "pitchSigned[pitchClass] += detector.band",
    "0.955f + feedback * 0.042f",
    "wet[ch] * 1.25f * synthLevel",
]:
    if need not in vongon:
        fail(f"Vongon dropout/voice-level regression: {need}")
for need in [
    "if (attack && chordVisualCooldown <= 0)",
    "effects_video::eurekaReactiveFrameRate (activity)",
    "eurekaFramePhase += eurekaVideoSpeed / 30.0f",
    "eurekaVideoRewindFrames > 0",
    "const bool fullVideo = total == 9 * 192 || total == 6 * 192",
    "const float musicalEnergy = juce::jlimit",
    "const float gestureEnergy = juce::jmax",
    "audioTileDivisions > 1 ? 0.45f : 0.0f",
    "const float targetFps = drive < 0.018f ? 0.0f",
    "vongonFramePhase += vongonVideoSpeed / 30.0f",
    "vongonRewindFrames > 0",
    "vongonGhostFrame = currentVongonFrame",
    "vongonFxStyle =",
    "vongonFringeRevision != dotVongonRevision",
    "vongonRedFringe",
    "vongonCyanFringe",
    "eurekaFrameIndex = randomClip * clipFrames + randomStart",
    "paintVideoEffect (g, currentEurekaFrame",
    "paintVideoEffect (",
    "g, currentVongonFrame",
]:
    if need not in editor:
        fail(f"Vongon reactive video effects missing: {need}")
for forbidden in [
    "vongonFramePhase += playbackFps / 30.0f",
    "row + (videoEffectTick % period)",
    "if (((y + dotUiTick) % 8) == 0)",
]:
    if forbidden in editor:
        fail(f"Video bank unwanted clock-driven lines: {forbidden}")
vstart = editor.find("void RealtimeChordFxAudioProcessorEditor::paintVideoEffect (")
vend = editor.find("void RealtimeChordFxAudioProcessorEditor::paintVongonMode (", vstart)
if vstart < 0 or vend < 0:
    fail("Vongon video rendering function boundaries missing")
vfx = editor[vstart:vend]
for need in [
    "vongonVisualFx < 0.025f",
    "vongonGhostFrame",
    "bandY",
    "vongonRewindFrames",
]:
    if need not in editor:
        fail(f"Vongon event-reactive visual change missing: {need}")
if "for (int row = 0; row < 720; row += period)" in vfx:
    fail("Vongon periodic scanning lines must not be drawn")
pstart = editor.find("void RealtimeChordFxAudioProcessorEditor::paintVongonMode (")
pend = editor.find("void RealtimeChordFxAudioProcessorEditor::paintVongonParameterPanel (", pstart)
if pstart < 0 or pend < 0:
    fail("Vongon overlay removal test could not locate target region")
vpaint = editor[pstart:pend]
for forbidden in [
    "g.drawLine (px, 72.0f, px, 558.0f, 1.0f);",
    "g.drawLine (0.0f, py, 720.0f, py, 1.0f);",
    "g.fillRect (0.0f, 0.0f, 720.0f, 88.0f);",
]:
    if forbidden in vpaint:
        fail(f"Vongon crosshair or black upper curtain remained: {forbidden}")


# #195 Android clang failure: JUCE jlimit(T,T,T) does not accept a
# float/double expression mixture. The native test shim must enforce the
# same template-deduction constraints to catch such mistakes before APK.
if "float) (((double) polySamplesWritten" not in vongon:
    fail("POLYPHRASE #195 JUCE jlimit expression must cast its complete ratio")
shim = (R / "tests/dsp_juce_shim/JuceHeader.h").read_text()
if "template <typename T>\nconstexpr T jlimit(T low,T high,T value)" not in shim:
    fail("offline JUCE shim must use a single-Type jlimit template")
if "std::common_type_t" in shim:
    fail("offline JUCE shim must not promote mixed arithmetic types")

# MIDI IN operates for all existing EFFECTS modes without blocking the
# audio callback. These controls are not the MIDI OUT X/Y configuration.
midi_proc = (R / "Source/PluginProcessor.cpp").read_text()
midi_head = (R / "Source/PluginProcessor.h").read_text()
midi_editor = (R / "Source/PluginEditor.cpp").read_text()
for need in [
    "midiInputPending[(size_t) cc].store (",
    "message.isController()",
    "message.isForChannel (midiInputChannel)",
    "cc >= 110 && cc <= 119",
    "effectMode == 0 && cc >= 20 && cc <= 27",
    "effectMode == 2 && cc >= 40 && cc <= 56",
    "effectMode == 3 && (cc == 120 || cc == 121)",
    "setCloudsXY (x, y, false)",
    "setVongonXY (x, y, false)",
    "void RealtimeChordFxAudioProcessor::applyPendingMidiInputChanges()",
    "case 24: id = ParamID::hold",
    "case 46: id = ParamID::hazeReverb",
    "ParamID::effectMode",
    "notifyChordAHoldChanged()",
]:
    if need not in midi_proc:
        fail(f"all-mode MIDI IN route missing: {need}")
for need in [
    "std::array<std::atomic<int>, 128> midiInputPending",
    "void applyPendingMidiInputChanges()",
]:
    if need not in midi_head:
        fail(f"all-mode MIDI IN queue missing: {need}")
if "processor.applyPendingMidiInputChanges();" not in midi_editor:
    fail("all-mode MIDI IN message-thread bridge missing")

# Existing MIDI input bridge and existing DSP controls only. Distinct ranges
# must remain mode-gated and no MIDI may be echoed to the output.
for need in [
    "const bool chordB = effectMode == 0 && chordMode == 1",
    "cc >= 65 && cc <= 68",
    "const bool dreamy = effectMode == 1 && cc >= 60 && cc <= 63",
    "const bool clouds = effectMode == 4 && cc >= 70 && cc <= 81",
    "cloudsEngine.setMidiCc (cc, value)",
    "chordBForcedFx.store (",
    "dreamyAmbienceMidi.store (value",
    "setNormalised (ParamID::dryLevel, norm)",
    "setNormalised (ParamID::internalBpm, norm)",
    "cc == 66 ? ParamID::effect : ParamID::length",
    "if (activeChordB)",
    "chordBRandomFx.setForcedEffect (",
    "const int midiAmbience = dreamyAmbienceMidi.load",
]:
    if need not in midi_proc:
        fail(f"new per-effect MIDI CC path missing: {need}")
for need in [
    "chordBForcedFx",
    "dreamyAmbienceMidi",
    "getCloudsMidiCc",
]:
    if need not in midi_head:
        fail(f"new per-effect MIDI state missing: {need}")
for need in [
    "void setMidiCc (int number, int value) noexcept",
    "int getMidiCc (int number) const noexcept",
    "ccNorm (70, smoothedX)",
    "ccNorm (71, smoothedY)",
    "ccNorm (72, smoothedY)",
    "ccNorm (73, smoothedX)",
    "getMidiCc (74)",
    "ccNorm (75, smoothedX)",
    "ccNorm (76, smoothedY)",
    "ccNorm (77, xyReverb)",
    "ccNorm (78, xyWet)",
    "getMidiCc (79)",
    "getMidiCc (80)",
    "getMidiCc (81)",
    "std::array<std::atomic<int>, 12> midiCc",
]:
    if need not in clouds_engine:
        fail(f"CLOUDS individual MIDI CC wiring missing: {need}")
renderer = (R / "Source/ChordRenderers.h").read_text()
for need in [
    "void setForcedEffect (int requested) noexcept",
    "forcedEffect = next",
    "const int nextEffect = forcedEffect >= 0",
    "int forcedEffect = -1;",
]:
    if need not in renderer:
        fail(f"CHORD-B existing FX MIDI selector missing: {need}")
if "run_build: true" in (R / ".circleci/config.yml").read_text():
    fail("CI must stay manual run_build=false by default")

# New controls must be usable and input gain must reach the real audio path.
for need in [
    "ParamID::inputGainDb",
    "inputGainLinearSmoothed",
    "vongonProcessor.handleMidi (",
    "vongonProcessor.process (",
]:
    if need not in (R / "Source/PluginProcessor.cpp").read_text():
        fail(f"EFFECTS gain/Vongon integration missing: {need}")
for need in [
    "INPUT GAIN ",
    "updateInputGainFromX",
    "paintVongonParameterPanel",
    "setVongonParameterFromX",
]:
    if need not in (R / "Source/PluginEditor.cpp").read_text():
        fail(f"EFFECTS input gain/Vongon UI contract missing: {need}")
if "inputGainDb" not in (R / "Source/ParameterIDs.h").read_text():
    fail("EFFECTS INPUT GAIN parameter ID missing")


for need in [
    "vongonFrames",
    "BinaryData::vongon_frames_pack",
    "dotVongonRevision",
    "updateVongonXYPoint",
    "vongonXYTouchDown",
    "paintVongonMode",
]:
    if need not in editor and need not in editor_header:
        fail(f"Vongon video/DOT/XY contract missing: {need}")

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
if root.attrib.get("pluginChannelConfigs") != "{1,2},{2,2}":
    fail("expected mono/stereo input -> stereo output channel configs {1,2},{2,2}")

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
if "GranularPitchBank.cpp" in jucer_text or "GranularPitchBank.h" in jucer_text:
    fail("obsolete generated-audio pitch shifter must not be compiled into EFFECTS")
if "Resources/classroom_frames.pack" not in jucer_text:
    fail("supplied frame pack not embedded")
if "Resources/eureka_frames.pack" not in jucer_text:
    fail("EUREKA frame pack not embedded")
if "Resources/vongon_frames.pack" not in jucer_text:
    fail("Vongon frame pack not embedded")
for bad_ref in ["FLOWER_Standalone.jucer","SynthVoice","Carnival","flower_"]:
    if bad_ref in jucer_text:
        fail(f"cross-project JUCER reference found: {bad_ref}")

# CI: preserve the known-good AN-10..AN-15 controls instead of simplifying them.
ci = (R / ".circleci/config.yml").read_text()
for need in [
    "run_build:", "default: false",
    "Materialize exact supplied frame pack",
    "Materialize pinned Clouds source",
    "scripts/materialize_effects_clouds.py",
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

# Noise-hardening contracts.
if "std::clamp (left, -0.72f, 0.72f)" in renderers or "std::clamp (right, -0.72f, 0.72f)" in renderers:
    fail("CHORD-B hard clamp must not return; use soft-knee protection")

for need in [
    "sampleRate * 0.006",
    "targetDelaySamples",
    "delaySmoothing",
    "readFrac",
    "dreamyDelayBuffer.clear()",
]:
    if need not in renderers and need not in dreamy:
        fail(f"noise-hardening contract missing: {need}")

print("[PASS] exact 300-frame visual bank SHA/size verified")
print("[PASS] product source isolated; synth/cross-project tokens absent")
print("[PASS] Android input/JNI/startup contract verified")
print("[PASS] JUCER module set matches proven FLOWER Golden")
print("[PASS] JUCE 9.0.2 dependency closure verified")
print("[PASS] Android RECORD_AUDIO exporter contract verified")
print("[PASS] known-good AN-10..AN-15 CircleCI controls preserved")
print("[PASS] pinned Clouds DSP / musical XY / attribution contracts verified")
print("[PASS] CHORD-B continuously-smoothed cents-offset contract verified")
print("[PASS] EUREKA click-hardening / output-safety contracts verified")
print("[PASS] three global preset slots verified")
# Vongon video mosaics follow pitched audio, NOT incoming MIDI notes.
# Existing CHORD/DREAMY/EUREKA pitch tracking stays independent.
video_logic = (R / "Source/VideoInteraction.h").read_text()
processor_visual = (R / "Source/PluginProcessor.cpp").read_text()
editor_visual = (R / "Source/PluginEditor.cpp").read_text()
editor_header_visual = (R / "Source/PluginEditor.h").read_text()
for need in [
    "divisionsForPitchHz (float hz)",
    "class AudioMosaicGate",
    "randomState % 5u == 0u",
    "holdDuration = 75",
    "quietToRearm = 3",
    "vongonModeForXY (float x, float y)",
    "localX (float x)",
    "localY (float y)",
]:
    if need not in video_logic:
        fail(f"Audio pitch video/MULTI mapping missing: {need}")
if 'file="Source/VideoInteraction.h"' not in jucer_text:
    fail("Visual interaction helper not declared in JUCE project")
for need in [
    "videoPitchDetector.prepare (currentSampleRate / 4.0, 1024, 512)",
    "videoPitchDetector.pushSample (decimated)",
    "videoPitchAccumulator += mono",
    "videoPitchStableCount >= 2",
    "effects_video::divisionsForPitchHz (estimate.hz)",
    "audioTileDivisions.store (",
    "currentSampleRate * 0.45",
]:
    if need not in processor_visual:
        fail(f"Audio pitch mosaic signal route missing: {need}")
if "shouldDivideOnNote" in processor_visual or "visualMidiSequence" in processor_visual:
    fail("Obsolete MIDI Note On mosaic trigger still active")
if "videoPitchDetector" not in (R / "Source/PluginProcessor.h").read_text():
    fail("Independent low-rate video pitch detector missing")
for need in [
    "paintEffectsMenu (juce::Graphics& g)",
    "paintEffectsShortcut (juce::Graphics& g)",
    "chooseEffectFromMenu (int index)",
    "drawAudioVideoTiles (juce::Graphics& g)",
    "startMultiRecording()",
    "stopMultiRecordingAndPlay()",
    "clearMultiRecording()",
    "multiPointerDownMs = juce::Time::getMillisecondCounterHiRes()",
    "heldMs >= 700.0 && ! multiPointerMoved",
    "multiTransport == 1",
    "multiTransport == 2",
    "processor.getAudioTileDivisions()",
    "audioMosaicGate.advance (",
    "audioTileDivisions = tiles.divisions",
    "if (tiles.started)",
    "drawAudioVideoTiles (g)",
    "effects_video::vongonModeForXY (multiX, multiY)",
    "processor.setVongonXY (",
    'g.drawText ("EFFECTS", 499, 22, 84, 24',
    '"CHORD-A", "CHORD-B", "DREAMY", "EUREKA"',
    '"CHORDBOT", "CLOUDS", "ULTRASHEER", "ROSETONES"',
    '"SOLARFAZE", "POLYPHRASE", "MULTI"',
]:
    if need not in editor_visual:
        fail(f"EFFECTS direct menu/MULTI recorder contract missing: {need}")
for need in ["multiModeEnabled", "multiRecordedXY", "multiTransport",
             "effectsMenuVisible", "audioTileEventSequence"]:
    if need not in editor_header_visual:
        fail(f"Visual control/editor state missing: {need}")
# The 720px top bar keeps BOOST at 300..376, mode settings at
# 380..441, DRY at 438..495, EFFECTS at 496..590, CONFIG beyond 590.
# The settings/DRY text areas overlap only minimally by design.
if 'boostBounds (300.0f' in editor_visual:
    fail("Obsolete BOOST drawing constructor reappeared")
if 'const juce::Rectangle<float> boostBounds (\n        300.0f' not in editor_visual:
    fail("BOOST header location not updated for DRY shortcut")
# CLOUDS video must not obscure original frames with upper/lower
# black curtains or an XY crosshair. Other modes retain their own UI.
cloud_start = editor_visual.find(
    "void RealtimeChordFxAudioProcessorEditor::paintClouds (")
cloud_end = editor_visual.find(
    "void RealtimeChordFxAudioProcessorEditor::paintEffectsShortcut (", cloud_start)
if cloud_start < 0 or cloud_end < 0:
    fail("CLOUDS painter boundaries missing")
cloud_paint = editor_visual[cloud_start:cloud_end]
for obsolete in [
    "g.fillRect (0.0f, 0.0f, 720.0f, 86.0f)",
    "g.fillRect (0.0f, 560.0f, 720.0f, 160.0f)",
    "g.drawLine (px, 72.0f, px, 558.0f",
    "g.drawLine (0.0f, py, 720.0f, py",
    "g.fillRect (px - 4.0f, py - 4.0f, 8.0f, 8.0f)",
]:
    if obsolete in cloud_paint:
        fail(f"CLOUDS video masking/crosshair regression: {obsolete}")
# An audio-pitch-driven 2..10x grid must display actual moving film,
# not three static JPEGs decoded just once at the trigger.
tile_start = editor_visual.find(
    "void RealtimeChordFxAudioProcessorEditor::drawAudioVideoTiles (")
tile_end = editor_visual.find(
    "void RealtimeChordFxAudioProcessorEditor::applyMultiXY (", tile_start)
if tile_start < 0 or tile_end < 0:
    fail("Audio tile rendering boundaries missing")
tile_paint = editor_visual[tile_start:tile_end]
for required in [
    "audioTileLastVongonRevision != dotVongonRevision",
    "const int slot = audioTileAlternateUpdateSlot++ % 3",
    "audioTileAlternateFrames[(size_t) slot]",
    "effects_video::tileSourceVariant (",
    "0, 0, picture.getWidth(), picture.getHeight()",
]:
    if required not in tile_paint:
        fail(f"Live mosaic video tiles missing: {required}")

# Global DRY is explicit, saved in processor parameters, and independent
# of generated wet effects. All modes—including MIDI-only CHORDBOT—use
# the same after-FX tap; its default zero preserves approved wet-only audio.
for required in [
    'ParamID::dryLevel, "DRY", 0.0f, 1.0f, 0.0f',
    "dryInputTap.copyFrom (channel, 0, buffer",
    "const bool dryTapActive = dryTarget > 0.0f || dryLevelSmoothed > 0.0f",
    "softProtectSample (wetSample",
    "rawInput * dryLevelSmoothed",
]:
    if required not in processor_controls:
        fail(f"Optional 0-default global DRY mix missing: {required}")
if 'inline constexpr const char* dryLevel = "dryLevel";' not in parameter_ids:
    fail("Global DRY parameter ID missing")
for required in [
    "paintDryPopup (juce::Graphics& g)",
    "setDryFromPoint (float x)",
    'g.drawText ("DRY", 444, 22, 46, 24',
    "if (dryPopupVisible)",
]:
    if required not in editor_visual:
        fail(f"All-mode DRY control unavailable: {required}")
for obsolete in [
    "const int variant = (col + 2 * row) % 4",
    "constexpr int clipOffsets[3] = { 2, 5, 0 }",
]:
    if obsolete in editor_visual:
        fail(f"Fixed example tile order is prohibited: {obsolete}")
for required in [
    "tileHash (std::uint32_t seed)",
    "tileSourceVariant (int row, int col, int divisions,",
    "tileAlternateClipOffset (int slot, int clipCount,",
    "tileAlternateSeek (int slot, int framesPerClip,",
]:
    if required not in video_logic:
        fail(f"Randomized full-video tile source mapping absent: {required}")

if "dryPopupVisible" not in editor_header_visual:
    fail("DRY popup missing from editor state")

# Regression: one ~20% probability draw on an audible onset, but the
# count of video tiles is still governed by the detected input pitch.
# No MIDI drawing gate or performance-video diagnostic overlay.
timer_start = editor_visual.find("void RealtimeChordFxAudioProcessorEditor::timerCallback()")
timer_end = editor_visual.find("void RealtimeChordFxAudioProcessorEditor::paint (", timer_start)
if timer_start < 0 or timer_end < 0:
    fail("Audio-pitch video tile timer boundaries missing")
timer_code = editor_visual[timer_start:timer_end]
# Verify the audio-driven mosaic update in its actual timer section.
# MULTI long-press CLEAR is handled earlier in the same callback.
pitch_tile_start = timer_code.find("    const int effectMode = juce::jlimit")
pitch_tile_end = timer_code.find("    if ((effectMode == 0 || effectMode == 4)", pitch_tile_start)
if pitch_tile_start < 0 or pitch_tile_end < 0:
    fail("Audio pitch tile timer event-section boundary missing")
pitch_tile_code = timer_code[pitch_tile_start:pitch_tile_end]
for required in [
    "audioMosaicGate.advance (",
    "audioTileDivisions = tiles.divisions",
    "if (tiles.started)",
    "++audioTileEventSequence",
]:
    if required not in pitch_tile_code:
        fail(f"Live audio pitch cannot update video grid: {required}")
for obsolete in [
    "getVisualMidiSequence()", "getVisualMidiNote()",
    "shouldDivideOnNote", "audioTileHoldTicks", "midiTileDivisions",
]:
    if obsolete in pitch_tile_code:
        fail(f"Obsolete MIDI mosaic trigger remains: {obsolete}")
paint_start = editor_visual.find("void RealtimeChordFxAudioProcessorEditor::paintVongonMode (")
paint_end = editor_visual.find("void RealtimeChordFxAudioProcessorEditor::paintVongonParameterPanel (", paint_start)
if paint_start < 0 or paint_end < 0:
    fail("Vongon mosaic painting boundaries missing")
paint = editor_visual[paint_start:paint_end]
# The correct behavior is a full-opacity replacement video canvas, not
# a semitransparent overlay on top of the original fullscreen picture.
for required in [
    "if (audioTileDivisions > 1 && currentVongonFrame.isValid())",
    "drawAudioVideoTiles (g);",
    "else\n    {\n        if (currentVongonFrame.isValid())",
    "paintMediaFrame (",
    "paintVideoEffect (",
]:
    if required not in paint:
        fail(f"Vongon replace-not-overlay mosaic paint missing: {required}")
mosaic_branch_end = paint.find("    else\n    {\n        if (currentVongonFrame.isValid())")
if mosaic_branch_end < paint.find("drawAudioVideoTiles (g);"):
    fail("Video mosaic is not painted before normal full-frame branch")
mosaic_branch = paint[:mosaic_branch_end]
if "paintVideoEffect (" in mosaic_branch or "paintMediaFrame (" in mosaic_branch:
    fail("Full video/VHS still drawn underneath audio-pitch mosaic")
for required in [
    "g.setOpacity (1.0f)",
    "g.setColour (juce::Colours::black)",
    "g.fillRect (0.0f, 0.0f, 720.0f, 720.0f)",
    "effects_video::tileSourceVariant (",
    "0, 0, picture.getWidth(), picture.getHeight()",
]:
    if required not in tile_paint:
        fail(f"Opaque full-screen audio-pitch video tiles missing: {required}")




# Pure Data Vanilla import is the PATCH contract; the rejected JSON DSL
# must never be reintroduced. The existing 0..8 effects stay numbered.
pd_header = (R / "Source/EffectsPureDataEngine.h").read_text()
pd_patch = (R / "patches/Effects_Simple_Stereo_Delay.pd").read_text()
pd_get = (R / "scripts/materialize_effects_libpd.sh").read_text()
pd_cmake = (R / "scripts/patch_android_effects_libpd.py").read_text()
for token in [
    "libpd_init_audio (2, 2,",
    "libpd_process_float (",
    "libpd_openfile (",
    "libpd_closefile (",
    "ScopedTryLockType",
    "std::array<float, tickFrames * 2>",
]:
    if token not in pd_header:
        fail(f"Pd native DSP contract missing: {token}")
# Never store raw UTF-8 pointers produced by temporary juce::String values.
# Such pointers dangle before libpd_openfile() and can crash patch imports.
for token in [
    "const juce::String filename = file.getFileName();",
    "const juce::String filename = selectedFile.getFileName();",
    "filename.toRawUTF8(), folder.toRawUTF8()",
]:
    if token not in pd_header:
        fail(f"Pd filepath lifetime guard missing: {token}")
for unsafe in [
    "file.getFileName().toRawUTF8();",
    "file.getParentDirectory().getFullPathName().toRawUTF8();",
]:
    if unsafe in pd_header:
        fail(f"Pd filepath has dangling UTF-8 pointer: {unsafe}")
# Organelle control compatibility uses fixed MIDI/XY input only:
# no extra UI or permanent mode. Official receivers are notes/knob1..4/aux
# and enc/encbut for selector. Test the bridge independently of a fake mother.
for token in [
    'libpd_finish_list ("notes")',
    'libpd_float ("knob1", x)', 'libpd_float ("knob2", y)',
    'event.number >= 21 && event.number <= 24',
    'libpd_float (knob[event.number - 21],',
    'libpd_float ("aux", event.value >= 64 ? 1.0f : 0.0f)',
    'libpd_float ("encbut", pressed)',
    'libpd_float ("enc", event.value >= 64 ? 1.0f : 0.0f)',
    'libpd_controlchange (event.channel, event.number, event.value)',
]:
    if token not in pd_header:
        fail(f"Organelle fixed MIDI/XY receiver bridge missing: {token}")
# PATCH-only scope and stable-mode contract.
if "if (effectMode == 9)\n        pdEngine.captureMidi (midi);" not in processor_visual:
    fail("Organelle MIDI bridge must be PATCH-only")
# PATCH MIDI: notes remain 0-based at the libpd API boundary and MIDI
# START/STOP/CONTINUE/CLOCK reach [realtimein]; XY reaches Vanilla [r].
for token in [
    "void captureMidi (const juce::MidiBuffer& midi)",
    "message.isNoteOnOrOff()", "message.isMidiStart()",
    "message.isMidiStop()", "message.isMidiContinue()",
    "message.isMidiClock()",
    "libpd_noteon (event.channel, event.number, event.value)",
    "libpd_sysrealtime (0, 0xFA)", "libpd_sysrealtime (0, 0xFC)",
    "libpd_sysrealtime (0, 0xFB)", "libpd_sysrealtime (0, 0xF8)",
    'libpd_float ("effects-x", x)', 'libpd_float ("effects-y", y)',
    'libpd_float ("effects-touch", touch)',
    'libpd_float ("effects-transport", 1.0f)',
    'libpd_float ("effects-transport", 0.0f)',
    "std::array<MidiEvent, 512> pendingMidi",
    "std::array<std::array<bool, 128>, 16> activeNotes",
]:
    if token not in pd_header:
        fail(f"Pure Data MIDI/XY bridge missing: {token}")
if ("if (effectMode == 9)\n        pdEngine.captureMidi (midi);" not in processor_visual
        or processor_visual.index("pdEngine.captureMidi (midi);") >
           processor_visual.index("midi.clear();")):
    fail("PATCH input MIDI must be captured before EFFECTS clears MIDI IN")
if "pdEngine.process (buffer," not in processor_visual:
    fail("PATCH XY must be passed to libpd render")
pd_synth_fixture = (R / "patches/Effects_MIDI_XY_Transport_Test_Synth.pd").read_text()
if pd_synth_fixture.count("#X obj ") != 20 or pd_synth_fixture.count("#X connect ") != 17:
    fail("MIDI/XY test synth fixture graph corrupted")
for token in ("notein;", "r effects-x;", "r effects-y;", "r effects-transport;",
              "r effects-start;", "r effects-stop;", "r effects-continue;",
              "r effects-clock;", "dac~ 1 2;"):
    if token not in pd_synth_fixture:
        fail(f"MIDI/XY test synth lacks Vanilla object: {token}")
if "tests/effects_pd_midi_xy_regression.cpp" not in (R / ".circleci/config.yml").read_text():
    fail("Missing real libpd MIDI/XY host audio regression")
if "EFFECTS_PATCH_V1" in editor_visual or "importPatchJson" in processor_visual:
    fail("Rejected custom JSON patch DSL leaked into Pd implementation")
for token in [
    "importPureDataPatch (",
    "restorePureDataPatchFromState()",
    "pdEngine.prepare (currentSampleRate)",
    "pdEngine.process (buffer,",
    "Effects_Simple_Stereo_Delay_pd",
    "pdPatchFile",
]:
    if token not in processor_visual:
        fail(f"Pd import / persistence bridge missing: {token}")
for token in [
    "choosePureDataPatchFile()",
    "pdChooser->launchAsync (",
    "importPureDataPatch (file, error)",
    "paintPureDataMode",
    "eurekaFrames.getFrame (pdFrameIndex)",
    "vongonFrames.getFrame (pdFrameIndex)",
    "const int totalClips = euCount + vgCount",
    '"LOAD .PD"',
]:
    if token not in editor_visual:
        fail(f"Pure Data CONFIG/video bridge missing: {token}")
for token in [
    "EffectsPureDataEngine.h",
    'file="patches/Effects_Simple_Stereo_Delay.pd"',
]:
    if token not in jucer_text:
        fail(f"Pd source/asset missing from JUCER: {token}")
if not (pd_get.find("ba0dc63262901d658af8bbda5e619a60fa975e78") >= 0
        and pd_get.find("f009fd8d7b537e209e09898d487fdf1bf547da2b") >= 0):
    fail("Pinned libpd 0.16.1 / Vanilla submodule must be exact")
for token in ["libpd_static", "add_subdirectory(", "target_link_libraries",
              "JUCE", "EFFECTS_PINNED_LIBPD_LINK_BEGIN"]:
    if token not in pd_cmake:
        fail(f"libpd Android CMake bridge missing: {token}")
# #221: original Projucer links juce_jni with plain target_link_libraries.
# Mixing the PRIVATE keyword with that same target prevents CMake generation.
if 'target_link_libraries(${BINARY_NAME} libpd_static m dl)' not in pd_cmake:
    fail("libpd link must match generated Projucer plain CMake signature")
if 'target_link_libraries(${BINARY_NAME} PRIVATE libpd_static m dl)' in pd_cmake:
    fail("libpd keyword signature conflicts with original Projucer link")
if "unexpected keyword link signature" not in pd_cmake:
    fail("Projucer signature drift guard missing")

# #222: bionic API24-27 cannot provide glob/globfree (API28+). Keep minSdk24.
glob_src = (R / "Source/compat/android_api24_glob.c").read_text()
glob_hdr = (R / "Source/compat/android_api24_glob.h").read_text()
if 'androidMinimumSDK="24"' not in jucer_text:
    fail("Android minSDK must not be raised to hide API24 libc errors")
for token in ("__ANDROID_API__ < 28", "int glob(", "void globfree(", "GLOB_NOMATCH", "fnmatch("):
    if token not in glob_src:
        fail(f"Android API24-27 Pd glob implementation missing: {token}")
for token in ("__ANDROID_API__ < 28", "#include <glob.h>", "int glob(", "void globfree("):
    if token not in glob_hdr:
        fail(f"Android API24-27 Pd glob declaration missing: {token}")
for token in ("target_sources(libpd_static PRIVATE", "android_api24_glob.c",
              "target_compile_options(libpd_static PRIVATE", "android_api24_glob.h"):
    if token not in pd_cmake:
        fail(f"libpd Android glob CMake link missing: {token}")
for token in ("API24 glob compatibility C regression", "effects_android_glob_regression.c",
              "android-libpd-arm64-test.log", "--target libpd_static -j2"):
    if token not in (R / ".circleci/config.yml").read_text():
        fail(f"Real Android API24 libpd test gate missing: {token}")

if pd_patch.count("#X obj ") != 16 or pd_patch.count("#X connect ") != 18:
    fail("Vanilla stereo delay test patch changed without audio regression")
if ("adc~ 1 2" not in pd_patch or "dac~ 1 2" not in pd_patch
        or "\\$0-delayL" not in pd_patch or "\\$0-delayR" not in pd_patch):
    fail("Pure Data left/right test delay must be independent")
for token in [
    "Real Pure Data libpd audio regression (L and R)",
    "tests/effects_pd_audio_regression.cpp",
    "tests/effects_pd_static_check.py",
    "default: false",
    "manual_android_build:",
]:
    if token not in (R / ".circleci/config.yml").read_text():
        fail(f"Pd audio regression missing from manual CI: {token}")
# Regression: past CMake version mismatch must not be hidden by editing
# upstream libpd's minimum version. Android SDK Gradle and native Pd
# regression must use exactly the explicitly installed supported CMake.
ci_config = (R / ".circleci/config.yml").read_text()
# CircleCI v2.1 interprets every literal << as a template expression.
# #221: the shell form python3 - <<'PY' made the entire workflow invalid.
# Only the existing manual run_build condition is allowed in this config.
circleci_templates = [
    line.strip() for line in ci_config.splitlines() if "<<" in line
]
if circleci_templates != ["when: << pipeline.parameters.run_build >>"]:
    fail("CircleCI 2.1 contains unsupported or unclosed << template tag")
if 'cmake_minimum_required (VERSION 3.22)' in pd_get:
    fail("Forbidden libpd CMake requirement downgrade")
if "grep -Fx 'cmake_minimum_required (VERSION 3.25)'" not in pd_get:
    fail("Pinned libpd upstream CMake requirement not verified")
if 'new = \'version = "3.31.6"\'' not in pd_cmake:
    fail("Generated Projucer Android CMake version not patched to 3.31.6")
if '"cmake;3.31.6"' not in ci_config:
    fail("Android SDK CMake 3.31.6 not installed in CI")
# #220: Gradle silently picked installed NDK 28 despite job exporting 26.
# Always make the generated Gradle project use the installed/pinned toolchain.
if 'ndk = \'ndkVersion "26.1.10909125"\'' not in pd_cmake:
    fail("Generated Android Gradle exporter missing exact NDK pin")
if 'ndkVersion "26.1.10909125"' not in ci_config:
    fail("CircleCI must assert generated Gradle NDK pin")
# #220 had only Java stack-trace highlights. Capture the actual first CMake
# diagnostic and preflight the same native graph before the Gradle task.
for required in [
    "Python syntax and Android exporter regression",
    "python3 -m py_compile",
    "python3 tests/effects_android_exporter_regression.py",
    "Configure Android arm64 CMake before Gradle",
    "android-cmake-configure.log",
    "android.toolchain.cmake",
    "test -s JuceLibraryCode/BinaryData.cpp",
    "test -s JuceLibraryCode/JuceHeader.h",
    "CMake Error|Cannot find source file|No SOURCES given",
]:
    if required not in ci_config:
        fail(f"Native CMake / historical BinaryData preflight missing: {required}")
if ci_config.find("name: Install Android SDK components") > ci_config.find("name: Real Pure Data libpd audio regression"):
    fail("Android CMake installed after Pd native test")
if '"$PD_CMAKE" -S third_party/libpd' not in ci_config:
    fail("Pd native test not using exact CMake binary")
for signature in [
    "CMAKE_BUILD_PARALLEL_LEVEL=2",
    "--max-workers=2",
    "-DCMAKE_JOB_POOLS:STRING=compile=2;link=1",
    "no_output_timeout: 30m",
    "path: audio-regression/pure-data",
]:
    if signature not in ci_config:
        fail(f"Previous CircleCI failure guard lost: {signature}")
if ci_config.count("workflows:") != 1:
    fail("AN-12 regression: duplicate workflows block")
print("[PASS] pinned Android CMake 3.31.6, historic CircleCI safeguards, native DSP logs")
print("[PASS] Real libpd native DSP, .pd import, pinned submodule and combined video contracts")

print("[PASS] Vongon-inspired 4-mode DSP / supplied-video / DOT contracts verified")
print("[PASS] CHORD/Dreamy noise-hardening contracts verified")

# MOOD 13-key chromatic keyboard can select C-1..C9 (full MIDI 0..127).
mood_keyboard_begin = editor.find("int RealtimeChordFxAudioProcessorEditor::moodKeyAtPoint (")
mood_keyboard_end = editor.find("void RealtimeChordFxAudioProcessorEditor::paintMoodMode (", mood_keyboard_begin)
if mood_keyboard_begin < 0 or mood_keyboard_end <= mood_keyboard_begin:
    fail("MOOD keyboard hit-test definition missing")
mood_keyboard = editor[mood_keyboard_begin:mood_keyboard_end]
for required in (
    "constexpr int white[8] = {0, 2, 4, 5, 7, 9, 11, 12};",
    "constexpr int black[5] = {1, 3, 6, 8, 10};",
    "const int baseNote = moodKeyboardBaseNote();",
    "const int note = baseNote + black[i];",
    "const int note = baseNote + white[index];",
    "return juce::isPositiveAndBelow (note, 128) ? note : -1;",
):
    if required not in mood_keyboard:
        fail(f"MOOD octave keyboard mapping contract missing: {required}")
keyboard_header = (R / "Source/PluginEditor.h").read_text()
for required in (
    "int moodKeyboardOctave = 4;",
    "return 12 * (moodKeyboardOctave + 1);",
):
    if required not in keyboard_header:
        fail(f"MOOD octave selector state missing: {required}")
for required in (
    '"OCT-", false',
    '"OCT+", false',
    "moodKeyboardOctave = juce::jmax (-1, moodKeyboardOctave - 1);",
    "moodKeyboardOctave = juce::jmin (9, moodKeyboardOctave + 1);",
    "juce::isPositiveAndBelow (note, 128)",
    '"OCT " + juce::String (moodKeyboardOctave)',
    "processor.moodNote (moodKeyboardNote, false);",
):
    if required not in editor:
        fail(f"MOOD octave selector touch/draw contract missing: {required}")
intervals = (0, 2, 4, 5, 7, 9, 11, 12, 1, 3, 6, 8, 10)
all_mood_notes = set()
for octave in range(-1, 10):
    base_note = 12 * (octave + 1)
    notes = [base_note + n for n in intervals if base_note + n <= 127]
    if len(notes) != len(set(notes)):
        fail(f"MOOD duplicate note in octave {octave}")
    if not notes or min(notes) < 0 or max(notes) > 127:
        fail(f"MOOD octave out of MIDI range: {octave}")
    all_mood_notes.update(notes)
if all_mood_notes != set(range(128)):
    fail("MOOD octave switch does not cover all MIDI notes 0..127")
if [60 + n for n in intervals[:8]] != [60, 62, 64, 65, 67, 69, 71, 72]:
    fail("MOOD default C4-C5 keyboard changed")
# Same 720x720 touch/paint bounds; never overlay LOAD TILES, CC labels or keypad.
button_ranges = ((26, 162), (174, 318), (328, 474), (478, 571), (578, 636), (643, 701))
if any(button_ranges[i][1] > button_ranges[i + 1][0] for i in range(5)):
    fail("MOOD OCT buttons collide with previous button")
for required in (
    "p.x >= 478.0f && p.x < 571.0f",
    "p.x >= 578.0f && p.x < 636.0f",
    "p.x >= 643.0f && p.x < 701.0f",
):
    if required not in editor:
        fail(f"MOOD OCT hitbox missing: {required}")
print("[PASS] MOOD OCT +/-: all MIDI notes 0..127, C4 default, safe hitboxes and note-off")
for midi_parameter in ("midiChannel", "chordMidiChannel"):
    expected_wrap = (
        "setChoiceActual (ParamID::" + midi_parameter
        + ", ((v - 1 + dir + 16) % 16) + 1);")
    if expected_wrap not in editor:
        fail(f"{midi_parameter} must wrap 16->1 and 1->16")
for channel in range(1, 17):
    if ((channel - 1 + 1 + 16) % 16) + 1 != (channel % 16) + 1:
        fail("MIDI channel increment regression")
    if ((channel - 1 - 1 + 16) % 16) + 1 != (channel + 14) % 16 + 1:
        fail("MIDI channel decrement regression")
print("[PASS] MOOD 13 distinct MIDI notes and both MIDI channels wrap 1..16")

# MOOD touchscreen notes must survive taps faster than a single audio buffer.
for marker in (
    "static constexpr unsigned int moodNoteQueueSize = 64;",
    "std::array<std::atomic<int>, moodNoteQueueSize> moodNoteQueue",
    "moodNoteQueue[(size_t) (write % moodNoteQueueSize)].store",
    "moodNoteWrite.store (write + 1, std::memory_order_release);",
    "note | (down ? 0x100 : 0)",
):
    if marker not in processor_header:
        fail(f"MOOD note queue source missing: {marker}")
note_handler_begin = processor.find("void RealtimeChordFxAudioProcessor::processMoodMidi (")
note_handler_end = processor.find("void RealtimeChordFxAudioProcessor::processMidiController (", note_handler_begin)
if note_handler_begin < 0 or note_handler_end <= note_handler_begin:
    fail("MOOD MIDI note dispatch function missing")
note_handler = processor[note_handler_begin:note_handler_end]
for marker in (
    "const unsigned int read = moodNoteRead.load",
    "const unsigned int write = moodNoteWrite.load",
    "moodNoteRead.store (read + 1, std::memory_order_release);",
    "const int key = event & 127;",
    "const bool keyDown = (event & 0x100) != 0;",
    "channel, key, (juce::uint8) 100",
    "else if (moodLastSentNote == key)",
    "moodLastSentChannel, moodLastSentNote",
):
    if marker not in note_handler:
        fail(f"MOOD separate MIDI Note On/Off queue contract missing: {marker}")
if "const int note = enabled ? moodUiNote.load" in note_handler:
    fail("MOOD may not collapse fast key down/up into a single state")
print("[PASS] MOOD key transitions queued across audio callbacks without missing short taps")

# MOOD 2026-10-31: MULTI-style XY recorder, panel navigation and hardware CC mapping.
for required in (
    "int moodTransport = 0;",
    "static constexpr int moodRecordCapacity = 1800;",
    "std::array<juce::Point<float>, moodRecordCapacity> moodRecordedXY",
    "int moodRecordCount = 0;",
    "bool moodPanelVisible = true;",
    "void startMoodRecording();",
    "void stopMoodRecordingAndPlay();",
    "void clearMoodRecording();",
    "void applyMoodXY (float x, float y, bool fromPlayback = false);",
):
    if required not in editor_header:
        fail(f"MOOD MULTI-style recorder/panel state missing: {required}")
for required in (
    "if (moodTransport == 1)",
    "moodRecordedXY[(size_t) moodRecordCount++]",
    "moodRecordCount == moodRecordCapacity",
    "moodPlaybackIndex = (moodPlaybackIndex + 1) % moodRecordCount;",
    "applyMoodXY (xy.x, xy.y, true);",
    "moodPointerDownMs = juce::Time::getMillisecondCounterHiRes();",
    "if (heldMs >= 700.0)",
    "clearMoodRecording();",
    "startMoodRecording();",
    "stopMoodRecordingAndPlay();",
    "moodPanelVisible = false;",
    "moodPanelVisible = true;",
    "if (! moodPanelVisible)",
    '"CLOSE", false',
    '"PANEL"',
    '"OCT+", false',
    '"OCT-", false',
):
    if required not in editor:
        fail(f"MOOD XY REC/PLAY, panel CLOSE or octave regression: {required}")
for required in (
    "moodCaptureSetByUser.store (true",
    "static int moodMappedCc (int cc, int value) noexcept",
    "value < 43 ? 1 : value < 85 ? 2 : 3",
    "int getMoodAxisValue (bool xAxis) const noexcept",
):
    if required not in processor_header:
        fail(f"MOOD hardware MIDI CC mapping contract missing: {required}")
for required in (
    "changedChannel && moodCaptureSetByUser.load",
    "const bool captureChanged = moodCaptureDirty.exchange",
    "const int value = moodMappedCc (cc, juce::roundToInt (",
):
    if required not in processor:
        fail(f"MOOD pedal bypass or three-step CC regression: {required}")
if "if (changedChannel || moodCaptureDirty.exchange" in processor:
    fail("MOOD must not auto-send CC102=0 just by entering the mode")
if "moodProcessor.process (buffer);" in processor:
    fail("MOOD must remain external MIDI only")
if editor.count("void RealtimeChordFxAudioProcessorEditor::applyMoodXY (") != 1:
    fail("MOOD XY playback method duplication")
if "processChordMidi (midi);\n        processChordBotMidi (midi);" not in processor:
    fail("CHORDBOT entry ordering regression")
print("[PASS] MOOD XY recording/playback, CLOSE/PANEL, conditional CC102 and 3-position toggles")

# MOOD MIDI Note input enters hardware Synth Mode. A released software key
# must exit with CC59 value 2, not leave the physical pedal muted in Synth Mode.
for marker in (
    "bool moodUiSynthSessionActive = false;",
):
    if marker not in processor_header:
        fail(f"MOOD synth session state missing: {marker}")
mood_cc59_start = processor.find("void RealtimeChordFxAudioProcessor::processMoodMidi (")
mood_cc59_end = processor.find("void RealtimeChordFxAudioProcessor::processMidiController (", mood_cc59_start)
if mood_cc59_start < 0 or mood_cc59_end <= mood_cc59_start:
    fail("MOOD MIDI synth exit function boundaries missing")
mood_cc59 = processor[mood_cc59_start:mood_cc59_end]
for marker in (
    "moodUiSynthSessionActive = true;",
    "moodUiSynthSessionActive && moodLastSentNote < 0",
    "moodUiNote.load (std::memory_order_acquire) < 0",
    "moodNoteRead.load (std::memory_order_relaxed)",
    "== moodNoteWrite.load (std::memory_order_acquire)",
    "moodLastSentChannel, 59, 2",
    "moodUiSynthSessionActive = false;",
):
    if marker not in mood_cc59:
        fail(f"MOOD software keyboard must exit Synth Mode after final note: {marker}")
if "moodLastSentChannel, 103" in mood_cc59:
    fail("MOOD keyboard must never toggle WET bypass")
if "moodLastSentChannel, 102" in mood_cc59:
    fail("MOOD keyboard must never toggle MICRO-LOOPER bypass")
if mood_cc59.index("moodUiSynthSessionActive && moodLastSentNote < 0") > mood_cc59.index("const bool changedChannel = moodLastCcChannel != channel;"):
    fail("MOOD Synth Mode exit happens after unrelated CC updates")
print("[PASS] MOOD final-key Note Off exits Synth Mode via CC59=2 without pedal bypass")

# MOOD MKII extra 90-user-photo frames: preserve the Golden Baseline's 343
# JPEG payloads, reconstruct all 433 only from committed source assets.
materializer = (R / "scripts/materialize_frame_pack.py").read_text()
for required in (
    "MOOD_COUNT = 343",
    "MOOD_ADDED_PARTS = 16",
    "MOOD_ADDED_COUNT = 90",
    "MOOD_FINAL_COUNT = MOOD_COUNT + MOOD_ADDED_COUNT",
    "MOOD_ADDED_BYTES = 568047",
    "MOOD_FINAL_SIZE = 2785693",
    "MOOD_FINAL_SHA256 = 'ab78e1ae79d3b7f812ded6e3f7c0295e617e2742fc22c315a9c80fe5273cdb77'",
    "mood[old_header_size:] + added",
    "MOOD_OUT.write_bytes(mood_combined)",
):
    if required not in materializer:
        fail(f"MOOD 433-frame integration contract drift: {required}")
mood_added_dir = R / "Resources/mood_frame_additions"
expected_assets = [mood_added_dir / f"mood_add_{n:02}.b64" for n in range(16)]
for asset in expected_assets:
    if not asset.is_file():
        fail(f"MOOD 433-frame source asset not committed: {asset.relative_to(R)}")
sizefile = mood_added_dir / "mood_append_sizes.txt"
if not sizefile.is_file():
    fail("MOOD appended size manifest missing")
mood_sizes = [int(v) for v in sizefile.read_text(encoding="ascii").split()]
if len(mood_sizes) != 90 or sum(mood_sizes) != 568047:
    fail("MOOD 90-frame appended sizes manifest mismatch")
print("[PASS] MOOD 433-frame immutable source assets and 90 JPEG size entries")
