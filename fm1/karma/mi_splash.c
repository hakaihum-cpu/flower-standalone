/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO FM-1 boot splash. No generated artwork or bitmap resources.
 * All drawing uses the existing FM-1 bounded RGB565 canvas.
 */
static void sloop_splash(void)
{
    static const uint16_t yellow = RGB(255, 209, 12);
    uint32_t i;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    cv_begin(240, 112, C_BLACK);
    for (i = 0; i < 25u; ++i) {
        uint16_t c = (i & 1u) ? RGB(43, 39, 20) : RGB(95, 76, 10);
        cv_rect(15 + (int32_t)i * 8, 15, 3, 3, c);
        cv_rect(15 + (int32_t)i * 8, 103, 3, 3, c);
    }
    cv_text(120 - text_w(&FONT_L, "MIYAKO") / 2, 31, &FONT_L, "MIYAKO", yellow);
    cv_text(120 - text_w(&FONT_S, "FM-1") / 2, 73, &FONT_S, "FM-1", C_WHITE);
    cv_blit(0, 62);
    cv_begin(240, 35, C_BLACK);
    cv_text(120 - text_w(&FONT_S, FELUCCA_VERSION) / 2, 1,
            &FONT_S, FELUCCA_VERSION, RGB(196, 196, 204));
    cv_text(120 - text_w(&FONT_S, "KARMA / FELUCCA") / 2, 18,
            &FONT_S, "KARMA / FELUCCA", RGB(96, 96, 104));
    cv_blit(0, 202);
}
