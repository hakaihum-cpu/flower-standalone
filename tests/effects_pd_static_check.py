#!/usr/bin/env python3
"""Lightweight Pd source and native integration preflight before Android build."""
from pathlib import Path
import re
r=Path(__file__).resolve().parents[1]
pd=(r/"patches/Effects_Simple_Stereo_Delay.pd").read_text()
nodes=re.findall(r"^#X obj [^\n]+;\s*$",pd,re.M)
wires=[tuple(map(int,m.groups())) for m in re.finditer(
    r"^#X connect (\d+) (\d+) (\d+) (\d+);$",pd,re.M)]
assert pd.startswith("#N canvas ") and len(nodes)==16
assert len(wires)==18
assert any("adc~ 1 2;" in n for n in nodes)
assert any("dac~ 1 2;" in n for n in nodes)
assert sum("delwrite~" in n for n in nodes)==2
assert sum("delread~" in n for n in nodes)==2
assert all(0<=a<16 and 0<=c<16 for a,b,c,d in wires)
# Delays have separated write/read state, one per input channel.
assert (0,0,1,0) in wires and (0,1,8,0) in wires
assert (7,0,15,0) in wires and (14,0,15,1) in wires
src=(r/"Source/EffectsPureDataEngine.h").read_text()
cpp=(r/"Source/PluginProcessor.cpp").read_text()
editor=(r/"Source/PluginEditor.cpp").read_text()
for required in ("libpd_init_audio (2, 2", "libpd_openfile",
 "libpd_process_float (", "ScopedTryLockType",
 "tickFrames = 64", "getSample (0, n)", "getSample (1, n)",
 "setSample (\n                0, n", "setSample (\n                1, n"):
    assert required in src, required
for required in ("pdEngine.prepare (currentSampleRate)",
 "pdEngine.process (buffer,", "importPureDataPatch",
 '"pdPatchFile"', '"POLYPHRASE", "PATCH"'):
    assert required in cpp, required
for required in ("choosePureDataPatchFile", '"*.pd"', 
 "const int totalClips = euCount + vgCount", "paintPureDataMode (g)",
 "LOAD .PD"):
    assert required in editor, required
assert "EFFECTS_PATCH_V1" not in src+cpp+editor

# Transport / note / XY bridge remains exclusive to PATCH (index 9).
for required in (
    "void captureMidi (const juce::MidiBuffer& midi)",
    "message.isNoteOnOrOff()", "message.isMidiStart()",
    "message.isMidiStop()", "message.isMidiContinue()",
    "message.isMidiClock()",
    "libpd_noteon (event.channel, event.number, event.value)",
    "libpd_sysrealtime (0, 0xFA)",
    "libpd_sysrealtime (0, 0xFB)",
    "libpd_sysrealtime (0, 0xFC)",
    "libpd_sysrealtime (0, 0xF8)",
    'libpd_float ("effects-x", x)',
    'libpd_float ("effects-y", y)',
    'libpd_float ("effects-touch", touch)',
    'libpd_float ("effects-transport", 1.0f)',
    'libpd_float ("effects-transport", 0.0f)',
    'libpd_bang ("effects-clock")',
    "std::array<MidiEvent, 512> pendingMidi",
    "std::array<std::array<bool, 128>, 16> activeNotes",
):
    assert required in src, required
for required in (
    "if (effectMode == 9)\n        pdEngine.captureMidi (midi);",
    "midi.clear();",
    "pdEngine.process (buffer,",
    "vongonX.load (std::memory_order_relaxed)",
    "vongonY.load (std::memory_order_relaxed)",
    "vongonTouch.load (std::memory_order_relaxed)",
):
    assert required in cpp, required
assert cpp.find("pdEngine.captureMidi (midi);") < cpp.find("midi.clear();")
fixture=(r/"patches/Effects_MIDI_XY_Transport_Test_Synth.pd").read_text()
assert fixture.startswith("#N canvas ") and fixture.count("#X obj ") == 20
assert fixture.count("#X connect ") == 17
for required in ("notein;", "osc~;", "r effects-x;",
                 "r effects-y;", "r effects-transport;",
                 "r effects-start;", "r effects-stop;",
                 "r effects-continue;", "r effects-clock;", "dac~ 1 2;"):
    assert required in fixture, required
ci=(r/".circleci/config.yml").read_text()
assert "tests/effects_pd_midi_xy_regression.cpp" in ci
assert "midi-xy-test.log" in ci
assert ci.count("workflows:") == 1
print("[PASS] real Pd MIDI notes, transport, clock and XY PATCH bridge preflight")
print("[PASS] Pd Vanilla test patch graph, stereo audio route and UI static preflight")
