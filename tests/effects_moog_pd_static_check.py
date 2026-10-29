#!/usr/bin/env python3
"""Static preflight for the CA-72 Pd external (not an audio test)."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
patch = (root / "patches/Minimoog_ModelD_CA72_Circuit.pd").read_text()
c = (root / "Source/MoogPdExternal.c").read_text()
rust = (root / "native/moog_model_d/src/lib.rs").read_text()
cargo = (root / "native/moog_model_d/Cargo.toml").read_text()
engine = (root / "Source/EffectsPureDataEngine.h").read_text()
android = (root / "scripts/patch_android_effects_libpd.py").read_text()
ci = (root / ".circleci/config.yml").read_text()

assert patch.startswith("#N canvas ")
nodes = [line for line in patch.splitlines()
         if re.match(r"^#X (obj|text|msg) ", line)]
wires = [tuple(map(int, m.groups())) for m in
         re.finditer(r"^#X connect (\d+) (\d+) (\d+) (\d+);$",
                     patch, re.M)]
assert len(nodes) == 27 and len(wires) == 23
assert all(0 <= a < len(nodes) and 0 <= b < len(nodes)
           for a,ao,b,bi in wires)
for v in ("notein;", "moog_model_d~ 8;", "dac~ 1 2;",
          "r effects-x;", "r effects-y;", "r knob3;", "r knob4;"):
    assert v in patch, v
assert c.count('gensym("moog_model_d~")') == 1
assert 'class_addmethod(moog_model_d_class,(t_method)moog_note' in c
assert 'class_addmethod(moog_model_d_class,(t_method)moog_parameter' in c
assert 'ca72_pd_process(x->engine' in c
assert 'sp[1]->s_vec,sp[2]->s_vec' in c
assert 'extern "C" void moog_model_d_tilde_setup(void);' in engine
assert engine.count("        moog_model_d_tilde_setup();") == 1
assert "ca72::voice::{Panel, Priority, Quality, Voice, Waveform}" in rust
assert "voice.tick()" in rust
assert 'rev = "cfd5859e913b4969e5416abd026cce135c37ffad"' in cargo
assert "EFFECTS_MOOG_MODEL_D_NATIVE_LINK_BEGIN" in android
assert "target_link_libraries(@@" not in android
assert 'target_link_libraries(${BINARY_NAME}' in android
assert "PD_EXTRA OFF" in android
assert ci.count("manual_android_build:") == 1
assert "when: << pipeline.parameters.run_build >>" in ci
assert ci.count("Build pinned Model D circuit bridge") == 1

print("[PASS] CA-72 Model D Pd graph and native bridge static preflight")
print("[NOTE] Android build and realtime performance are not yet verified")
