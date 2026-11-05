#!/usr/bin/env python3
"""Static preflight before the explicit CircleCI FM-1 build. No target build here."""
from pathlib import Path

root = Path(__file__).resolve().parent.parent
engine = (root / "fm1" / "karma" / "eng_miyako.c").read_text(encoding="utf-8")
patcher = (root / "fm1" / "karma" / "apply_port.py").read_text(encoding="utf-8")
verifier = (root / "fm1" / "karma" / "verify_port.py").read_text(encoding="utf-8")
dot = (root / "fm1" / "karma" / "mi_dot_ui.c").read_text(encoding="utf-8")
full = (root / "fm1" / "karma" / "mi_full_ui.c").read_text(encoding="utf-8")
cloud_shim = (root / "fm1" / "karma" / "mi_cloud_shim.c").read_text(encoding="utf-8")
video = (root / "fm1" / "karma" / "mi_eureka_video.c").read_text(encoding="utf-8")
packer = (root / "fm1" / "karma" / "pack_eureka_frames.py").read_text(encoding="utf-8")
eureka_materializer = (root / "scripts" / "materialize_miyako_eureka_video.py").read_text(encoding="utf-8")
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
for token in ('eng_miyako.c', 'mi_dot_ui.c', 'mi_eureka_video.c', 'mi_eureka_vq_frames.h', 'mi_eureka_vq_decoder.c',
              'mi_boot_anim.c', 'mi_splash.c',
              'ENG_PRISM', 'ENG_ELEM', 'cloud.c',
              'sizeof(miyako_slot_t)', '&ENG_MIYAKO', 'if (e == ENGI_ACID) return 0;'):
    require(token in patcher + verifier, f"native KARMA port missing: {token}")
# The user explicitly dropped Plaits/Elements/Clouds: none is allowed in the
# MIYAKO UI or sound-picker, and all non-MIYAKO parts must stay muted.
for token in ('ENG_PRISM', 'ENG_ELEM', 'CLOUD', '"KARMA"'):
    require(token not in full, f"non-MIYAKO UI reference: {token}")
for token in ('Vongon', 'EP-Sampler', 'Knowing', 'Tape', 'Twilight', 'Rings', 'Warps'):
    require(token in manifest, f"requested exclusion omitted: {token}")
from karma.native_guard import forbidden_dependencies, selftest as guard_selftest
# Scan executable C only: English comments mentioning "new" are not allocations.
guard_selftest()
violations = forbidden_dependencies(engine)
require(not violations, f"unsupported target dependency: {', '.join(violations)}")
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
require('#include "mi_eureka_video.c"' in dot and 'mi_ev_draw(t);' in dot,
        "EUREKA original video not wired into FX display")
require('mi_eureka_vq_decoder.c' in video and 'mi_ev_seek' in video and
        'MI_EUREKA_ASSET_COUNT == 576u' in video and
        'mi_ev_rand() % 5u' in video and
        'mi_ev_choose_clip' in video and 'mi_ev_prior' in video,
        "EUREKA 576-frame C decoder missing or playback not audio gated")
vq = (root / "fm1" / "karma" / "pack_eureka_vq_candidate.py").read_text(encoding="utf-8")
vq_decoder = (root / "fm1" / "karma" / "mi_eureka_vq_decoder.c").read_text(encoding="utf-8")
require('COUNT = 16 * 36' in packer and
        'COLORS = 16' in packer and
        'EUREKA_COMPLETE_COUNT' in eureka_materializer and
        'MI_EUREKA_DECODED_CRC32' in vq and
        'reconstructed != 576' in vq and
        'MEDIA_BYTES == 250368' in vq and
        '--min-psnr-db' in vq and
        'mi_eureka_vq_maps[frame]' in vq_decoder and
        'mi_eureka_vq_palette[mi_ev_visible / 36u]' in video and
        'mi_ev_seek(frame)' not in video and
        'mi_vq_seek(frame)' in video and
        'MI_EUREKA_ASSET_COUNT == 576u' in video and
        'mi_eureka_vq_frames.h' in patcher and
        'mi_eureka_vq_frames.h' in verifier,
        "16-colour 576-frame VQ renderer/quality checks not fully integrated")
require((root / "fm1" / "karma" / "test_eureka_vq_decoder.c").is_file(),
        "VQ native regression missing")
require('pack_eureka_vq_candidate.py' in ci and
        'test_eureka_vq_decoder.c' in ci and
        '--min-psnr-db' in ci,
        "VQ candidate not used by CI quality/decoder steps")
require('scripts/materialize_miyako_eureka_video.py' in ci and
        'pack_eureka_vq_candidate.py' in ci,
        "CI must restore all 576 original frames before host/target checks")
require('mi_dot_graph' in dot and 'MI_DOT_Y RGB(255, 209, 12)' in dot,
        "yellow DOT drawing helper missing")
require('mi_cloud_shim.c' in patcher and
        '#include "mi_cloud_shim.c"' in patcher and
        'CL_ULAW[256]' in cloud_shim and
        'punch_ring[CLD_N]' in cloud_shim and
        'cl_mulaw(int32_t pcm)' in cloud_shim and
        '#include "cloud_dsp.c"' not in cloud_shim,
        "Clouds DSP removal would break shared punch recording buffer")
# Inspect the actual replacement() calls, not fragile fragments of their
# Python string representation (which may include upstream source comments).
import ast

