/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO FM-1: exclusive, complete 240x240 UI.
 * The upstream source supplies LCD, input, sequencer and audio primitives.
 * NO KARMA page, bar, menu, layer, visualiser or other engine UI is drawn.
 * Two 240x120 RGB565 canvas passes fit the actual FM-1 display RAM.
 * All navigation is by physical keys/rotary encoders. No touch controls.
 *
 * Mapping:
 *   HOME=overview  EDIT=osc  FX=MIYAKO fx  SEL=Eureka video
 *   ENV=envelope  LFO=modulation  ARP=arpeggiator
 *   SEQ=sequence  GLO=settings  SAVE=project slot (3 taps to save)
 *   ALGORITHM=waveform  PRESETS=one of six MIYAKO presets
 *   SELECT=page  KNOB 1..4=visible parameters
 *   PLAY=transport  REC=record  OCT-/+=octave
 */
enum {
    MI_VIEW_HOME, MI_VIEW_OSC, MI_VIEW_FX, MI_VIEW_VIDEO,
    MI_VIEW_ENV, MI_VIEW_LFO, MI_VIEW_ARP, MI_VIEW_SEQ,
    MI_VIEW_GLOBAL, MI_VIEW_SAVE, MI_VIEW_COUNT
};
static uint8_t mi_view = MI_VIEW_HOME;
static uint8_t mi_full_dirty = 1u;
static uint8_t mi_full_ready = 0u;
static uint8_t mi_save_armed = 0u;
static uint32_t mi_save_t0 = 0u;
static uint32_t mi_draw_t0 = 0u;
#define MI_Y RGB(255, 209, 12)
#define MI_DIM RGB(130, 107, 35)
#define MI_DARK RGB(39, 37, 30)
#define MI_TEXT RGB(223, 220, 211)
static const char *const MI_VIEWS[MI_VIEW_COUNT] = {
    "HOME", "OSC", "FX", "VIDEO", "ENV", "LFO", "ARP",
    "SEQ", "CONFIG", "SAVE"
};
static const uint8_t MI_TRK_PARAM[MI_VIEW_COUNT][4] = {
    {P_E0, P_E4, P_E5, P_LEVEL},
    {P_E0, P_E1, P_E2, P_E3},
    {P_E4, P_E5, P_E6, P_E7},
    {P_E4, P_E5, P_E6, P_E7},
    {P_ATK, P_DEC, P_SUS, P_REL},
    {P_LRATE, P_LWAVE, P_LPHASE, P_LFADE},
    {P_AMODE, P_ARATE, P_AOCT, P_AGATE},
    {P_SLEN, P_SDIV, P_SSWING, P_SGATE},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
};
static const uint8_t MI_GLOB_PARAM[4] = {G_BPM, G_SWING, G_TUNE, G_CLOCK};

/* MIYAKO-owned LED state. Not even the function-key LEDs may refer to
 * KARMA's hidden page family or layer selection. Preserve note LEDs. */
static void mi_full_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, dl[FM1_NCOL] = {0}, bl[FM1_NCOL] = {0};
    uint32_t c, k, keys, guide, back, active;
    static uint8_t ready;
    static const uint8_t button[MI_VIEW_COUNT] = {
        B_HOME, B_EDIT, B_FX, B_SCL, B_ENV, B_LFO, B_ARP,
        B_SEQ, B_GLO, B_SAVE
    };
    if (!ready) {
        led_pos_init();
        ready = 1u;
    }
    active = button[mi_view % MI_VIEW_COUNT];
    led_put(nl, panel.btn[active], 1);
    led_put(nl, panel.btn[B_PLAY], song.playing != 0u);
    led_put(nl, panel.btn[B_REC], song.rec != 0u || rec_wait || ft_on);
    led_put(nl, panel.btn[B_OCTDN], song.octave < 0);
    led_put(nl, panel.btn[B_OCTUP], song.octave > 0);
    keys = keys_lit();
    guide = (keys_guide() | keys_notes_dim()) & ~keys;
    back = lights_keys_mask() & ~keys & ~guide;
    for (k = 0; k < 27u; ++k) {
        led_put(nl, 14u + k, (int)((keys >> k) & 1u));
        led_put(dl, 14u + k, (int)((guide >> k) & 1u));
        led_put(bl, 14u + k, (int)((back >> k) & 1u));
    }
    if (lights_lvl)
        for (k = 0u; k < NB; ++k)
            led_put(bl, panel.btn[k], 1);
    for (c = 0u; c < FM1_NCOL; ++c) {
        fm1_led[c] = nl[c];
        fm1_led_dim[c] = dl[c];
        fm1_led_bg[c] = (uint8_t)(bl[c] & ~nl[c]);
    }
    fm1_led_bg_ns = LIGHTS_NS[lights_lvl % LIGHTS_N];
}


