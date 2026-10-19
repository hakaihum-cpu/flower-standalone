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
 "pdEngine.process (buffer)", "importPureDataPatch",
 '"pdPatchFile"', '"POLYPHRASE", "PATCH"'):
    assert required in cpp, required
for required in ("choosePureDataPatchFile", '"*.pd"', 
 "const int totalClips = euCount + vgCount", "paintPureDataMode (g)",
 "LOAD .PD"):
    assert required in editor, required
assert "EFFECTS_PATCH_V1" not in src+cpp+editor
print("[PASS] Pd Vanilla test patch graph, stereo audio route and UI static preflight")
