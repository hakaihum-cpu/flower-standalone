#!/usr/bin/env python3
"""Fail-closed post-patch verifier. Does not claim target build or sound test."""
from pathlib import Path
import sys
from native_guard import forbidden_dependencies, selftest as guard_selftest

def main():
    if len(sys.argv) != 2:
        raise RuntimeError("usage: verify_port.py PINNED_KARMA_CHECKOUT")
    root = Path(sys.argv[1]).resolve()
    src = root / "firmware" / "src"
    parts = {n: (src / n).read_text(encoding="utf-8") for n in
             ("engines.c", "params.c", "ui.c", "eng_miyako.c", "cloud.c", "cloud_dsp.c",
              "eng_elem.c", "eng_prism.c", "fx.c", "mi_cloud_shim.c", "ui_draw.c", "mi_dot_ui.c",
              "ui_vis.c", "mi_boot_anim.c", "splash.c", "ui_input.c", "mi_panel_nav.c", "mi_full_ui.c",
              "mi_eureka_video.c", "mi_eureka_vq_decoder.c", "mi_eureka_vq_frames.h")}
    guard_selftest()
    checks = {
        "MIYAKO engine table": ('&ENG_GONE, &ENG_GONE, &ENG_MIYAKO, &ENG_JUNO,' in parts["engines.c"]),
        "MIYAKO native source included": ('#include "eng_miyako.c"' in parts["engines.c"]),
        "Removed ACID DSP": ('#include "eng_acid.c"' not in parts["engines.c"] and
                             'acid_slot_t acid;' not in parts["engines.c"]),
        "MIYAKO arena allocated": ("miyako_slot_t miyako;" in parts["engines.c"] and
                                   "eng == ENGI_ACID ? sizeof(miyako_slot_t)" in parts["engines.c"]),
        "Eureka note-off tail retained": ('miyako_tail_active(track_t *t)' in parts["eng_miyako.c"] and
                                           'tail_left' in parts["eng_miyako.c"] and
                                           '&& !miyako_tail_active(t) && !slicer_busy(t) &&' in parts["fx.c"]),
        "Mutable DSP removed, legacy IDs reserved and muted": (
            '#include "eng_prism.c"' not in parts["engines.c"] and
            '#include "eng_elem.c"' not in parts["engines.c"] and
            '#include "eng_phys.c"' not in parts["engines.c"] and
            '#include "mi_cloud_shim.c"' in parts["fx.c"] and
            '#include "cloud.c"' not in parts["fx.c"] and
            '#include "cloud_dsp.c"' not in parts["mi_cloud_shim.c"] and
            'punch_ring[CLD_N]' in parts["mi_cloud_shim.c"] and
            'CL_ULAW[256]' in parts["mi_cloud_shim.c"] and
            'static uint8_t cl_mulaw(int32_t pcm)' in parts["mi_cloud_shim.c"] and
            'typedef union { uint32_t parked; } eng_arena1_t;' in parts["engines.c"] and
            'typedef union { uint32_t parked; } eng_arena2_t;' in parts["engines.c"] and
            '&ENG_GONE, &ENG_GONE, &ENG_MIYAKO, &ENG_JUNO' in parts["engines.c"] and
            'song.sel = 0u' in parts["mi_full_ui.c"] and
            'trk[i].p[P_MUTE] = 1' in parts["mi_full_ui.c"] and
            'song.g[G_CLSEND] = 0' in parts["mi_full_ui.c"] and
            'set_engine_of(&trk[0], ENGI_ACID)' in parts["mi_full_ui.c"]),
        "Native MIYAKO slot name": ('"PLAITS", "ELEMENTS", "MIYAKO", "JUNO"' in parts["params.c"]),
        "Native MIYAKO version": ('#define FELUCCA_VERSION "MIYAKO FM1 P2"' in parts["ui.c"]),
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
            'MI_REPS[] = {"1", "2", "3", "4", 0}' in parts["eng_miyako.c"]),
        "Four basic MIYAKO oscillators; no Plaits/FM": ('MY_WAVES[] = {"SINE", "SAW", "SQR", "PULSE"}' in parts["eng_miyako.c"] and
                                            '"WAVE", F_ENUM, 0, 3' in parts["eng_miyako.c"] and
                                            '.desc = miyako_desc' in parts["eng_miyako.c"] and
                                            '"FM"' not in parts["eng_miyako.c"]),
        "Exclusive MIYAKO full screen and no native page render": (
            '#include "mi_full_ui.c"' in parts["ui_input.c"] and
            'static void mi_full_draw(void);' in parts["ui_draw.c"] and
            'mi_full_draw(); /* MIYAKO exclusive' in parts["ui_draw.c"] and
            'mi_full_input(); /* physical encoder' in parts["ui_input.c"] and
            'ui_timers();' in parts["ui_draw.c"] and
            'cv_begin(240, 120, C_BLACK)' in parts["mi_full_ui.c"] and
            'cv_blit(0, pass * 120u)' in parts["mi_full_ui.c"]),
        "Exclusive physical key and note LEDs": (
            'mi_full_leds(); /* MIYAKO physical LED' in parts["ui_input.c"] and
            'static void mi_full_leds(void)' in parts["mi_full_ui.c"] and
            'led_put(nl, panel.btn[active], 1)' in parts["mi_full_ui.c"] and
            'keys_lit()' in parts["mi_full_ui.c"] and
            'mi_view % MI_VIEW_COUNT' in parts["mi_full_ui.c"]),
        "Complete physical MIYAKO page/knob contract": (
            all(x in parts["mi_full_ui.c"] for x in (
                'case B_HOME:', 'case B_EDIT:', 'case B_FX:', 'case B_SCL:',
                'case B_ENV:', 'case B_LFO:', 'case B_ARP:', 'case B_SEQ:',
                'case B_GLO:', 'case B_PLAY:', 'case B_REC:',
                'case B_SAVE:', 'case B_OCTDN:', 'case B_OCTUP:',
                'panel_enc(EN_SELECT)', 'panel_enc(EN_ALGO)',
                'panel_enc(EN_PRESET)', 'panel_enc(EN_K1 + k)',
                'mi_full_knob(k, s)', 'MI_VIEW_VIDEO'))),
        "No KARMA visual on splash": (
            '"KARMA / FELUCCA"' not in parts["splash.c"] and
            '"MIYAKO"' in parts["splash.c"]),
        "EUREKA video tick only once with two RGB565 display passes": (
            'mi_ev_update(&trk[0]);' in parts["mi_full_ui.c"] and
            'mi_ev_render();' in parts["mi_full_ui.c"] and
            'static void mi_ev_update(const track_t *t)' in parts["mi_eureka_video.c"] and
            'static void mi_ev_render(void)' in parts["mi_eureka_video.c"]),
        "576 frame 16-colour EUREKA VQ genuinely integrated": (
            '#include "mi_eureka_video.c"' in parts["mi_dot_ui.c"] and
            'mi_ev_draw(t);' in parts["mi_dot_ui.c"] and
            'MI_EUREKA_ASSET_COUNT 576u' in parts["mi_eureka_vq_frames.h"] and
            'MI_EUREKA_ASSET_COLORS 16u' in parts["mi_eureka_vq_frames.h"] and
            'MI_EUREKA_ASSET_BPP 4u' in parts["mi_eureka_vq_frames.h"] and
            'MI_EUREKA_DECODED_CRC32 0x' in parts["mi_eureka_vq_frames.h"] and
            'mi_eureka_vq_palette[16][16]' in parts["mi_eureka_vq_frames.h"] and
            'mi_eureka_vq_tilebook[16][256][16]' in parts["mi_eureka_vq_frames.h"] and
            'mi_eureka_vq_maps[576][320]' in parts["mi_eureka_vq_frames.h"] and
            '#include "mi_eureka_vq_decoder.c"' in parts["mi_eureka_video.c"] and
            'mi_eureka_vq_palette[mi_ev_visible / 36u]' in parts["mi_eureka_video.c"] and
            'mi_vq_seek(frame)' in parts["mi_eureka_video.c"] and
            'mi_eureka_vq_maps[frame]' in parts["mi_eureka_vq_decoder.c"] and
            'mi_ev_rand() % 5u' in parts["mi_eureka_video.c"] and
            'MI_EUREKA_ASSET_COUNT == 576u' in parts["mi_eureka_video.c"] and
            'int32_t n = 1 + (rate >> 5);' not in parts["mi_dot_ui.c"]),
        "MIYAKO standalone yellow UI integrated": (
            '#include "mi_full_ui.c"' in parts["ui_input.c"] and
            'cv_text(10, 4, &FONT_S, "MIYAKO", MI_Y)' in parts["mi_full_ui.c"] and
            '"KARMA"' not in parts["mi_full_ui.c"]),
        "MIYAKO-only boot screens": ('#include "mi_boot_anim.c"' in parts["ui_vis.c"] and
                                     'static int boot_draw(void)' in parts["mi_boot_anim.c"] and
                                     'static void sloop_splash(void)' in parts["splash.c"] and
                                     '"MIYAKO"' in parts["splash.c"]),
        "240x240 UI with 16-colour 128x78 video within screen": (
            'cv_begin(240, 120, C_BLACK)' in parts["mi_full_ui.c"] and
            'cv_oy = 49 - (int32_t)(pass * 120u);' in parts["mi_full_ui.c"] and
            'MI_EUREKA_ASSET_H == 78u' in parts["mi_eureka_video.c"]),
        "Excluded Tape texture disabled": ('"OFF", "VINYL", "-", "RAIN"' in parts["params.c"] and
                                            'if (src == TX_TAPE) src = TX_OFF;' in parts["fx.c"] and
                                            'case TX_TAPE:' not in parts["fx.c"]),
        "No excluded MIYAKO engines": (not any(v in parts["eng_miyako.c"] for v in
                                            ("Vongon", "Knowing", "Twilight", "EP-Sampler", "Rings", "Warps"))),
        "No JUCE / runtime allocation": (not forbidden_dependencies(parts["eng_miyako.c"])),
        "MIYAKO buffer bounded": ("#define MIYAKO_RING 4096u" in parts["eng_miyako.c"] and
                                  "edge < 32u" in parts["eng_miyako.c"] and
                                  "mi_slot_max_bytes" in parts["eng_miyako.c"] and
                                  "mi_ring_power_of_two" in parts["eng_miyako.c"]),
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
