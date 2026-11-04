/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO FM-1: hardware-only navigation; not a touchscreen UI.
 * This module is included by KARMA ui_input.c immediately before layer_tap().
 *
 * Existing FM-1 controls stay intact: ALGORITHM=track, PRESETS=sound,
 * SELECT=page, KNOB1..4=parameter, OCT buttons=octave, HOME=back.
 * The short FX and EDIT taps are the only deliberately changed mappings.
 * Long-press FX (LY_FX), the drum pages and sequencer erase are preserved.
 */
static int mi_panel_selected(void)
{
    return !is_drum(TSEL) && TSEL->eng_req % NENGINES == ENGI_ACID;
}

/* Resolve by page metadata, never a fragile numeric page index. */
static uint32_t mi_panel_sound_page(uint32_t first_param)
{
    uint32_t i;
    for (i = 0u; i < NPAGES; ++i)
        if (PAGES[i].fam == FAM_EDIT && PAGES[i].scope == SC_ENGINE &&
            PAGES[i].id[0] == first_param)
            return i;
    return NPAGES;
}

/* EDIT from HOME/another family begins at OSC, not a stale VOICE page.
 * On an EDIT page, the physical EDIT key still cycles the normal pages. */
static void mi_panel_edit_tap(void)
{
    if (mi_panel_selected() && (ui.home || cur_page()->fam != FAM_EDIT))
        ui.fam_last[FAM_EDIT] = 0u;
    open_family(FAM_EDIT);
}

/* Short FX tap on MIYAKO:
 *   from HOME/OSC/other family -> MIYAKO's REEL/EUREKA page
 *   from MIYAKO FX -> native KARMA FX bus (including CLOUD)
 *   inside native FX family -> native FX page cycle; SELECT goes both ways
 * Other tracks keep KARMA's original FX mapping.
 */
static void mi_panel_fx_tap(void)
{
    uint32_t page;
    if (!mi_panel_selected() || (!ui.home && cur_page()->fam == FAM_FX)) {
        open_family(FAM_FX);
        return;
    }
    page = mi_panel_sound_page(P_E4);
    if (page >= NPAGES) {
        open_family(FAM_FX);   /* fail safe if upstream EDIT pages change */
        return;
    }
    if (!ui.home && ui.page == page) {
        ui.fam_last[FAM_FX] = 0u; /* start shared FX at FX, not a stale CLOUD page */
        open_family(FAM_FX);
        return;
    }
    ui.page = (uint8_t)page;
    ui.fam_last[FAM_EDIT] = ui.page;
    ui.home = 0u;
    page_entered();          /* clear old focus and force an actual redraw */
}