static void mi_full_prepare(void)
{
    uint32_t i;
    if (mi_full_ready) return;
    mi_full_ready = 1u;
    /* Hard-lock performance to MIYAKO. No hidden part can sound a
     * Mutable engine, including after loading an older stored project.
     * Audio engine IDs stay in the upstream binary for safe boot/ABI;
     * they are NOT playable or exposed from MIYAKO's UI. */
    song.sel = 0u;
    song.seq_mode = 0u; /* never inherit a hidden KARMA step/grid mode */
    kb_grid = 0u;
    ui.home = 1u; /* never walk upstream KARMA page families */
    if (trk[0].eng_req % NENGINES != ENGI_ACID)
        set_engine_of(&trk[0], ENGI_ACID);
    for (i = 1u; i < NTRK; ++i)
        trk[i].p[P_MUTE] = 1;
    song.g[G_CLSEND] = 0;
    mi_ev_enabled = 0u;
    ui.menu = 0u;
    ui.layer = LY_PLAY;
    ui.force = 1u;
}
static void mi_full_set_page(uint32_t p)
{
    mi_view = (uint8_t)(p % MI_VIEW_COUNT);
    mi_save_armed = 0u;
    mi_ev_enabled = 0u;
    mi_full_dirty = 1u;
    ui.force = 1u;
}
static void mi_full_load_preset(int32_t delta)
{
    int32_t current = trk[0].preset;
    int32_t next = clamp(current + (delta > 0 ? 1 : -1), 0, 5);
    if (next != current) {
        apply_preset_to(&trk[0], (uint32_t)next);
        mi_ev_enabled = 0u;
        mi_full_dirty = 1u;
    }
}
static void mi_full_knob(uint32_t k, int32_t step)
{
    uint8_t id;
    int16_t *value;
    const param_desc_t *d;
    if (mi_view == MI_VIEW_SAVE) {
        if (k != 0u) return;
        id = G_SLOT;
        value = &song.g[id];
        d = &GP[id];
    } else if (mi_view == MI_VIEW_GLOBAL) {
        id = MI_GLOB_PARAM[k];
        value = &song.g[id];
        d = &GP[id];
    } else {
        id = MI_TRK_PARAM[mi_view][k];
        value = &trk[0].p[id];
        d = track_desc(&trk[0], id);
    }
    if (!d || !value) return;
    *value = (int16_t)clamp((int32_t)*value + step,
                            (int32_t)d->min, (int32_t)d->max);
    mi_full_dirty = 1u;
}
/* Called instead of the entire KARMA ui_input(). Important: the actual
 * keyboard notes, MIDI and DSP are handled by the separate FM-1 audio
 * input path, not by layer/page rendering. */