def native_replacements(filename):
    operations = []
    for node in ast.walk(ast.parse(patcher)):
        if not (isinstance(node, ast.Call) and
                isinstance(node.func, ast.Name) and
                node.func.id == "replacement" and len(node.args) == 3):
            continue
        target = node.args[0]
        if not (isinstance(target, ast.BinOp) and
                isinstance(target.op, ast.Div) and
                isinstance(target.left, ast.Name) and
                target.left.id == "src"):
            continue
        try:
            if ast.literal_eval(target.right) != filename:
                continue
            operations.append((ast.literal_eval(node.args[1]),
                               ast.literal_eval(node.args[2])))
        except (ValueError, TypeError, SyntaxError) as exc:
            raise SystemExit(f"FAIL malformed native replacement: {exc}")
    return operations

engine_ops = native_replacements("engines.c")
for removed in ("eng_prism.c", "eng_elem.c", "eng_phys.c"):
    active = '#include "' + removed + '"'
    require(any(active in old and active not in new_source
                for old, new_source in engine_ops),
            f"Mutable DSP removal missing from actual patch operations: {removed}")
require(any('&ENG_PRISM, &ENG_ELEM, &ENG_ACID, &ENG_JUNO,' in old and
            '&ENG_GONE, &ENG_GONE, &ENG_MIYAKO, &ENG_JUNO,' in new_source
            for old, new_source in engine_ops),
        "Mutable engine table IDs not reserved/replaced by MIYAKO")
require('plx_anchor = "    plx = build_plx()"' in patcher and
        'no Plaits C++ model objects' in patcher and
        'Plaits C++ linker inputs excluded' in verifier,
        "Plaits C++ models still linked into MIYAKO-only app slot")
require('mi_full_ui.c' in patcher and
        'mi_full_draw(); /* MIYAKO exclusive' in patcher and
        'mi_full_input(); /* physical encoder' in patcher and
        'song.sel = 0u' in full and
        'trk[i].p[P_MUTE] = 1' in full and
        'song.g[G_CLSEND] = 0' in full,
        "exclusive full-screen MIYAKO display/input or Mutable mute missing")
require('cv_begin(240, 120, C_BLACK)' in full and
        'cv_blit(0, pass * 120u)' in full and
        'mi_ev_update(&trk[0]);' in full and
        'mi_ev_render();' in full and
        'static void mi_ev_update(const track_t *t)' in video and
        'static void mi_ev_render(void)' in video and
        '"KARMA / FELUCCA"' not in splash,
        "full-height RGB565 MIYAKO + 16-colour EUREKA display contract missing")
buttons = ('B_HOME','B_EDIT','B_FX','B_SCL','B_ENV','B_LFO','B_ARP',
           'B_SEQ','B_GLO','B_PLAY','B_REC','B_SAVE','B_OCTDN','B_OCTUP')
require(all(f'case {b}:' in full for b in buttons) and
        all(f'panel_enc({e})' in full for e in ('EN_ALGO','EN_PRESET','EN_SELECT')) and
        'panel_enc(EN_K1 + k)' in full,
        "not all FM-1 hardware buttons and encoders have MIYAKO bindings")
require('static int boot_draw(void)' in boot and 'MIYAKO' in boot and
        'static void sloop_splash(void)' in splash and 'MIYAKO' in splash,
        "MIYAKO boot presentation missing")
require('((sine_i(p) * depth) >> 18)' in dot,
        "Reel trace must stay within the display canvas")
for test in ('test_engine.c', 'test_dot_ui.c', 'test_stress.c', 'test_boot_ui.c', 'test_panel_nav.c', 'test_eureka_video.c', 'test_eureka_vq_decoder.c'):
    require((root / 'fm1' / 'karma' / test).is_file() and test in ci,
            f"missing sanitizer gate: {test}")
require(ci.count('\nworkflows:') == 1 and ci.count('      - run:') >= 8, 'CI YAML duplicated or truncated')
require('\n fm1/karma/mi_eureka_frames.h' not in ci, 'CI contains malformed duplicated tail')
require((root / "fm1" / "karma" / "prepare_fwsc_version.py").is_file() and
        (root / "fm1" / "karma" / "verify_fwsc_version.py").is_file() and
        "Set fresh M-UPGRADE numeric identity" in ci and
        "--build-number" in ci and
        "verify_fwsc_version.py" in ci and
        "FWSC_VERSION.txt" in ci,
        "FWSC CurrentVersion increment/identity verification not wired into CI")
require('when: << pipeline.parameters.run_build >>' in ci, "manual parameter guard missing")
require('default: false' in ci, "CI default must be false")
require('only: feature/fm1-miyako-port-20261104' in ci, "branch guard missing")
require('KARMA_PIN: "db3cda79359ae914d1e58c0fc96c6ca744e7950e"' in ci,
        "upstream commit must remain pinned")
require('fm1/karma/apply_port.py' in ci and 'fm1/karma/verify_port.py' in ci,
        "native source integration/verification missing from CI")
require('MIYAKO-FM1-PREVIEW.fwsc' in ci and 'eureka-vq-preview.png' in ci,
        "FWSC or actual VQ source preview artifact path missing")
print("PASS preflight: branch, features, exclusions, bounded buffers, pinned KARMA, manual CI gate")
