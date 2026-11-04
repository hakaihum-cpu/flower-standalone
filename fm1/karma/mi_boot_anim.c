/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO firmware boot animation; replaces inherited KARMA glyphs.
 * Included after boot_t/BOOT_END in ui_vis.c, so their behavior is preserved.
 */
static int boot_draw(void)
{
    uint32_t t = boot_t, pass, i;
    int32_t fade, fill;
    if (t >= BOOT_END)
        return 0;
    if (fm1_in.buttons || fm1_in.notes || ++boot_t >= BOOT_END) {
        boot_t = 0xFFFFu;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
        return 0;
    }
    fade = t > BOOT_END - 20u ? (int32_t)(t - (BOOT_END - 20u)) * 256 / 20 : 0;
    fill = clamp(((int32_t)t - 9) * 4, 0, 210);
    for (pass = 0; pass < 2u; ++pass) {
        cv_begin(240, 120, C_BLACK);
        cv_oy = -(int32_t)(pass * 120u);
        for (i = 0; i < 27u; ++i) {
            uint32_t reveal = (i * 7u + 11u) % 37u;
            if (t < reveal)
                continue;
            cv_rect(12 + (int32_t)i * 8, 65, 4, 4,
                    mix565(RGB(255, 209, 12), C_BLACK, fade));
            cv_rect(12 + (int32_t)i * 8, 151, 4, 4,
                    mix565(RGB(95, 76, 10), C_BLACK, fade));
        }
        if (t > 10u) {
            cv_text(120 - text_w(&FONT_L, "MIYAKO") / 2, 94, &FONT_L,
                    "MIYAKO", mix565(RGB(255, 209, 12), C_BLACK, fade));
        }
        if (fill > 0)
            cv_rect(15, 161, fill, 3,
                    mix565(RGB(255, 209, 12), C_BLACK, fade));
        if (t > 45u)
            cv_text(120 - text_w(&FONT_S, "MIYAKO FM1") / 2, 176,
                    &FONT_S, "MIYAKO FM1", mix565(C_WHITE, C_BLACK, fade));
        cv_oy = 0;
        cv_blit(0, pass * 120u);
    }
    return 1;
}
