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
    s->fx2 = MI_FX_OFF; s->fx_slot=0;
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
    s->fx2 = MI_FX_OFF; s->wet2=80; s->fx_slot=0;
    s->global[0] = 90;
    s->project_slot = 1;
    s->fx_options[MI_FX_CHORUS][0] = 64;
    s->fx_options[MI_FX_CHORUS][1] = 80;
    s->fx_options[MI_FX_REEL][0] = 48;
    s->fx_options[MI_FX_REEL][1] = 45;
    s->env[0] = 4; s->env[1] = 65; s->env[2] = 115; s->env[3] = 45;
    /* ARP disabled at start; 1/16 note gate 70%, one octave. */
    s->arp[0] = 0; s->arp[1] = 2; s->arp[2] = 70; s->arp[3] = 1;
    /* LFO off unless depth is raised. Wave 0=sine, target 0=pitch. */
    s->lfo[0] = 35; s->lfo[1] = 0; s->lfo[2] = 0; s->lfo[3] = 0;
    s->fx_options[MI_FX_DREAMY][0] = 64;
    s->fx_options[MI_FX_DREAMY][1] = 80;
    s->fx_options[MI_FX_EUREKA][0] = 48;
    s->fx_options[MI_FX_EUREKA][1] = 64;
    s->osc2_wave=0u;s->osc2_level=0u;s->osc2_detune=64u;
    s->osc2_octave=2u;s->osc_page=0u;s->osc1_wave=0u;
    s->fx_extra[MI_FX_DREAMY][0]=64u;
    s->fx_extra[MI_FX_DREAMY][1]=0u;
    s->fx_extra[MI_FX_DREAMY][2]=64u;
    s->fx_extra[MI_FX_DREAMY][3]=64u;
    s->fx_extra[MI_FX_EUREKA][0]=64u;
    s->fx_extra[MI_FX_EUREKA][1]=0u;
    s->fx_extra[MI_FX_EUREKA][2]=64u;
    s->fx_extra[MI_FX_EUREKA][3]=0u;
    s->fx_page=0u;
    /* REC editor: IN=0, OUT=127, pitch=0 semitones, level=100. */
    s->seq[0]=0;s->seq[1]=127;s->seq[2]=3;s->seq[3]=100;
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
    case MI_B_EDIT:
        if(s->view==MI_OSC)s->osc_page^=1u;
        else{if(s->wave!=MI_WAVE_MULTI)s->osc_page=0u;view(s, MI_OSC);}
        break;
    case MI_B_FX:
        if(s->view==MI_FX){s->fx_slot^=1u;}
        else{view(s,MI_FX);s->fx_slot=0u;}
        /* Entering or switching FX A/B always exposes MODE on P1.
         * PRESET detents select P2 only when deliberately requested. */
        s->fx_page=0u;
        break;
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
    case MI_B_PLAY: view(s,MI_SEQ);return result(MI_ACTION_PLAY, 0);
    case MI_B_REC: view(s,MI_SEQ);return result(MI_ACTION_RECORD, 0);
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
    uint8_t previous_wave=s->wave;
    if (k >= 4) return;
    switch (s->view) {
    case MI_HOME:
        if(k==0){p=&s->wave;hi=MI_WAVE_MULTI;}
        if(k==1){if(s->wave==MI_WAVE_MULTI){p=&s->osc_page;hi=1;}
                 else{p=&s->fx;hi=MI_FX_COUNT-1;}}
        if(k==2)p=s->wave==MI_WAVE_MULTI?&s->osc2_detune:&s->wet;
        if(k==3)p=&s->level;
        break;
    case MI_OSC:
        if(s->osc_page==0u){
            if (k == 0) { p = s->wave==MI_WAVE_MULTI?&s->osc1_wave:&s->wave; hi = 3; }
            if (k == 1) p = &s->width;
            if (k == 2) p = &s->sub;
            if (k == 3) p = &s->crush;
        }else{
            if(k==0){p=&s->osc2_wave;hi=3;}
            if(k==1)p=&s->osc2_detune;
            if(k==2)p=&s->osc2_level;
            if(k==3){p=&s->osc2_octave;hi=4;}
        }
        break;
    case MI_FX: {
        unsigned mode=s->fx_slot?s->fx2:s->fx;
        if(s->fx_page && (mode==MI_FX_DREAMY||mode==MI_FX_EUREKA)){
            p=&s->fx_extra[mode][k];
            if(k==1)hi=127; /* REV: 0..63 off, 64..127 on. */
            break;
        }
        if(k==0){
            uint8_t candidate=(uint8_t)limited(mode,d,0,MI_FX_COUNT-1);
            unsigned one=s->fx_slot?s->fx:candidate;
            unsigned two=s->fx_slot?candidate:s->fx2;
            int32_t step=d>0?1:-1;
            while(!mi_fx_pair_supported(one,two) && candidate<MI_FX_COUNT){
                int32_t next=(int32_t)candidate+step;
                if(next<0||next>=MI_FX_COUNT)return;
                candidate=(uint8_t)next;
                one=s->fx_slot?s->fx:candidate;
                two=s->fx_slot?candidate:s->fx2;
            }
            if(s->fx_slot)s->fx2=candidate;
            else s->fx=candidate;
            /* Do not steal K1 from MODE during DREAMY/EUREKA selection.
             * PRESET still toggles P1/P2 explicitly. */
            s->fx_page=0u;
            return;
        }
        if(k==1)p=s->fx_slot?&s->wet2:&s->wet;
        if(k>1&&mode<MI_FX_COUNT)p=&s->fx_options[mode][k-2];
        break;
    }
    case MI_VIDEO:
        /* SEL is a true media-only screen: avoid hidden knob changes. */
        break;
    case MI_ENV: p = &s->env[k]; break;
    case MI_LFO: p = &s->lfo[k]; if(k==1)hi=3; if(k==3)hi=2; break;
    case MI_ARP:
        p = &s->arp[k];
        if(k==0)hi=4;
        if(k==1)hi=3;
        if(k==2){lo=10;hi=100;}
        if(k==3){lo=1;hi=4;}
        break;
    case MI_SEQ:
        p=&s->seq[k];
        if(k==0)hi=s->seq[1]>0u?(int32_t)s->seq[1]-1:0;
        if(k==1)lo=(int32_t)s->seq[0]+1;
        if(k==2)hi=6;
        break;
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
    if(s->view==MI_HOME&&k==0u &&
       previous_wave!=MI_WAVE_MULTI&&s->wave==MI_WAVE_MULTI){
        s->osc1_wave=previous_wave;
        if(s->osc2_level==0u)s->osc2_level=64u;
        s->osc_page=0u;
    }
    if(s->view==MI_HOME && !mi_fx_pair_supported(s->fx,s->fx2))
        s->fx2=MI_FX_OFF;
}
mi_result_t mi_rotate(mi_state_t *s, mi_encoder_t e, int32_t steps)
{
    if (!s || !steps) return result(MI_ACTION_NONE, 0);
    switch (e) {
    case MI_EN_ALGORITHM:
        /* Never silently exit MULTI while an FX/VIDEO/other page is open. */
        if(s->view!=MI_HOME&&s->view!=MI_OSC)break;
        if(s->view==MI_OSC&&s->wave==MI_WAVE_MULTI){
            uint8_t *selected=s->osc_page?&s->osc2_wave:&s->osc1_wave;
            *selected=(uint8_t)limited(*selected,steps>0?1:-1,0,3);
        }else if(s->view==MI_OSC&&s->osc_page){
            s->osc2_wave=(uint8_t)limited(s->osc2_wave,steps>0?1:-1,0,3);
        }else{
            uint8_t old=s->wave;
            s->wave=(uint8_t)limited(s->wave,steps>0?1:-1,0,
                                     s->view==MI_HOME?MI_WAVE_MULTI:3);
            if(old!=MI_WAVE_MULTI&&s->wave==MI_WAVE_MULTI){
                s->osc1_wave=old;
                if(s->osc2_level==0u)s->osc2_level=64u;
                s->osc_page=0u;
            }
        }
        break;
    case MI_EN_PRESETS: {
        if(s->view==MI_FX){
            unsigned mode=s->fx_slot?s->fx2:s->fx;
            if(mode==MI_FX_DREAMY||mode==MI_FX_EUREKA)
                s->fx_page^=1u; /* one physical detent => page change */
            break;
        }
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
int mi_fx_pair_supported(unsigned first,unsigned second)
{
    if(first>=MI_FX_COUNT||second>=MI_FX_COUNT)return 0;
    if(first==MI_FX_OFF||second==MI_FX_OFF)return 1;
    if(first==second)return 0;
    if((first==MI_FX_DELAY&&second==MI_FX_EUREKA)||
       (first==MI_FX_EUREKA&&second==MI_FX_DELAY))return 0;
    return 1;
}

/* Reject corrupted flash state BEFORE the audio thread, UI indexers and
 * recorder consume parameters. Test this exact predicate as native C. */
int mi_state_valid(const mi_state_t *s)
{
    unsigned i, j;
    if (!s || (unsigned)s->view >= MI_VIEW_COUNT ||
        s->wave > MI_WAVE_MULTI || s->osc1_wave > 3u ||
        s->width > 127u || s->sub > 127u ||
        s->crush > 127u || s->level > 127u ||
        s->fx >= MI_FX_COUNT || s->fx2 >= MI_FX_COUNT ||
        s->wet > 127u || s->wet2 > 127u || s->fx_slot > 1u ||
        !mi_fx_pair_supported(s->fx, s->fx2) ||
        s->preset > 5u || s->project_slot < 1u ||
        s->project_slot > 4u || s->octave < -3 || s->octave > 3 ||
        s->seq[0] >= s->seq[1] || s->seq[1] > 127u ||
        s->seq[2] > 6u || s->seq[3] > 127u ||
        s->lfo[1] > 3u || s->lfo[3] > 2u ||
        s->arp[0] > 4u || s->arp[1] > 3u ||
        s->arp[2] < 10u || s->arp[2] > 100u ||
        s->arp[3] < 1u || s->arp[3] > 4u ||
        s->global[0] < 40 || s->global[0] > 240 ||
        s->global[1] < 0 || s->global[1] > 100 ||
        s->global[2] < -50 || s->global[2] > 50 ||
        s->global[3] < 0 || s->global[3] > 2 ||
        s->osc2_wave>3u || s->osc2_level>127u ||
        s->osc2_detune>127u || s->osc2_octave>4u ||
        s->osc_page>1u || s->fx_page>1u)
        return 0;
    for (i = 0; i < 4u; ++i) {
        if (s->env[i] > 127u || s->lfo[i] > 127u)
            return 0;
    }
    for (i = 0; i < MI_FX_COUNT; ++i) {
        for (j = 0; j < 2u; ++j)
            if (s->fx_options[i][j] > 127u)return 0;
        for (j = 0; j < 4u; ++j)
            if (s->fx_extra[i][j] > 127u)return 0;
    }
    return 1;
}

const char *mi_wave_name(unsigned wave)
{
    static const char *const names[] = {"SINE","SAW","SQUARE","PULSE","MULTI"};
    return wave<=MI_WAVE_MULTI?names[wave]:"?";
}
const char *mi_arp_mode_name(unsigned mode)
{
    static const char *const names[] = {"OFF","UP","DOWN","UPDN","RND"};
    return mode<5u?names[mode]:"?";
}
const char *mi_lfo_wave_name(unsigned wave)
{
    static const char *const names[] = {"SINE","TRI","SAW","SQUARE"};
    return wave<4u?names[wave]:"?";
}
const char *mi_view_name(mi_view_t v)
{
    static const char *const names[] = {
        "HOME", "OSC", "FX", "VIDEO", "ENV", "LFO",
        "ARP", "RECORDER", "CONFIG", "SAVE"
    };
    return (unsigned)v < MI_VIEW_COUNT ? names[v] : "?";
}
const char *mi_fx_name(mi_fx_t effect)
{
    static const char *const names[] = {
        "OFF", "REEL", "DREAMY", "DELAY", "REVERB", "CHORUS", "EUREKA"
    };
    return (unsigned)effect < MI_FX_COUNT ? names[effect] : "?";
}
const char *mi_param_name(const mi_state_t *s, unsigned k)
{
    static const char *const names[MI_VIEW_COUNT][4] = {
        {"WAVE","MODE","WET","LEVEL"}, {"WAVE","WIDTH","SUB","CRUSH"},
        {"MODE","WET","P3","P4"}, {"MODE","WET","P3","P4"},
        {"ATK","DEC","SUS","REL"}, {"RATE","WAVE","DEPTH","DEST"},
        {"MODE","RATE","GATE","OCT"}, {"IN","OUT","PITCH","LEVEL"},
        {"BPM","SWING","TUNE","CLOCK"}, {"SLOT","","",""}
    };
    if (!s || k > 3 || (unsigned)s->view >= MI_VIEW_COUNT) return "?";
    if (s->view == MI_VIDEO) return "";
    if(s->view==MI_HOME&&s->wave==MI_WAVE_MULTI){
        static const char *const multi[4]={"WAVE","MODE","DETUNE","LEVEL"};
        return multi[k];
    }
    if(s->view==MI_OSC&&s->wave==MI_WAVE_MULTI&&!s->osc_page&&k==0u)
        return "WAVE1";
    if (s->view==MI_OSC && s->osc_page){
        static const char *const osc2_names[4]={"WAVE2","DETUNE","MIX2","OCT2"};
        return osc2_names[k];
    }
    if (s->view == MI_FX && s->fx_page){
        static const char *const more[4]={
            "PITCH","REVERSE","SPREAD","FEEDBK"
        };
        unsigned fx=s->fx_slot?s->fx2:s->fx;
        if(k==2u && fx==MI_FX_EUREKA)return "GRAIN";
        return more[k];
    }
    if (s->view == MI_FX && k >= 2) {
        unsigned fx=s->fx_slot?s->fx2:s->fx;
        if (fx == MI_FX_REEL) return k == 2 ? "WOW" : "FLTR";
        if (fx == MI_FX_DREAMY) return k == 2 ? "GRAIN" : "SPACE";
        if (fx == MI_FX_DELAY) return k == 2 ? "TIME" : "FDBK";
        if (fx == MI_FX_REVERB) return k == 2 ? "DECAY" : "TONE";
        if (fx == MI_FX_CHORUS) return k == 2 ? "RATE" : "DEPTH";
        if (fx == MI_FX_EUREKA) return k == 2 ? "TIME" : "HAZE";
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
        if(s->wave==MI_WAVE_MULTI){
            /* Return signed values explicitly: DETUNE may be negative and
             * MI_WAVE_MULTI is an unsigned macro (4u). */
            if(k==0u)return (int32_t)MI_WAVE_MULTI;
            if(k==1u)return (int32_t)s->osc_page;
            if(k==2u)return (int32_t)s->osc2_detune-64;
            return (int32_t)s->level;
        }
        return k==0?s->wave:k==1?s->fx:k==2?s->wet:s->level;
    case MI_OSC:
        if(s->osc_page)
            return k==0?s->osc2_wave:
                   k==1?(int32_t)s->osc2_detune-64:
                   k==2?s->osc2_level:(int32_t)s->osc2_octave-2;
        return k==0?(s->wave==MI_WAVE_MULTI?s->osc1_wave:s->wave):
               k==1?s->width:k==2?s->sub:s->crush;
    case MI_VIDEO: return 0;
    case MI_FX: {
        unsigned fx=s->fx_slot?s->fx2:s->fx;
        if(s->fx_page && (fx==MI_FX_DREAMY||fx==MI_FX_EUREKA))
            return k==0?((int32_t)s->fx_extra[fx][0]-64)*12/63:
                   k==1?(s->fx_extra[fx][1]>=64u?1:0):
                   (int32_t)s->fx_extra[fx][k];
        return k==0?(int32_t)fx:k==1?(int32_t)(s->fx_slot?s->wet2:s->wet):
            (fx<MI_FX_COUNT?(int32_t)s->fx_options[fx][k-2]:0);
    }
    case MI_ENV: return s->env[k];
    case MI_LFO: return s->lfo[k];
    case MI_ARP: return s->arp[k];
    case MI_SEQ: return s->seq[k];
    case MI_CONFIG: return s->global[k];
    case MI_SAVE: return k == 0 ? s->project_slot : 0;
    default: return 0;
    }
}
