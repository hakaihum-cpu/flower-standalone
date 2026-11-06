/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO-only physical UI control logic.
 * All hardware (encoders, LCD, LEDs, DSP, storage) is behind a future HAL.
 * This is not yet a complete or flashable firmware.
 */
#include "mi_control.h"
#include "mi_memory.h"
static int32_t limited(int32_t value, int32_t steps, int32_t lo, int32_t hi)
{
    if (steps > 4096) steps = 4096;
    if (steps < -4096) steps = -4096;
    value += steps;
    return value < lo ? lo : (value > hi ? hi : value);
}
static mi_result_t result(mi_action_t a, uint8_t slot)
{
    mi_result_t r = {a, slot};
    return r;
}
static void apply_preset(mi_state_t *s, unsigned preset)
{
    s->preset = (uint8_t)preset;
    s->wave = (uint8_t)(preset == 1 || preset == 5 ? 1 :
                        preset == 2 ? 2 : preset == 3 ? 3 : 0);
    s->width = (uint8_t)(preset == 3 ? 36 : 64);
    s->sub = (uint8_t)(preset == 3 ? 32 : 0);
    s->crush = 0;
    s->fx = (uint8_t)(preset == 4 ? MI_FX_REEL :
                      preset == 5 ? MI_FX_CHORUS : MI_FX_OFF);
    s->wet = (uint8_t)(preset == 4 ? 80 : preset == 5 ? 95 : 0);
    if (preset == 4) {
        s->fx_options[MI_FX_REEL][0] = 48;
        s->fx_options[MI_FX_REEL][1] = 45;
    }
    if (preset == 5) {
        s->fx_options[MI_FX_CHORUS][0] = 64;
        s->fx_options[MI_FX_CHORUS][1] = 80;
    }
}
void mi_init(mi_state_t *s)
{
    if (!s) return;
    mi_memzero(s, sizeof(*s));
    s->view = MI_HOME;
    s->width = 64;
    s->level = 100;
    s->global[0] = 90;
    s->project_slot = 1;
    s->fx_options[MI_FX_CHORUS][0] = 64;
    s->fx_options[MI_FX_CHORUS][1] = 80;
    s->fx_options[MI_FX_REEL][0] = 48;
    s->fx_options[MI_FX_REEL][1] = 45;
    s->env[0] = 4; s->env[1] = 65; s->env[2] = 115; s->env[3] = 45;
}
static void view(mi_state_t *s, mi_view_t v)
{
    s->view = v;
    s->save_armed = 0;
}
mi_result_t mi_button(mi_state_t *s, mi_button_t b, uint32_t now_ms)
{
    if (!s) return result(MI_ACTION_NONE, 0);
    switch (b) {
    case MI_B_HOME: view(s, MI_HOME); break;
    case MI_B_EDIT: view(s, MI_OSC); break;
    case MI_B_FX: view(s, MI_FX); break;
    case MI_B_SEL:
        /* Visualizer is a view, not an EUREKA sound selector.
         * Leave the selected MIYAKO FX and its WET state untouched.
         * Animation mode is chosen by the media renderer, not by UI navigation.
         */
        view(s, MI_VIDEO);
        break;
    case MI_B_ENV: view(s, MI_ENV); break;
    case MI_B_LFO: view(s, MI_LFO); break;
    case MI_B_ARP: view(s, MI_ARP); break;
    case MI_B_SEQ: view(s, MI_SEQ); break;
    case MI_B_GLO: view(s, MI_CONFIG); break;
    case MI_B_OCT_DOWN:
        s->octave = (int8_t)limited(s->octave, -1, -3, 3); break;
    case MI_B_OCT_UP:
        s->octave = (int8_t)limited(s->octave, +1, -3, 3); break;
    case MI_B_PLAY: return result(MI_ACTION_PLAY, 0);
    case MI_B_REC: return result(MI_ACTION_RECORD, 0);
    case MI_B_SAVE:
        if (s->view != MI_SAVE) {
            view(s, MI_SAVE);
        } else if (!s->save_armed ||
                   (uint32_t)(now_ms - s->save_armed_ms) > 4000u) {
            s->save_armed = 1;
            s->save_armed_ms = now_ms;
        } else {
            s->save_armed = 0;
            return result(MI_ACTION_SAVE, s->project_slot);
        }
        break;
    default: break;
    }
    return result(MI_ACTION_NONE, 0);
}
static void knob(mi_state_t *s, unsigned k, int32_t d)
{
    uint8_t *p = 0;
    int32_t lo = 0, hi = 127;
    if (k >= 4) return;
    switch (s->view) {
    case MI_HOME:
        if (k == 0) { p = &s->wave; hi = 3; }
        if (k == 1) { p = &s->fx; hi = MI_FX_COUNT - 1; }
        if (k == 2) p = &s->wet;
        if (k == 3) p = &s->level;
        break;
    case MI_OSC:
        if (k == 0) { p = &s->wave; hi = 3; }
        if (k == 1) p = &s->width;
        if (k == 2) p = &s->sub;
        if (k == 3) p = &s->crush;
        break;
    case MI_FX:
    case MI_VIDEO:
        if (k == 0) { p = &s->fx; hi = MI_FX_COUNT - 1; }
        if (k == 1) p = &s->wet;
        if (k > 1 && s->fx < MI_FX_COUNT) p = &s->fx_options[s->fx][k-2];
        break;
    case MI_ENV: p = &s->env[k]; break;
    case MI_LFO: p = &s->lfo[k]; break;
    case MI_ARP: p = &s->arp[k]; break;
    case MI_SEQ: p = &s->seq[k]; break;
    case MI_CONFIG: {
        static const int16_t mn[] = {40, 0, -50, 0};
        static const int16_t mx[] = {240, 100, 50, 2};
        s->global[k] = (int16_t)limited(s->global[k], d, mn[k], mx[k]);
        return;
    }
    case MI_SAVE:
        if (k == 0) { p = &s->project_slot; lo = 1; hi = 4; }
        break;
    default: break;
    }
    if (p) *p = (uint8_t)limited(*p, d, lo, hi);
}
mi_result_t mi_rotate(mi_state_t *s, mi_encoder_t e, int32_t steps)
{
    if (!s || !steps) return result(MI_ACTION_NONE, 0);
    switch (e) {
    case MI_EN_ALGORITHM:
        s->wave = (uint8_t)limited(s->wave, steps > 0 ? 1 : -1, 0, 3);
        break;
    case MI_EN_PRESETS: {
        unsigned next = (unsigned)limited(s->preset, steps > 0 ? 1 : -1, 0, 5);
        if (next != s->preset) apply_preset(s, next);
        break;
    }
    case MI_EN_SELECT:
        view(s, (mi_view_t)((s->view + MI_VIEW_COUNT + (steps > 0 ? 1 : -1))
                            % MI_VIEW_COUNT));
        break;
    case MI_EN_K1: case MI_EN_K2: case MI_EN_K3: case MI_EN_K4:
        knob(s, (unsigned)(e - MI_EN_K1), steps);
        break;
    default: break;
    }
    return result(MI_ACTION_NONE, 0);
}
const char *mi_view_name(mi_view_t v)
{
    static const char *const names[] = {
        "HOME", "OSC", "FX", "VIDEO", "ENV", "LFO",
        "ARP", "SEQ", "CONFIG", "SAVE"
    };
    return (unsigned)v < MI_VIEW_COUNT ? names[v] : "?";
}
const char *mi_fx_name(mi_fx_t effect)
{
    static const char *const names[] = {
        "OFF", "REEL", "DREAMY", "DELAY", "REVERB", "CHORUS"
    };
    return (unsigned)effect < MI_FX_COUNT ? names[effect] : "?";
}
const char *mi_param_name(const mi_state_t *s, unsigned k)
{
    static const char *const names[MI_VIEW_COUNT][4] = {
        {"WAVE","MODE","WET","LEVEL"}, {"WAVE","WIDTH","SUB","CRUSH"},
        {"MODE","WET","P3","P4"}, {"MODE","WET","P3","P4"},
        {"ATK","DEC","SUS","REL"}, {"RATE","WAVE","PHS","FADE"},
        {"MODE","RATE","OCT","GATE"}, {"LEN","DIV","SWG","GATE"},
        {"BPM","SWING","TUNE","CLOCK"}, {"SLOT","","",""}
    };
    if (!s || k > 3 || (unsigned)s->view >= MI_VIEW_COUNT) return "?";
    if ((s->view == MI_FX || s->view == MI_VIDEO) && k >= 2) {
        if (s->fx == MI_FX_REEL) return k == 2 ? "WOW" : "FLTR";
        if (s->fx == MI_FX_DELAY) return k == 2 ? "TIME" : "FDBK";
        if (s->fx == MI_FX_REVERB) return k == 2 ? "DECAY" : "TONE";
        if (s->fx == MI_FX_CHORUS) return k == 2 ? "RATE" : "DEPTH";
        /* Other DSP controls are pending source-accurate port; not fabricated. */
        return "TBD";
    }
    return names[s->view][k];
}
int32_t mi_param_value(const mi_state_t *s, unsigned k)
{
    if (!s || k > 3) return 0;
    switch (s->view) {
    case MI_HOME:
        return k == 0 ? s->wave : k == 1 ? s->fx : k == 2 ? s->wet : s->level;
    case MI_OSC:
        return k == 0 ? s->wave : k == 1 ? s->width : k == 2 ? s->sub : s->crush;
    case MI_FX: case MI_VIDEO:
        return k == 0 ? s->fx : k == 1 ? s->wet :
            (s->fx < MI_FX_COUNT ? s->fx_options[s->fx][k-2] : 0);
    case MI_ENV: return s->env[k];
    case MI_LFO: return s->lfo[k];
    case MI_ARP: return s->arp[k];
    case MI_SEQ: return s->seq[k];
    case MI_CONFIG: return s->global[k];
    case MI_SAVE: return k == 0 ? s->project_slot : 0;
    default: return 0;
    }
}
