#!/usr/bin/env python3
"""Install a MIYAKO-ONLY standalone user interface on the pinned FM-1 backend.

Fail closed on changed upstream anchors, never modify Android or main.
Upstream DSP sources provide only internal hardware/transport infrastructure.
Mutable audio sources are removed from the native compilation, while engine
numeric IDs remain reserved so old saved project formats are not renumbered.
"""
from pathlib import Path
import argparse
import ast
import shutil
import sys

HERE = Path(__file__).resolve().parent

def replacement(path: Path, old: str, new: str):
    src = path.read_text(encoding="utf-8")
    count = src.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: required anchor occurs {count} times (expected 1): {old[:75]!r}")
    path.write_text(src.replace(old, new, 1), encoding="utf-8")

def preflight_all_replacements(src: Path):
    """Verify *every* replacement against the pinned, unmodified upstream.
    Do this before writing ANY file. The previous ad-hoc preflight only tested
    a subset and could fail halfway through an installation overlay.
    Parse our own AST so even the exact 1.9 KiB boot-image replacement string
    is checked, without maintaining a second copy.
    """
    tree = ast.parse(Path(__file__).read_text(encoding="utf-8"))
    checks = []
    for node in ast.walk(tree):
        if not (isinstance(node, ast.Call) and
                isinstance(node.func, ast.Name) and node.func.id == "replacement"):
            continue
        if len(node.args) != 3:
            raise RuntimeError(f"replacement at line {node.lineno} lacks three arguments")
        target = node.args[0]
        if not (isinstance(target, ast.BinOp) and isinstance(target.op, ast.Div) and
                isinstance(target.left, ast.Name) and target.left.id == "src"):
            raise RuntimeError(f"replacement target at line {node.lineno} is not src / <file>")
        file_name = ast.literal_eval(target.right)
        old = ast.literal_eval(node.args[1])
        expected_new = ast.literal_eval(node.args[2])
        if not (isinstance(file_name, str) and file_name.endswith(".c") and
                isinstance(old, str) and old and isinstance(expected_new, str)):
            raise RuntimeError(f"replacement at line {node.lineno} contains invalid operands")
        checks.append((file_name, old, node.lineno))
    if len(checks) < 25:
        raise RuntimeError(f"expected at least 25 patch operations, saw {len(checks)}")
    errors = []
    for file_name, old, line in checks:
        source_file = src / file_name
        body = source_file.read_text(encoding="utf-8")
        hits = body.count(old)
        if hits != 1:
            errors.append(f"{file_name} line {line}: old text matches {hits} times")
    if errors:
        raise RuntimeError("KARMA patch preflight failed BEFORE any source edit:\\n" +
                           "\\n".join(errors))
    print(f"PASS: {len(checks)} exact upstream replacement anchors; no files changed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("karma_root", type=Path, help="Pinned KARMA source checkout")
    args = parser.parse_args()
    root = args.karma_root.resolve()
    src = root / "firmware" / "src"
    for item in ("engines.c", "params.c", "ui.c", "ui_draw.c", "ui_input.c", "ui_vis.c", "splash.c", "cloud.c", "eng_elem.c", "eng_prism.c"):
        if not (src / item).is_file():
            raise RuntimeError(f"Missing KARMA source {item}")
    engine = HERE / "eng_miyako.c"
    dest = src / "eng_miyako.c"
    if dest.exists() or (src / "mi_dot_ui.c").exists():
        raise RuntimeError("Native MIYAKO engine or UI already exists in upstream checkout")
    # Stage every changed source only after all target files have been checked.
    # Strict match on all legacy hooks prevents silently patching a different release.
    checks = {
        "engines.c": [
            '#include "eng_prism.c"',
            '#include "eng_acid.c"           /* ACID: X0X\'s TB-303 (Charles Vestal, GPL-3.0; Open303, MIT) */',
            'typedef union { acid_slot_t acid; juno_slot_t juno; } eng_arena0_t;',
            'eng == ENGI_ACID ? sizeof(acid_slot_t)',
            '&ENG_PRISM, &ENG_ELEM, &ENG_ACID, &ENG_JUNO,',
            'return e < PT_ENGINES && pi < PT_MAX ? PRESET_TRIM[e][pi] : 0;',
        ],
        "params.c": [
            '"PLAITS", "ELEMENTS", "ACID", "JUNO",',
            '"OFF", "VINYL", "TAPE", "RAIN",',
        ],
        "fx.c": [
            'tsvf_t fc;\n    if (song.playing && !tx.play)',
            '&& !slicer_busy(t) &&',
        ],
        "ui_draw.c": [
            'static void draw_graph(void)',
            '    } else {\n        switch (pg->graph) {',
            '    sig = graph_signature();',
            '    if (!ui.home && !drum_note && !is_drum(t) && !big_page(pg)',
            '    top = !ui.home && !drum_note && (pg->graph == GR_BROWSE',
            'TE_COL[song.sel & 3u], title, &ui.head_sig);',
        ],
        "ui_input.c": [
            'static uint32_t cur_btn(void)',
            '    return FAM_BTN[cur_fam()];',
            'static void layer_tap(uint32_t layer)',
            '    case LY_FX:\n        open_family(FAM_FX);\n        break;',
            '        open_family(layer == LY_STEP ? FAM_SEQ : FAM_EDIT);',
        ],
        "ui_vis.c": ['static int boot_draw(void)', 'static uint16_t boot_t = 0xFFFFu;'],
        "splash.c": ['static void sloop_splash(void)'],
        "ui.c": [
            '#define FELUCCA_VERSION "KARMA 3.0.3"',
            '/* ACID (track 1) */',
            '{BK_BASS, ENGI_ACID, "TB ACID"}, {BK_BASS, ENGI_ACID, "TB SQUARE"}, {BK_BASS, ENGI_ACID, "TB DIRTY"},',
            '{BK_BASS, ENGI_ACID, "TB DEEP"}, {BK_BASS, ENGI_ACID, "TB SQUELCH"}, {BK_BASS, ENGI_ACID, "TB RUBBER"},',
        ],
    }
    for filename, anchors in checks.items():
        body = (src / filename).read_text(encoding="utf-8")
        for needle in anchors:
            if body.count(needle) != 1:
                raise RuntimeError(f"{filename}: incompatible upstream; anchor count {body.count(needle)}: {needle!r}")
    preflight_all_replacements(src)
    # Only the approved 576-frame RGB565 16-colour VQ candidate is allowed.
    # A packer quality failure MUST abort CI before any source edits/build.
    packed = HERE / "mi_eureka_vq_frames.h"
    if not packed.is_file():
        raise RuntimeError("EUREKA 576-frame VQ data not generated")
    pack_text = packed.read_text(encoding="ascii")
    required = (
        "#define MI_EUREKA_ASSET_COUNT 576u",
        "#define MI_EUREKA_ASSET_BYTES 4992u",
        "#define MI_EUREKA_ASSET_COLORS 16u",
        "#define MI_EUREKA_ASSET_BPP 4u",
        "#define MI_EUREKA_VQ_TILE_COUNT 320u",
        "#define MI_EUREKA_DECODED_CRC32 0x",
        "mi_eureka_vq_palette[16][16]",
        "mi_eureka_vq_tilebook[16][256][16]",
        "mi_eureka_vq_maps[576][320]",
    )
    if any(token not in pack_text for token in required):
        raise RuntimeError("EUREKA VQ data corrupt / mono fallback / frame loss")
    shutil.copyfile(engine, dest)
    shutil.copyfile(HERE / "mi_dot_ui.c", src / "mi_dot_ui.c")
    shutil.copyfile(HERE / "mi_full_ui.c", src / "mi_full_ui.c")
    shutil.copyfile(HERE / "mi_cloud_shim.c", src / "mi_cloud_shim.c")
    shutil.copyfile(HERE / "mi_eureka_video.c", src / "mi_eureka_video.c")
    shutil.copyfile(HERE / "mi_eureka_vq_decoder.c", src / "mi_eureka_vq_decoder.c")
    shutil.copyfile(packed, src / "mi_eureka_vq_frames.h")
    shutil.copyfile(HERE / "mi_panel_nav.c", src / "mi_panel_nav.c")
    shutil.copyfile(HERE / "mi_boot_anim.c", src / "mi_boot_anim.c")
    shutil.copyfile(HERE / "mi_splash.c", src / "splash.c")
    # ACID is no longer an accessible engine. Remove its DSP compilation as
    # well, rather than retaining dead firmware code and stale arena types.
    replacement(src / "engines.c",
                '#include "eng_acid.c"           /* ACID: X0X\'s TB-303 (Charles Vestal, GPL-3.0; Open303, MIT) */',
                '/* ACID engine replaced by MIYAKO; unrelated ACID GEN remains available. */')
    # Plaits/Elements/PHYS are not mere hidden presets: exclude the
    # actual Mutable DSP source from compilation and reserve old IDs.
    replacement(src / "engines.c", '#include "eng_prism.c"',
                '#include "eng_miyako.c"')
    replacement(src / "engines.c",
                '#include "eng_phys.c"           /* PHYS\'s bank and models, for ELEMENTS (from Felucca 1.0: DaisySP, Rings: MIT) */',
                '/* Mutable PHYS oscillator removed; numeric engine ID preserved. */')
    replacement(src / "engines.c",
                '#include "eng_elem.c"           /* ELEMENTS: Elements\' exciters (Emilie Gillet, MIT) on PHYS\'s bank */',
                '/* Mutable ELEMENTS oscillator removed; numeric engine ID preserved. */')
    replacement(src / "engines.c",
                '#define PRISM_ARENA (sizeof(void *) == 8u ? 16704u : 16640u)   /* (a 64-bit host: the pointers in Plaits\' objects) */',
                '/* No PRISM DSP arena is required in MIYAKO-only firmware. */')
    replacement(src / "engines.c",
                'typedef union { uint32_t phys[PHYS_ARENA / 4u]; } eng_arena1_t;',
                'typedef union { uint32_t parked; } eng_arena1_t;')
    replacement(src / "engines.c",
                'typedef union { prism_slot_t prism[NVOICE]; uint64_t plx[(sizeof(void *) == 8u ? 16704u : 16640u) / 8u]; } eng_arena2_t;',
                'typedef union { uint32_t parked; } eng_arena2_t;')
    replacement(src / "engines.c",
                ': eng == ENGI_PRISM ? sizeof(prism_slot_t) : PHYS_ARENA;',
                ': 0u; /* dropped Mutable DSP cannot claim an arena */')
    replacement(src / "engines.c", 'typedef union { acid_slot_t acid; juno_slot_t juno; } eng_arena0_t;',
                'typedef union { juno_slot_t juno; miyako_slot_t miyako; } eng_arena0_t;')
    replacement(src / "engines.c", 'eng == ENGI_ACID ? sizeof(acid_slot_t)',
                'eng == ENGI_ACID ? sizeof(miyako_slot_t)')
    replacement(src / "engines.c", '&ENG_PRISM, &ENG_ELEM, &ENG_ACID, &ENG_JUNO,',
                '&ENG_GONE, &ENG_GONE, &ENG_MIYAKO, &ENG_JUNO,')
    # The reused engine ID must NOT inherit ACID's calibrated preset gains.
    replacement(src / "engines.c",
                'return e < PT_ENGINES && pi < PT_MAX ? PRESET_TRIM[e][pi] : 0;',
                'if (e == ENGI_ACID) return 0; /* ID 14 is MIYAKO, not ACID */\n'
                '    return e < PT_ENGINES && pi < PT_MAX ? PRESET_TRIM[e][pi] : 0;')
    replacement(src / "params.c", '"PLAITS", "ELEMENTS", "ACID", "JUNO",',
                '"PLAITS", "ELEMENTS", "MIYAKO", "JUNO",')
    # TAPE was explicitly excluded; leave its persisted enum id reserved
    # but make it unavailable and silent rather than renumber stored values.
    replacement(src / "params.c", '"OFF", "VINYL", "TAPE", "RAIN",',
                '"OFF", "VINYL", "-", "RAIN",')
    # Mutable Clouds is gone, but punch.c still needs the shared mu-law
    # recording ring defined at the original cloud.c include site.
    replacement(src / "fx.c",
                '#include "cloud.c"               /* the CLOUD send effect (Clouds\' grains), in mix_block below */',
                '#include "mi_cloud_shim.c" /* shared punch ring only; zero Clouds DSP */')
    replacement(src / "fx.c",
                'tsvf_t fc;\n    if (song.playing && !tx.play)',
                'tsvf_t fc;\n    if (src == TX_TAPE) src = TX_OFF; /* MIYAKO: deleted Tape texture */\n'
                '    if (song.playing && !tx.play)')
    # EUREKA's captured segment must remain audible after a note ends,
    # even when KARMA's legacy DIST/INSERT tail counter reaches zero.
    # Do NOT change native drum, Clouds, Elements or Plaits tail routing.
    replacement(src / "fx.c", '&& !slicer_busy(t) &&',
                '&& !miyako_tail_active(t) && !slicer_busy(t) &&')
    # Keep the historic TX_TAPE index so older projects load correctly, but
    # exclude the complete Tape generator branch from the C firmware build.
    replacement(src / "fx.c", "    case TX_TAPE:                                       /* t0: the level drifting (Q8), t2: its next target */\n        if (!(tx.t1++ & 31))\n            tx.t2 = (24 << 8) + (int32_t)((noise32(&tx.rnd) >> 20) & 0x7FFu) * 4;\n        tx.t0 += (tx.t2 - tx.t0) >> 6;\n        for (i = 0; i < n; i++)\n            for (k = 0; k < 2u; k++) {\n                int32_t w = tx_n16();\n                tx.a[k] += (w - tx.a[k]) >> 2;          /* tilted dark: mostly the low-passed part */\n                (k ? r : l)[i] = (((tx.a[k] >> 1) + (w >> 3)) * (tx.t0 >> 8)) >> 5;\n            }\n        break;\n", '')
    replacement(src / "ui.c", '#define FELUCCA_VERSION "KARMA 3.0.3"',
                '#define FELUCCA_VERSION "MIYAKO FM1 P2"')
    replacement(src / "ui.c", '/* ACID (track 1) */', '/* MIYAKO (track 1) */')
    replacement(src / "ui.c",
                '{BK_BASS, ENGI_ACID, "TB ACID"}, {BK_BASS, ENGI_ACID, "TB SQUARE"}, {BK_BASS, ENGI_ACID, "TB DIRTY"},',
                '{BK_KEYS, ENGI_ACID, "SINE"}, {BK_LEAD, ENGI_ACID, "SAW"}, {BK_KEYS, ENGI_ACID, "SQUARE"},')
    replacement(src / "ui.c",
                '{BK_BASS, ENGI_ACID, "TB DEEP"}, {BK_BASS, ENGI_ACID, "TB SQUELCH"}, {BK_BASS, ENGI_ACID, "TB RUBBER"},',
                '{BK_LEAD, ENGI_ACID, "PULSE"}, {BK_PAD, ENGI_ACID, "REEL"}, {BK_FX, ENGI_ACID, "EUREKA"},')
    # Retain the legacy DOT helper solely to share waveform + video drawing.
    # The full screen and ALL input are intercepted below: no KARMA GUI.
    # Full-screen UI is the only code path reached after the custom boot.
    replacement(src / "ui_draw.c", 'static void draw_graph(void)',
                '#include "mi_dot_ui.c"\nstatic void draw_graph(void)')
    replacement(src / "ui_draw.c", 'TE_COL[song.sel & 3u], title, &ui.head_sig);',
                'e == &ENG_MIYAKO ? RGB(255, 209, 12) : TE_COL[song.sel & 3u], title, &ui.head_sig);')
    replacement(src / "ui_draw.c", '    sig = graph_signature();',
                '    uint32_t mi_dot = !ui.home && !is_drum(t) && pg->scope == SC_ENGINE &&\n'
                '                      t->eng_req % NENGINES == ENGI_ACID;\n'
                '    sig = graph_signature();\n'
                '    if (mi_dot && t->peak > 32) sig ^= (ui.frame / 3u) * 40503u;')
    replacement(src / "ui_draw.c", '    } else {\n        switch (pg->graph) {',
                '    } else if (mi_dot) {\n'
                '        mi_dot_graph(t, pg->id[0] != P_E0);\n'
                '    } else {\n        switch (pg->graph) {')
    replacement(src / "ui_draw.c", '    if (!ui.home && !drum_note && !is_drum(t) && !big_page(pg)',
                '    if (!mi_dot && !ui.home && !drum_note && !is_drum(t) && !big_page(pg)')
    replacement(src / "ui_draw.c", '    top = !ui.home && !drum_note && (pg->graph == GR_BROWSE',
                '    top = mi_dot || (!ui.home && !drum_note && (pg->graph == GR_BROWSE')
    replacement(src / "ui_draw.c", '                                     big_page(pg));   /* these draw from the top */',
                '                                     big_page(pg)));   /* these draw from the top */')
    # Original page navigation remains linked for source compatibility but
    # becomes unreachable when MIYAKO full-screen input takes ownership.
    # Never expose native KARMA FX/CLOUD menus or track selectors.
    # The corresponding PHYSICAL FX button must light on the MIYAKO FX page;
    # this page belongs to FAM_EDIT internally, but is reached with FX.
    replacement(src / "ui_input.c", '    return FAM_BTN[cur_fam()];',
                '    if (!ui.home && cur_page()->scope == SC_ENGINE &&\n'
                '        cur_page()->id[0] == P_E4 && !is_drum(TSEL) &&\n'
                '        TSEL->eng_req % NENGINES == ENGI_ACID) return B_FX;\n'
                '    return FAM_BTN[cur_fam()];')
    replacement(src / "ui_input.c", 'static void layer_tap(uint32_t layer)',
                '#include "mi_panel_nav.c"\nstatic void layer_tap(uint32_t layer)')
    replacement(src / "ui_input.c",
                '    case LY_FX:\n        open_family(FAM_FX);\n        break;',
                '    case LY_FX:\n        mi_panel_fx_tap();\n        break;')
    replacement(src / "ui_input.c",
                '        open_family(layer == LY_STEP ? FAM_SEQ : FAM_EDIT);',
                '        if (layer == LY_ERASE) mi_panel_edit_tap();\n'
                '        else open_family(FAM_SEQ);')
    # Exclusive MIYAKO full-height graphics and hardware-only input.
    # ui_draw.c precedes ui_input.c in felucca.c. Define only a forward
    # declaration here; implement the MIYAKO view AFTER ui_input.c's own
    # static input helper definitions (layer state, button handlers etc.).
    replacement(src / "ui_draw.c", 'static void ui_draw(void)',
                'static void mi_full_draw(void);\nstatic void ui_draw(void)')
    replacement(src / "ui_draw.c",
                '    if (boot_draw())                                    /* (KARMA 3.0: the boot animation first) */\n'
                '        return;',
                '    if (boot_draw())\n'
                '        return;\n'
                '    mi_full_draw(); /* MIYAKO exclusive; do not reach native KARMA drawing */\n'
                '    ui_timers();\n'
                '    ui.force = 0u;\n'
                '    return;')
    # The hardware LEDs must also belong to MIYAKO, not KARMA's page
    # and function-layer state. The implementation appears later in
    # mi_full_ui.c, so install a forward declaration before native ui_leds.
    replacement(src / "ui_input.c", 'static void ui_leds(void)\n{',
                'static void mi_full_leds(void);\n'
                'static void ui_leds(void)\n{\n'
                '    mi_full_leds(); /* MIYAKO physical LED indications only */\n'
                '    return;')
    # Exact source-based hook, with mi_full_ui.c included at this point.
    replacement(src / "ui_input.c", 'static void ui_input(void)\n{',
                '#include "mi_full_ui.c"\nstatic void ui_input(void)\n{\n'
                '    mi_full_input(); /* physical encoder + buttons; no KARMA pages */\n'
                '    return;')
    # Replace the entire legacy KARMA boot animation with MIYAKO DOT code.
    # The fixed upstream text must match exactly, or we stop before firmware build.
    replacement(src / "ui_vis.c", "static int boot_draw(void)\n{\n    const uint32_t *ROW = KARMA_ROW;\n    const uint16_t *COL = KARMA_COL;\n    uint32_t t = boot_t, pass, x, y;\n    int32_t fade;\n    if (t >= BOOT_END)\n        return 0;\n    if (fm1_in.buttons || fm1_in.notes || ++boot_t >= BOOT_END) {\n        boot_t = 0xFFFFu;\n        lcd_fill(0, 0, 240, 240, C_BLACK);\n        ui.force = 1;\n        return 0;\n    }\n    fade = t > BOOT_END - 20u ? (int32_t)(t - (BOOT_END - 20u)) * 256 / 20 : 0;   /* (the last 20 frames: to black) */\n    for (pass = 0; pass < 2u; pass++) {\n        cv_begin(240, 120, C_BLACK);\n        cv_oy = -(int32_t)(pass * 120u);\n        for (y = 0; y < 7u; y++)\n            for (x = 0; x < 29u; x++) {\n                uint32_t d = ((x * 7u + y * 13u) * 2654435761u >> 26) % 44u, s;   /* (each cell its own moment) */\n                uint16_t c;\n                if (!((ROW[y] >> x) & 1u) || t < d)\n                    continue;\n                s = t - d >= 3u ? 6u : (t - d + 1u) * 2u;\n                c = t - d < 4u ? C_WHITE : COL[x / 6u];\n                c = mix565(c, C_BLACK, fade);\n                cv_rect(18 + (int32_t)(x * 7u) + (6 - (int32_t)s) / 2, 76 + (int32_t)(y * 7u) + (6 - (int32_t)s) / 2,\n                        (int32_t)s, (int32_t)s, c);\n            }\n        for (x = 0; x < 4u; x++) {                              /* the tracks' bars */\n            int32_t w = clamp(((int32_t)t - 50 - (int32_t)x * 4) * 5, 0, 44);\n            if (w)\n                cv_rect(26 + (int32_t)x * 48 + (44 - w) / 2, 150, w, 5, mix565(COL[x], C_BLACK, fade));\n        }\n        if (t >= 72u) {\n            const char *v = FELUCCA_VERSION;\n            if (v[0] == 'K' && v[5] == ' ')\n                v += 6;\n            cv_text(120 - text_w(&FONT_S, v) / 2, 172, &FONT_S, v, mix565(RGB(196, 196, 204), C_BLACK, fade));\n        }\n        cv_oy = 0;\n        cv_blit(0, pass * 120u);\n    }\n    return 1;\n}\n", '#include "mi_boot_anim.c"\n')
    # Upstream code can remain linked but must NEVER be user-visible.
    draw = (src / "ui_draw.c").read_text(encoding="utf-8")
    inp = (src / "ui_input.c").read_text(encoding="utf-8")
    only = (src / "mi_full_ui.c").read_text(encoding="utf-8")
    if not ('mi_full_draw(); /* MIYAKO exclusive' in draw and
            'mi_full_input(); /* physical encoder' in inp and
            'trk[i].p[P_MUTE] = 1' in only and
            'song.g[G_CLSEND] = 0' in only and
            'mi_full_leds(); /* MIYAKO physical LED' in inp and
            '#include "eng_elem.c"' not in (src / "engines.c").read_text(encoding="utf-8") and
            '#include "eng_prism.c"' not in (src / "engines.c").read_text(encoding="utf-8") and
            '&ENG_GONE, &ENG_GONE, &ENG_MIYAKO' in
               (src / "engines.c").read_text(encoding="utf-8") and
            '#include "mi_cloud_shim.c"' in (src / "fx.c").read_text(encoding="utf-8") and
            '#include "cloud.c"' not in (src / "fx.c").read_text(encoding="utf-8")):
        raise RuntimeError("MIYAKO-only screen / muted legacy engines absent")
    print("PASS: MIYAKO-only full LCD and physical controls; legacy page UI bypassed")

if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
