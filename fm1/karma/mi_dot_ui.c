/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO's 240x124 graph strip on the FM-1's real 240x240 LCD.
 * Deliberately uses only KARMA's existing bounded canvas/bitmap font APIs.
 * The knob band, header, footer and all controls remain KARMA's working UI.
 * Yellow 1-bit-style pixels on black; there are no external image assets.
 */
#define MI_DOT_Y RGB(255, 209, 12)
#define MI_DOT_B RGB(150, 121, 12)
#define MI_DOT_D RGB(43, 39, 20)
#define MI_DOT_W C_WHITE

static int32_t mi_dot_wave(int32_t wave, uint32_t ph, uint32_t width)
{
    switch (wave) {
    case 0: return sine_i(ph);
    case 1: return (int32_t)(ph >> 16) - 32768;
    case 2: return ph < 0x80000000u ? 26000 : -26000;
    default: return ph < width ? 26000 : -26000;
    }
}
static void mi_dot_lattice(void)
{
    int32_t x, y;
    /* dotted lattice: no anti-aliasing, no full-screen framebuffer */
    for (y = 32; y <= 96; y += 8)
        for (x = 9; x <= 230; x += 8)
            cv_pset(x, y, MI_DOT_D);
    cv_rect(3, 2, 234, 1, MI_DOT_B);
    cv_rect(3, 120, 234, 1, MI_DOT_B);
    cv_rect(3, 2, 1, 119, MI_DOT_B);
    cv_rect(236, 2, 1, 119, MI_DOT_B);
    cv_rect(9, 25, 222, 1, MI_DOT_D);
    cv_rect(9, 97, 222, 1, MI_DOT_D);
}
static void mi_dot_osc(const track_t *t)
{
    uint32_t wave = (uint32_t)clamp(t->p[P_E0], 0, 3);
    uint32_t pw = (32u + ((uint32_t)clamp(t->p[P_E1], 0, 127) * 192u >> 7)) << 24;
    uint32_t shift = t->peak > 32 ? ui.frame * 25165824u : 0u;
    int32_t x, py = 64;
    const char *const wave_names[] = {"SINE", "SAW", "SQUARE", "PULSE"};
    for (x = 10; x <= 230; x += 2) {
        uint32_t ph = (((uint32_t)(x - 10) * 5u) << 23) + shift;
        int32_t y = 64 - (mi_dot_wave((int32_t)wave, ph, pw) * 27 >> 15);
        if (x > 10) cv_line(x - 2, py, x, y, MI_DOT_Y);
        py = y;
    }
    cv_text(10, 100, &FONT_S, wave_names[wave], MI_DOT_Y);
    cv_text(188, 100, &FONT_S, "OSC", MI_DOT_B);
}
static void mi_dot_fx(const track_t *t)
{
    int32_t mode = clamp(t->p[P_E4], 0, 2);
    int32_t wet = clamp(t->p[P_E5], 0, 127);
    int32_t depth = clamp(t->p[P_E6], 0, 127);
    int32_t rate = clamp(t->p[P_E7], 0, 127);
    int32_t x, h;
    static const char *const modes[] = {"BYPASS", "REEL", "EUREKA"};
    cv_text(10, 30, &FONT_S, modes[mode], mode ? MI_DOT_Y : MI_DOT_B);
    if (mode == 1) {
        int32_t px = 10, py = 78;
        /* transport wobble: activated by on-device envelope / note activity */
        uint32_t motion = t->peak > 32 ? ui.frame * 33554432u : 0u;
        for (x = 10; x <= 230; x += 3) {
            uint32_t p = (((uint32_t)x * 5u) << 23) + motion;
            int32_t y = 77 + ((sine_i(p) * depth) >> 18);
            if (x > 10) cv_line(px, py, x, y, MI_DOT_Y);
            px = x; py = y;
        }
    } else if (mode == 2) {
        /* Match actual DSP: P_E7 >> 5 selects 1..4 repeats. */
        int32_t n = 1 + (rate >> 5);
        for (x = 0; x < n; x++) {
            int32_t ww = (212 / n) - 4;
            int32_t h2 = 10 + depth / 8 + (x & 1) * 10;
            cv_rect(14 + x * (212 / n), 85 - h2, ww, h2, x & 1 ? MI_DOT_B : MI_DOT_Y);
        }
    } else {
        for (x = 0; x < 26; x++)
            cv_rect(10 + x * 8, 76, 3, 3, MI_DOT_D);
    }
    h = wet * 208 / 127;
    cv_rect(12, 99, 212, 3, MI_DOT_D);
    if (h) cv_rect(12, 99, h, 3, MI_DOT_Y);
    cv_text(10, 104, &FONT_S, "WET", MI_DOT_B);
    cv_text(180, 104, &FONT_S, "FX", MI_DOT_B);
}
static void mi_dot_graph(const track_t *t, int fx_page)
{
    cv_oy = 0;
    mi_dot_lattice();
    cv_text(10, 5, &FONT_S, "MIYAKO", MI_DOT_Y);
    cv_text(182, 5, &FONT_S, fx_page ? "FX" : "OSC", MI_DOT_W);
    if (fx_page) mi_dot_fx(t);
    else mi_dot_osc(t);
}