static void mi_full_input(void)
{
    uint32_t pressed = fm1_input_edges(0);
    uint32_t notes = fm1_input_note_edges();
    uint32_t id, b, k;
    if (pressed || notes)
        ui_input_ms = fm1_ms;
    int32_t s;
    mi_full_prepare();
    song.sel = 0u;
    song.seq_mode = 0u;
    kb_grid = 0u;
    ui.home = 1u;
    enc_hold = 0; /* every physical encoder is read fresh on each input tick */
    /* Never allow hidden Mutable voice streams through on older projects. */
    for (k = 1u; k < NTRK; ++k) trk[k].p[P_MUTE] = 1;
    song.g[G_CLSEND] = 0;
    ui.menu = 0u;
    ui.layer = LY_PLAY;
    if ((s = panel_enc(EN_ALGO)) != 0) {
        trk[0].p[P_E0] = (int16_t)clamp(trk[0].p[P_E0] + (s > 0 ? 1 : -1), 0, 3);
        mi_full_dirty = 1u;
    }
    if ((s = panel_enc(EN_PRESET)) != 0)
        mi_full_load_preset(s);
    if ((s = panel_enc(EN_SELECT)) != 0)
        mi_full_set_page((uint32_t)((mi_view + MI_VIEW_COUNT +
                                       (s > 0 ? 1 : -1)) % MI_VIEW_COUNT));
    for (k = 0u; k < 4u; ++k) {
        s = panel_enc(EN_K1 + k);
        if (s) mi_full_knob(k, s);
    }
    for (id = 0u; id < 14u; ++id) {
        if (!((pressed >> id) & 1u)) continue;
        b = panel_btn_of(id);
        switch (b) {
        case B_HOME: mi_full_set_page(MI_VIEW_HOME); break;
        case B_EDIT: mi_full_set_page(MI_VIEW_OSC); break;
        case B_FX:   mi_full_set_page(MI_VIEW_FX); break;
        case B_SCL:
            /* VIDEO must show the actual EUREKA animation immediately,
             * not an unrelated FX OFF or REEL page. Preserve user size/reps;
             * initialize wet only if still fully dry. */
            mi_full_set_page(MI_VIEW_VIDEO);
            trk[0].p[P_E4] = 2; /* EUREKA */
            if (trk[0].p[P_E5] == 0)
                trk[0].p[P_E5] = 95;
            break;
        case B_ENV:  mi_full_set_page(MI_VIEW_ENV); break;
        case B_LFO:  mi_full_set_page(MI_VIEW_LFO); break;
        case B_ARP:  mi_full_set_page(MI_VIEW_ARP); break;
        case B_SEQ:  mi_full_set_page(MI_VIEW_SEQ); break;
        case B_GLO:  mi_full_set_page(MI_VIEW_GLOBAL); break;
        case B_PLAY:
            transport_req = song.playing ? 2 : 1;
            mi_full_dirty = 1u;
            break;
        case B_REC:
            if (!ft_owns_press()) rec_toggle();
            mi_full_dirty = 1u;
            break;
        case B_OCTDN: song.octave = clamp(song.octave - 1, -3, 3);
            mi_full_dirty = 1u; break;
        case B_OCTUP: song.octave = clamp(song.octave + 1, -3, 3);
            mi_full_dirty = 1u; break;
        case B_SAVE:
            if (mi_view != MI_VIEW_SAVE) {
                mi_full_set_page(MI_VIEW_SAVE);
            } else if (!mi_save_armed || fm1_ms - mi_save_t0 > 4000u) {
                mi_save_armed = 1u;
                mi_save_t0 = fm1_ms;
            } else {
                project_save((uint32_t)song.g[G_SLOT]);
                mi_save_armed = 0u;
            }
            mi_full_dirty = 1u;
            break;
        default: break;
        }
    }
}
static void mi_full_graph(void)
{
    uint32_t x;
    int32_t prev_y = 108;
    if (mi_view == MI_VIEW_OSC || mi_view == MI_VIEW_HOME) {
        uint32_t wave = (uint32_t)clamp(trk[0].p[P_E0], 0, 3);
        uint32_t width = (32u + (((uint32_t)clamp(trk[0].p[P_E1], 0, 127) * 192u) >> 7)) << 24;
        const char *const kinds[] = {"SINE", "SAW", "SQUARE", "PULSE"};
        for (x = 8u; x < 234u; x += 3u) {
            uint32_t ph = (x - 8u) * 67108864u;
            int32_t val = mi_dot_wave((int32_t)wave, ph, width);
            int32_t y = 110 - (val * 40 >> 15);
            if (x > 8u)
                cv_line((int32_t)x - 3, prev_y, (int32_t)x, y, MI_Y);
            prev_y = y;
        }
        cv_text(13, 153, &FONT_S, kinds[wave], MI_TEXT);
    } else if (mi_view == MI_VIEW_FX || mi_view == MI_VIEW_VIDEO) {
        uint32_t mode = (uint32_t)clamp(trk[0].p[P_E4], 0, 2);
        if (mode == 0u) {
            cv_text(16, 102, &FONT_L, "FX OFF", MI_DIM);
        } else if (mode == 1u) {
            for (x = 12u; x < 230u; x += 4u) {
                uint32_t ph = x * 33554432u + ui.frame * 2147483u;
                int32_t y = 110 - (sine_i(ph) * 35 >> 15);
                if (x > 12u)
                    cv_line((int32_t)x - 4, prev_y, (int32_t)x, y, MI_Y);
                prev_y = y;
            }
            cv_text(13, 153, &FONT_S, "REEL", MI_TEXT);
        } else {
            /* MIYAKO EUREKA is drawn by mi_ev_render() between passes. */
        }
    } else {
        for (x = 12u; x < 230u; x += 8u)
            cv_rect((int32_t)x, 115, 3, 3, MI_DIM);
        cv_text(12, 86, &FONT_L, MI_VIEWS[mi_view], MI_Y);
    }
}
static void mi_full_param_band(void)
{
    uint32_t k, n = mi_view == MI_VIEW_SAVE ? 1u : 4u;
    for (k = 0u; k < n; ++k) {
        const param_desc_t *d;
        int32_t value;
        char txt[14];
        int32_t xx = (int32_t)k * 60 + 3;
        if (mi_view == MI_VIEW_SAVE) {
            value = song.g[G_SLOT]; d = &GP[G_SLOT];
        } else if (mi_view == MI_VIEW_GLOBAL) {
            uint32_t id = MI_GLOB_PARAM[k];
            value = song.g[id]; d = &GP[id];
        } else {
            uint32_t id = MI_TRK_PARAM[mi_view][k];
            value = trk[0].p[id]; d = track_desc(&trk[0], id);
        }
        if (!d) continue;
        cv_text(xx, 174, &FONT_S, d->label, MI_DIM);
        fmt_int(txt, value);
        cv_text(xx, 194, &FONT_S, txt, MI_TEXT);
        cv_rect(xx, 215, 52, 2, MI_DARK);
        if (d->max > d->min)
            cv_rect(xx, 215,
                    clamp((value - d->min) * 52 / (d->max - d->min), 0, 52),
                    2, MI_Y);
    }
}
/* Full-height screen; no call to KARMA's draw_head/columns/graph/menu/
 * footer, no implicit firmware UI mixing. */
