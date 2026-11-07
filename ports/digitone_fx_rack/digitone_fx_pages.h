/* SPDX-License-Identifier: GPL-2.0-or-later
 * Native 128x64 Digitone-style pages for the future three exclusive audio FX rack.
 * Screen/UI source only. No audio owner, no key hooks, no builds are installed
 * by this file: a verified rack dispatcher must wire it after DSP integration.
 */
#ifndef DIGITONE_FX_PAGES_H
#define DIGITONE_FX_PAGES_H
#include <stdint.h>

enum dnfx_type {
    DNFX_GRANULAR = 0,
    DNFX_REEL = 1,
    DNFX_CRUSH = 2,
    DNFX_COUNT = 3
};
enum dnfx_style { DNFX_DIAL = 0, DNFX_SELECT = 1 };

struct dnfx_control {
    const char *label; /* 0 means deliberately unassigned; no fake control */
    uint8_t minimum, maximum, initial, style;
};
struct dnfx_state {
    uint8_t effect;
    uint8_t page;
    uint8_t value[2][8]; /* Stored for this effect; rack keeps three states */
    uint8_t focus;       /* 0..7 */
    uint8_t flash;       /* show changed control in stock-style title bar */
};
unsigned dnfx_page_count(unsigned effect);
const struct dnfx_control *dnfx_control_at(unsigned effect, unsigned page, unsigned knob);
void dnfx_defaults(struct dnfx_state *state, unsigned effect);
int dnfx_edit(struct dnfx_state *state, unsigned knob, int detents);
int dnfx_set_page(struct dnfx_state *state, unsigned page);
void dnfx_tick(struct dnfx_state *state);
void dnfx_draw(void *bmp, const struct dnfx_state *state, unsigned tempo_bpm);
#endif
