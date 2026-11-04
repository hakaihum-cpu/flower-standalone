#!/usr/bin/env python3
"""Fail-closed post-patch verifier. Does not claim target build or sound test."""
from pathlib import Path
import sys
from native_guard import forbidden_dependencies, selftest as guard_selftest

def main():
    guard_selftest()
    if len(sys.argv) != 2:
        raise RuntimeError("usage: verify_port.py PINNED_KARMA_CHECKOUT")
    root = Path(sys.argv[1]).resolve()
    src = root / "firmware" / "src"
    parts = {n: (src / n).read_text(encoding="utf-8") for n in
             ("engines.c", "params.c", "ui.c", "eng_miyako.c", "cloud.c", "cloud_dsp.c",
              "eng_elem.c", "eng_prism.c", "fx.c", "ui_draw.c", "mi_dot_ui.c",
              "ui_vis.c", "mi_boot_anim.c", "splash.c", "ui_input.c", "mi_panel_nav.c")}
    checks = {
        "MIYAKO engine table": ('&ENG_PRISM, &ENG_ELEM, &ENG_MIYAKO, &ENG_JUNO,' in parts["engines.c"]),
        "MIYAKO native source included": ('#include "eng_miyako.c"' in parts["engines.c"]),
        "Removed ACID DSP": ('#include "eng_acid.c"' not in parts["engines.c"] and
                             'acid_slot_t acid;' not in parts["engines.c"]),
        "MIYAKO arena allocated": ("miyako_slot_t miyako;" in parts["engines.c"] and
                                   "eng == ENGI_ACID ? sizeof(miyako_slot_t)" in parts["engines.c"]),
        "Eureka note-off tail retained": ('miyako_tail_active(track_t *t)' in parts["eng_miyako.c"] and
                                           'tail_left' in parts["eng_miyako.c"] and
                                           '&& !miyako_tail_active(t) && !slicer_busy(t) &&' in parts["fx.c"]),
        "Plaits FM and 6OP models genuinely available": ('"FM", "6OP A", "6OP B", "6OP C"' in parts["eng_prism.c"] and
                                                          'plx_render(' in parts["eng_prism.c"]),
        "Three Mutable sources retained": ('#include "eng_prism.c"' in parts["engines.c"] and
                                            '#include "eng_elem.c"' in parts["engines.c"] and
                                            '#include "cloud_dsp.c"' in parts["cloud.c"] and
                                            '#include "cloud.c"' in parts["fx.c"]),
        "Native MIYAKO slot name": ('"PLAITS", "ELEMENTS", "MIYAKO", "JUNO"' in parts["params.c"]),
        "Native MIYAKO version": ('#define FELUCCA_VERSION "MIYAKO FM1 P1"' in parts["ui.c"]),
        "No inherited ACID preset trim": ('if (e == ENGI_ACID) return 0;' in parts["engines.c"]),
        "Existing core engine count": ('#define NENGINES (16 + FELUCCA_SLICE)' in (src / "core.h").read_text()),
        "MIYAKO factory presets registered": (all('ENGI_ACID, "' + v + '"' in parts["ui.c"] for v in
                                                   ("SINE", "SAW", "SQUARE", "PULSE", "REEL", "EUREKA"))),
        "Exclusive FX mode": ('MY_FX[] = {"OFF", "REEL", "EUREKA"}' in parts["eng_miyako.c"]),
        "Knobs use DSP-matched dynamic labels": (
            '.desc = miyako_desc' in parts["eng_miyako.c"] and
            'MI_WOW = {"WOW"' in parts["eng_miyako.c"] and
            'MI_FLTR = {"FLTR"' in parts["eng_miyako.c"] and
            'MI_SIZE = {"SIZE"' in parts["eng_miyako.c"] and
            'MI_REPS_DESC = {"REPS"' in parts["eng_miyako.c"] and
            'MI_PW_STORED = {"PW*"' in parts["eng_miyako.c"] and
            'MI_REPS[] = {"1", "2", "3", "4", 0}' in parts["eng_miyako.c"] and
            'int32_t n = 1 + (rate >> 5);' in parts["mi_dot_ui.c"]),
        "Four basic oscillators; Plaits owns FM": ('MY_WAVES[] = {"SINE", "SAW", "SQR", "PULSE"}' in parts["eng_miyako.c"] and
                                            '"WAVE", F_ENUM, 0, 3' in parts["eng_miyako.c"] and
                                            '.desc = miyako_desc' in parts["eng_miyako.c"] and
                                            '"FM"' not in parts["eng_miyako.c"]),
        "Physical FX/EDIT navigation wired": ('#include "mi_panel_nav.c"' in parts["ui_input.c"] and
                                               'mi_panel_fx_tap();' in parts["ui_input.c"] and
                                               'mi_panel_edit_tap();' in parts["ui_input.c"] and
                                               'return B_FX;' in parts["ui_input.c"] and
                                               'mi_panel_sound_page(P_E4)' in parts["mi_panel_nav.c"]),
        "Touch-free screen: hardware knob/navigation intact": (
            'panel_enc(EN_SELECT)' in parts["ui_input.c"] and
            'panel_enc(EN_K1 + k)' in parts["ui_input.c"] and
            'panel_enc(EN_ALGO)' in parts["ui_input.c"] and
            'panel_enc(EN_PRESET)' in parts["ui_input.c"]),
        "Yellow DOT screen integrated": ('#include "mi_dot_ui.c"' in parts["ui_draw.c"] and
                                          'mi_dot_graph(t, pg->id[0] != P_E0);' in parts["ui_draw.c"] and
                                          'MI_DOT_Y RGB(255, 209, 12)' in parts["mi_dot_ui.c"]),
        "MIYAKO-only boot screens": ('#include "mi_boot_anim.c"' in parts["ui_vis.c"] and
                                     'static int boot_draw(void)' in parts["mi_boot_anim.c"] and
                                     'static void sloop_splash(void)' in parts["splash.c"] and
                                     '"MIYAKO"' in parts["splash.c"]),
        "UI draws inside bounded 240x124 canvas": ('cv_begin(240, H_GRAPH, C_BLACK);' in parts["ui_draw.c"] and
                                                   'cv_text(10, 104, &FONT_S, "WET"' in parts["mi_dot_ui.c"] and
                                                   'top = mi_dot ||' in parts["ui_draw.c"]),
        "Excluded Tape texture disabled": ('"OFF", "VINYL", "-", "RAIN"' in parts["params.c"] and
                                            'if (src == TX_TAPE) src = TX_OFF;' in parts["fx.c"] and
                                            'case TX_TAPE:' not in parts["fx.c"]),
        "No excluded MIYAKO engines": (not any(v in parts["eng_miyako.c"] for v in
                                            ("Vongon", "Knowing", "Twilight", "EP-Sampler", "Rings", "Warps"))),
        "No JUCE / runtime allocation": (not forbidden_dependencies(parts["eng_miyako.c"])),
        "MIYAKO buffer bounded": ("#define MIYAKO_RING 1024u" in parts["eng_miyako.c"] and
                                  "#define MIYAKO_EUREKA_DECIM 4u" in parts["eng_miyako.c"] and
                                  "mi_eureka_max_segment_fits" in parts["eng_miyako.c"] and
                                  "edge < 32u" in parts["eng_miyako.c"] and
                                  "mi_slot_max_bytes" in parts["eng_miyako.c"] and
                                  "mi_ring_power_of_two" in parts["eng_miyako.c"]),
        "Native reverb buffers rebalanced without losing either allpass": (
            'static int16_t rev_ap0[556] __attribute__((section(".pool")));' in parts["fx.c"] and
            'static int16_t rev_ap1[441];' in parts["fx.c"] and
            'c = k ? rev_ap1 : rev_ap0;' in parts["fx.c"] and
            'c += REV_AP[k];' not in parts["fx.c"] and
            'static int16_t rev_ap[556 + 441]' not in parts["fx.c"]),
        "Target linker guards flash/RAM/pool": (
            "XIP" in (root / "firmware" / "app.ld").read_text() and
            "POOL" in (root / "firmware" / "app.ld").read_text() and
            "pool headroom" in (root / "tools" / "build.py").read_text()),
    }
    for label, passed in checks.items():
        print(f"{'PASS' if passed else 'FAIL'} {label}")
    if not all(checks.values()):
        raise RuntimeError("Failed MIYAKO static port verification")
    print(f"PASS {len(checks)}/{len(checks)} static gates (target compiler/CPU/audio tests are separate)")

if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        sys.exit(1)