static void mi_full_draw(void)
{
    uint32_t pass, now = fm1_ms;
    int photo = (mi_view == MI_VIEW_FX || mi_view == MI_VIEW_VIDEO) &&
                trk[0].p[P_E4] == 2;
    if (!mi_full_ready) mi_full_prepare();
    if (photo) {
        mi_ev_update(&trk[0]);  /* consume peak exactly ONCE per display tick */
    } else {
        mi_ev_enabled = 0u;
    }
    if (!mi_full_dirty && now - mi_draw_t0 < (photo ? 55u : 130u))
        return;
    mi_draw_t0 = now;
    for (pass = 0u; pass < 2u; ++pass) {
        cv_begin(240, 120, C_BLACK);
        cv_oy = -(int32_t)(pass * 120u);
        cv_rect(0, 0, 240, 26, MI_DARK);
        cv_text(10, 4, &FONT_S, "MIYAKO", MI_Y);
        cv_text(142, 4, &FONT_S, MI_VIEWS[mi_view], MI_TEXT);
        cv_rect(0, 28, 240, 1, MI_DIM);
        if (photo) {
            /* 16-colour original-derived EUREKA animation: a dedicated
             * 240x124 area at y=49..173, no KARMA widgets beneath it. */
            cv_oy = 49 - (int32_t)(pass * 120u);
            mi_ev_render();
            cv_oy = -(int32_t)(pass * 120u);
        } else {
            mi_full_graph();
        }
        cv_rect(0, 170, 240, 1, MI_DIM);
        mi_full_param_band();
        cv_rect(0, 219, 240, 1, MI_DIM);
        cv_text(7, 221, &FONT_S, song.playing ? "PLAY" : "STOP", MI_Y);
        if (song.rec || rec_wait)
            cv_text(70, 221, &FONT_S, "REC", RGB(255, 65, 65));
        if (mi_save_armed && mi_view == MI_VIEW_SAVE)
            cv_text(132, 221, &FONT_S, "SAVE AGAIN", MI_Y);
        cv_oy = 0;
        cv_blit(0, pass * 120u);
    }
    mi_full_dirty = 0u;
}
