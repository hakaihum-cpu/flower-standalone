#!/usr/bin/env python3
"""Static preflight before the explicit CircleCI FM-1 build. No target build here."""
from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
engine = (root / "fm1" / "karma" / "eng_miyako.c").read_text(encoding="utf-8")
patcher = (root / "fm1" / "karma" / "apply_port.py").read_text(encoding="utf-8")
verifier = (root / "fm1" / "karma" / "verify_port.py").read_text(encoding="utf-8")
dot = (root / "fm1" / "karma" / "mi_dot_ui.c").read_text(encoding="utf-8")
boot = (root / "fm1" / "karma" / "mi_boot_anim.c").read_text(encoding="utf-8")
splash = (root / "fm1" / "karma" / "mi_splash.c").read_text(encoding="utf-8")
manifest = (root / "fm1" / "PORT_MANIFEST.md").read_text(encoding="utf-8")
ci = (root / ".circleci" / "config.yml").read_text(encoding="utf-8")

def require(ok, why):
    if not ok:
        raise SystemExit(f"FAIL {why}")

for token in ('"SINE"', '"SAW"', '"SQR"', '"PULSE"', '"REEL"', '"EUREKA"', '"CRUSH"',
              'ENG_MIYAKO', 'miyako_note_on', 'miyako_render', 'miyako_post',
              'delay_q8'):
    require(token in engine, f"MIYAKO engine missing: {token}")
for token in ('eng_miyako.c', 'mi_dot_ui.c', 'mi_boot_anim.c', 'mi_splash.c',
              'ENG_PRISM', 'ENG_ELEM', 'cloud.c',
              'sizeof(miyako_slot_t)', '&ENG_MIYAKO', 'if (e == ENGI_ACID) return 0;'):
    require(token in patcher + verifier, f"native KARMA port missing: {token}")
for token in ('Plaits', 'Elements', 'Clouds'):
    require(token in manifest, f"Mutable retention omitted: {token}")
for token in ('Vongon', 'EP-Sampler', 'Knowing', 'Tape', 'Twilight', 'Rings', 'Warps'):
    require(token in manifest, f"requested exclusion omitted: {token}")
# Reject target-incompatible APIs in code, not in explanatory comments.
# The former raw-text scan incorrectly rejected "new slice"/"new one" comments.
def code_only(source):
    return re.sub(r'/\*[\s\S]*?\*/|//[^\n]*', '', source)

# Regression: explanatory prose must not masquerade as dynamic allocation;
# an actual allocation in C code must still be rejected.
require('new ' not in code_only('/* fade the new one */\n// new slice\nvoid ok(void) {}'),
        "comment stripping regression: false-positive new")
require('malloc(' in code_only('void bad(void) { malloc(4); }'),
        "comment stripping regression: forbidden API hidden")
engine_code = code_only(engine)
# A previous CircleCI run failed at C compilation because F_INT was used
# by the native source but absent from the host harness. Detect *all* such
# missing native format symbols before the first compiler is invoked.
host_harness = code_only((root / "fm1" / "karma" / "test_engine.c").read_text(encoding="utf-8"))
native_format_order = (
    "F_INT", "F_PCT", "F_BIPCT", "F_TIME", "F_LFOHZ", "F_CUTOFF",
    "F_DB", "F_SEMI", "F_ENUM", "F_BPM", "F_NOTE", "F_ONOFF",
    "F_OCT", "F_STEPS", "F_SWING", "F_FILT"
)
host_format_match = re.search(r"enum\s*\{([^}]*\bF_INT\b[^}]*)\}", host_harness)
require(host_format_match is not None, "host C ABI missing native F_INT enum")
host_format_order = tuple(re.findall(r"\bF_[A-Z0-9_]+\b", host_format_match.group(1)))
require(host_format_order == native_format_order,
        f"host C ABI does not match pinned KARMA core.h format order: {host_format_order}")
native_format_usage = set(re.findall(r"\bF_[A-Z0-9_]+\b", engine_code))
require(native_format_usage <= set(host_format_order),
        f"host C ABI lacks engine format symbols: {sorted(native_format_usage - set(host_format_order))}")
host_defines = set(re.findall(r"^#define\s+(P_E[0-7]|ENGI_ACID)\b", host_harness, re.M))
native_id_usage = set(re.findall(r"\b(?:P_E[0-7]|ENGI_ACID)\b", engine_code))
require(native_id_usage <= host_defines,
        f"host C ABI lacks engine IDs: {sorted(native_id_usage - host_defines)}")
for forbidden in ('JuceHeader', 'malloc(', 'calloc(', 'realloc(', 'new ', 'float ', 'double '):
    require(forbidden not in engine_code, f"unsupported target dependency: {forbidden}")
require('#define MIYAKO_RING 4096u' in engine, "bounded capture ring missing")
require('mi_slot_max_bytes' in engine and 'mi_ring_power_of_two' in engine,
        "static target-memory safety guard missing")
require('miyako_tail_active(track_t *t)' in engine and 'tail_left' in engine and
        "&& !miyako_tail_active(t) &&" in patcher,
        "Eureka note-off tail is not connected to native mixer")
require('\\n' not in engine,
        "C source contains literal backslash-n characters instead of actual newlines")
require('"FM"' not in engine and '"WAVE", F_ENUM, 0, 3' in engine,
        "FM should use existing Plaits models, not a duplicate MIYAKO oscillator")
require('.desc = miyako_desc' in engine,
        "Mode-dependent knob mapping not connected to native engine")
for name in ('MI_PW_STORED', 'MI_WOW', 'MI_FLTR', 'MI_SIZE', 'MI_REPS_DESC'):
    require(name in engine, f"MIYAKO knob descriptor missing: {name}")
require('int32_t n = 1 + (rate >> 5);' in dot,
        "EUREKA graphic repeat count contradicts DSP")
require('mi_dot_graph' in dot and 'MI_DOT_Y RGB(255, 209, 12)' in dot,
        "yellow DOT UI missing")
require('static int boot_draw(void)' in boot and 'MIYAKO' in boot and
        'static void sloop_splash(void)' in splash and 'MIYAKO' in splash,
        "MIYAKO boot presentation missing")
require('((sine_i(p) * depth) >> 18)' in dot,
        "Reel trace must stay within the display canvas")
for test in ('test_engine.c', 'test_dot_ui.c', 'test_stress.c', 'test_boot_ui.c', 'test_panel_nav.c'):
    require((root / 'fm1' / 'karma' / test).is_file() and test in ci,
            f"missing sanitizer gate: {test}")
require('when: << pipeline.parameters.run_build >>' in ci, "manual parameter guard missing")
require('default: false' in ci, "CI default must be false")
require('only: feature/fm1-miyako-port-20261104' in ci, "branch guard missing")
require('KARMA_PIN: "db3cda79359ae914d1e58c0fc96c6ca744e7950e"' in ci,
        "upstream commit must remain pinned")
require('fm1/karma/apply_port.py' in ci and 'fm1/karma/verify_port.py' in ci,
        "native source integration/verification missing from CI")
require('MIYAKO-FM1-PREVIEW.fwsc' in ci, "artifact path missing")
print("PASS preflight: branch, features, exclusions, bounded buffers, pinned KARMA, manual CI gate")
