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
/* Physical FM-1 encoder is intentionally coarse: ENV uses 4 units per
 * detent (~32 clicks full range); other continuous values use 2.
 * Keep enum selection and stepped pitch/clock rates exactly 1 per click. */
static int32_t fast_step(int32_t detents,unsigned rate)
{
    if(detents>4096)detents=4096;
    if(detents< -4096)detents=-4096;
    return detents*(int32_t)rate;
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
    s->filter_cutoff=127u;s->filter_resonance=0u;
    s->drum_tone=64u;s->drum_decay=64u;
    s->drum_color=64u;s->drum_level=100u;
    s->rec_bars=2u;
    s->video_params[0]=0u;s->video_params[1]=0u;
    s->video_params[2]=64u;s->video_params[3]=64u;
    s->fx_options[MI_FX_BITCRUSH][0]=32u;
    s->fx_options[MI_FX_BITCRUSH][1]=0u;
    s->fx_options[MI_FX_PITCHSHIFT][0]=64u;
    s->fx_options[MI_FX_PITCHSHIFT][1]=64u;
    s->fx_slicer[0]=64u;s->fx_slicer[1]=88u;
    /* REC editor: IN=0, OUT=127, pitch=0 semitones, level=100. */
    s->seq[0]=0;s->seq[1]=127;s->seq[2]=3;s->seq[3]=100;
    for(unsigned t=0u;t<2u;t++){
        s->rec_extra[t][0]=0u;s->rec_extra[t][1]=127u;
        s->rec_extra[t][2]=3u;s->rec_extra[t][3]=100u;
    }
    for(unsigned t=0u;t<3u;t++)s->rec_pan[t]=64u;
    s->step_len=8u; /* 4 silent FMDrum tracks, ready for white-key toggles */
    s->midi_channel=1u; /* default MIDI note input channel 1 */
    s->global[3]=0;     /* clock source: 0=INTERNAL, 1=MIDI */
}
static void view(mi_state_t *s, mi_view_t v)
{
    s->view = v;
    /* All routes back to HOME (including SELECT) must leave FILTER mode. */
    if(v==MI_HOME&&s->wave==MI_WAVE_MULTI&&s->osc_page>1u)
        s->osc_page=0u;
    if(v!=MI_SEQ)s->step_editor=0u;
    if(v!=MI_CONFIG)s->config_load_armed=0u;
    s->save_armed = 0;
}
mi_result_t mi_button(mi_state_t *s, mi_button_t b, uint32_t now_ms)
{
    if (!s) return result(MI_ACTION_NONE, 0);
    switch (b) {
    case MI_B_HOME:
        view(s, MI_HOME);
        /* FILTER is an EDIT-only page; HOME MULTI selects OSC1 or OSC2. */
        if(s->wave==MI_WAVE_MULTI&&s->osc_page>1u)s->osc_page=0u;
        break;
    case MI_B_EDIT:
        if(s->view==MI_OSC){
            if(s->osc_page==0u)s->osc_page=s->wave==MI_WAVE_MULTI?1u:2u;
            else if(s->osc_page==1u)s->osc_page=2u;
            else s->osc_page=0u;
        }
        else{if(s->wave!=MI_WAVE_MULTI && s->osc_page==1u)s->osc_page=0u;view(s, MI_OSC);}
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
    case MI_B_SEQ:
        if(s->view==MI_SEQ&&!s->step_editor)
            s->rec_page=(uint8_t)((s->rec_page+1u)%3u);
        else{view(s, MI_SEQ);s->rec_page=0u;}
        s->step_editor=0u;
        break;
    case MI_B_GLO: view(s, MI_CONFIG); break;
    case MI_B_OCT_DOWN:
        s->octave = (int8_t)limited(s->octave, -1, -3, 3); break;
    case MI_B_OCT_UP:
        s->octave = (int8_t)limited(s->octave, +1, -3, 3); break;
    case MI_B_PLAY: view(s,MI_SEQ);s->step_editor=0u;
        return result(MI_ACTION_PLAY, 0);
    case MI_B_REC: view(s,MI_SEQ);s->step_editor=0u;
        return result(MI_ACTION_RECORD, 0);
    case MI_B_SAVE:
        if (s->view != MI_SAVE) {
            view(s, MI_SAVE);
            s->save_action=0u;
        } else if (!s->save_armed ||
                   (uint32_t)(now_ms - s->save_armed_ms) > 4000u) {
            s->save_armed = 1;
            s->save_armed_ms = now_ms;
        } else {
            s->save_armed = 0;
            return result(s->save_action?MI_ACTION_LOAD:MI_ACTION_SAVE,
                          s->project_slot);
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
        /* HOME edits oscillator controls, never FX. MULTI's PRESET encoder
         * selects OSC1/OSC2 without changing the active synthesis mode.
         * K1 edits the selected waveform; K2 edits SUB or OSC2 DETUNE. */
        if(s->wave==MI_WAVE_MULTI){
            unsigned second=s->osc_page==1u;
            if(k==0){p=second?&s->osc2_wave:&s->osc1_wave;hi=3;}
            if(k==1)p=second?&s->osc2_detune:&s->sub;
            if(k==2)p=&s->osc2_level;
        }else{
            if(k==0){p=&s->wave;hi=MI_WAVE_PIANO;}
            if(s->wave==MI_WAVE_FMDRUM){
                if(k==1)p=&s->drum_tone;
                if(k==2)p=&s->drum_decay;
            }else if(s->wave==3u){
                if(k==1)p=&s->width;
                if(k==2)p=&s->sub;
            }else{
                if(k==1)p=&s->sub;
                if(k==2)p=&s->crush;
            }
        }
        if(k==3)p=&s->level;
        break;
    case MI_OSC:
        if(s->osc_page==2u){
            if(k==0)p=&s->filter_cutoff;
            if(k==1)p=&s->filter_resonance;
            if(k==2)p=&s->level;
            if(k==3)p=&s->crush;
        }else if(s->wave==MI_WAVE_FMDRUM){
            if(k==0)p=&s->drum_tone;
            if(k==1)p=&s->drum_decay;
            if(k==2)p=&s->drum_color;
            if(k==3)p=&s->drum_level;
        }else if(s->osc_page==0u){
            if(k==0){p=s->wave==MI_WAVE_MULTI?&s->osc1_wave:&s->wave;
                 hi=s->wave==MI_WAVE_MULTI?3:MI_WAVE_PIANO;}
            if(k==1)p=&s->width;
            if(k==2)p=&s->sub;
            if(k==3)p=&s->crush;
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
        if(k>1&&mode==MI_FX_SLICER)p=&s->fx_slicer[k-2];
        else if(k>1&&mode<MI_FX_STORAGE_COUNT)p=&s->fx_options[mode][k-2];
        break;
    }
    case MI_VIDEO: p=&s->video_params[k];break;
    case MI_ENV: p = &s->env[k]; break;
    case MI_LFO: p = &s->lfo[k]; if(k==1)hi=3; if(k==3)hi=MI_LFO_DEST_COUNT-1; break;
    case MI_ARP:
        p = &s->arp[k];
        if(k==0)hi=4;
        if(k==1)hi=3;
        if(k==2){lo=10;hi=100;}
        if(k==3){lo=1;hi=4;}
        break;
    case MI_SEQ:
        if(s->step_editor){
            if(k==0u)s->step_track=(uint8_t)limited(s->step_track,d,0,3);
            if(k==1u)s->step_play=(uint8_t)(d>0?1u:0u);
            if(k==2u)s->step_dir=(uint8_t)limited(s->step_dir,d,0,2);
            if(k==3u)s->step_len=(uint8_t)limited(s->step_len,d,8,16);
            return;
        }
        if(s->rec_page==2u){
            unsigned tr=s->rec_track;
            if(k==0u)s->rec_reverse[tr]^=1u;
            if(k==1u)s->rec_quant[tr]=(uint8_t)limited(s->rec_quant[tr],d,0,2);
            if(k==2u)s->rec_track=(uint8_t)limited(tr,d,0,2);
            return;
        }
        if(s->rec_page==1u){
            if(k==0u)s->rec_mute[s->rec_track]^=1u;
            if(k==1u)s->rec_pan[s->rec_track]=(uint8_t)
                limited(s->rec_pan[s->rec_track],fast_step(d,2u),0,127);
            if(k==2u)s->rec_track=(uint8_t)limited(s->rec_track,d,0,2);
            if(k==3u)s->rec_bars=(uint8_t)(s->rec_bars==2u?4u:2u);
            return;
        }
        p=s->rec_track?&s->rec_extra[s->rec_track-1u][k]:&s->seq[k];
        if(k==0){uint8_t out=s->rec_track?s->rec_extra[s->rec_track-1u][1]:s->seq[1];
                 hi=out>0u?(int32_t)out-1:0;}
        if(k==1){uint8_t in=s->rec_track?s->rec_extra[s->rec_track-1u][0]:s->seq[0];
                 lo=(int32_t)in+1;}
        if(k==2)hi=6;
        break;
    case MI_CONFIG:
        if(k==0u)
            s->midi_channel=(uint8_t)limited(s->midi_channel,d,1,16);
        else if(k==1u)
            s->global[3]=(int16_t)limited(s->global[3],d,0,1);
        else if(k==2u)
            s->project_slot=(uint8_t)limited(s->project_slot,d,1,4);
        if(k<3u)s->config_load_armed=0u;
        return;
    case MI_SAVE:
        if(k==0u) {s->project_slot=(uint8_t)limited(s->project_slot,d,1,4);}
        if(k==1u) {s->save_action=(uint8_t)limited(s->save_action,d,0,1);}
        s->save_armed=0u; /* switching slot/action cancels confirmation */
        return;
    default: break;
    }
    if(p){
        unsigned rate=s->view==MI_ENV?4u:2u;
        /* Choice knobs must never skip waveforms, FX, rhythm or octaves. */
        if((s->view==MI_HOME &&
            ((s->wave==MI_WAVE_MULTI&&k==0u)||
             (s->wave!=MI_WAVE_MULTI&&k==0u)))||
           (s->view==MI_OSC&&
            ((s->osc_page==0u&&k==0u&&s->wave!=MI_WAVE_FMDRUM)||
             (s->osc_page==1u&&s->wave!=MI_WAVE_FMDRUM&&
              (k==0u||k==3u))))||
           (s->view==MI_LFO&&(k==1u||k==3u))||
           (s->view==MI_ARP&&k!=2u)||
           (s->view==MI_SEQ&&k==2u)||
           (s->view==MI_SAVE))
            rate=1u;
        *p=(uint8_t)limited(*p,fast_step(d,rate),lo,hi);
    }
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
        /* Algorithm knob is not an accidental wave switch on FILTER. */
        if(s->view==MI_OSC&&s->osc_page==2u)break;
        if(s->view==MI_OSC&&s->wave==MI_WAVE_MULTI&&s->osc_page<2u){
            uint8_t *selected=s->osc_page?&s->osc2_wave:&s->osc1_wave;
            *selected=(uint8_t)limited(*selected,steps>0?1:-1,0,3);
        }else if(s->view==MI_OSC&&s->osc_page==1u){
            s->osc2_wave=(uint8_t)limited(s->osc2_wave,steps>0?1:-1,0,3);
        }else{
            uint8_t old=s->wave;
            s->wave=(uint8_t)limited(s->wave,steps>0?1:-1,0,
                                     s->view==MI_HOME?MI_WAVE_PIANO:MI_WAVE_PIANO);
            if(old!=MI_WAVE_MULTI&&s->wave==MI_WAVE_MULTI){
                s->osc1_wave=old;
                if(s->osc2_level==0u)s->osc2_level=64u;
                s->osc_page=0u;
            }
        }
        break;
    case MI_EN_PRESETS: {
        /* MULTI: PRESET selects which oscillator HOME is editing.
         * No preset is applied: that would overwrite MULTI/wave settings. */
        if(s->view==MI_HOME&&s->wave==MI_WAVE_MULTI){
            if(s->osc_page>1u)s->osc_page=0u;
            s->osc_page=(uint8_t)limited(s->osc_page,
                                        steps>0?1:-1,0,1);
            break;
        }
        if(s->view==MI_CONFIG||s->view==MI_SAVE){
            s->project_slot=(uint8_t)limited(s->project_slot,
                                           steps>0?1:-1,1,4);
            s->config_load_armed=0u;s->save_armed=0u;
            break;
        }
        if(s->view==MI_SEQ){
            /* PRESET selects one of the three independent REC tracks.
             * BAR length remains K4 on the REC MIX page. */
            s->rec_track=(uint8_t)limited(s->rec_track,steps>0?1:-1,0,2);
            break;
        }
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
    case MI_EN_SELECT: {
        /* SELECT traverses views; do not swallow fast encoder detents. */
        int32_t next=((int32_t)s->view+steps)%MI_VIEW_COUNT;
        if(next<0)next+=MI_VIEW_COUNT;
        view(s,(mi_view_t)next);
        break;
    }
    case MI_EN_K1: case MI_EN_K2: case MI_EN_K3: case MI_EN_K4:
        if(s->view==MI_CONFIG&&e==MI_EN_K4){
            /* Two explicit clockwise detents required for patch LOAD.
             * A counter-clockwise detent cancels the pending action. */
            if(steps<0)s->config_load_armed=0u;
            else if(s->config_load_armed){
                s->config_load_armed=0u;
                return result(MI_ACTION_LOAD,s->project_slot);
            }else s->config_load_armed=1u;
            break;
        }
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
    if((first==MI_FX_DREAMY&&second==MI_FX_PITCHSHIFT)||
       (first==MI_FX_PITCHSHIFT&&second==MI_FX_DREAMY))return 0;
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
        s->wave > MI_WAVE_PIANO || s->osc1_wave > 3u ||
        s->width > 127u || s->sub > 127u ||
        s->crush > 127u || s->level > 127u ||
        s->fx >= MI_FX_COUNT || s->fx2 >= MI_FX_COUNT ||
        s->wet > 127u || s->wet2 > 127u || s->fx_slot > 1u ||
        !mi_fx_pair_supported(s->fx, s->fx2) ||
        s->preset > 5u || s->project_slot < 1u ||
        s->project_slot > 4u || s->octave < -3 || s->octave > 3 ||
        s->seq[0] >= s->seq[1] || s->seq[1] > 127u ||
        s->seq[2] > 6u || s->seq[3] > 127u ||
        s->lfo[1] > 3u || s->lfo[3] >= MI_LFO_DEST_COUNT ||
        s->arp[0] > 4u || s->arp[1] > 3u ||
        s->arp[2] < 10u || s->arp[2] > 100u ||
        s->arp[3] < 1u || s->arp[3] > 4u ||
        s->global[0] < 40 || s->global[0] > 240 ||
        s->global[1] < 0 || s->global[1] > 100 ||
        s->global[2] < -50 || s->global[2] > 50 ||
        s->global[3] < 0 || s->global[3] > 1 ||
        s->osc2_wave>3u || s->osc2_level>127u ||
        s->osc2_detune>127u || s->osc2_octave>4u ||
        s->osc_page>2u || s->fx_page>1u ||
        s->filter_cutoff>127u||s->filter_resonance>127u||
        s->drum_tone>127u||s->drum_decay>127u||
        s->drum_color>127u||s->drum_level>127u||
        (s->rec_bars!=2u&&s->rec_bars!=4u)||
        s->step_track>=4u||s->step_play>1u||s->step_dir>2u||
        s->step_len<8u||s->step_len>16u||s->step_editor>1u||
        s->midi_channel<1u||s->midi_channel>16u||
        s->save_action>1u||s->config_load_armed>1u||
        s->rec_track>=3u||s->rec_page>2u)
        return 0;
    for (i = 0; i < 4u; ++i) {
        if (s->env[i] > 127u || s->lfo[i] > 127u || s->video_params[i]>127u)
            return 0;
    }
    for(i=0u;i<3u;i++)
        if(s->rec_mute[i]>1u||s->rec_pan[i]>127u||
           s->rec_reverse[i]>1u||s->rec_quant[i]>2u)return 0;
    for(i=0u;i<2u;i++){
        if(s->rec_extra[i][0]>=s->rec_extra[i][1] ||
           s->rec_extra[i][1]>127u || s->rec_extra[i][2]>6u ||
           s->rec_extra[i][3]>127u)return 0;
    }
    for (i = 0; i < MI_FX_STORAGE_COUNT; ++i) {
        for (j = 0; j < 2u; ++j)
            if (s->fx_options[i][j] > 127u)return 0;
        for (j = 0; j < 4u; ++j)
            if (s->fx_extra[i][j] > 127u)return 0;
    }
    for(i=0u;i<2u;i++)if(s->fx_slicer[i]>127u)return 0;
    return 1;
}

/* Any MIDI channel, like the existing selected-track CC21 pitch.
 * No new per-channel state or audio buffers. */
int mi_rec_cc(mi_state_t *s,unsigned cc,unsigned value)
{
    unsigned tr;
    if(!s||value>127u)return 0;
    if(cc>=MI_REC_LEVEL_CC_BASE&&cc<MI_REC_LEVEL_CC_BASE+3u){
        tr=cc-MI_REC_LEVEL_CC_BASE;
        if(tr==0u)s->seq[3]=(uint8_t)value;
        else s->rec_extra[tr-1u][3]=(uint8_t)value;
        return 1;
    }
    if(cc>=MI_REC_PAN_CC_BASE&&cc<MI_REC_PAN_CC_BASE+3u){
        s->rec_pan[cc-MI_REC_PAN_CC_BASE]=(uint8_t)value;
        return 1;
    }
    if(cc>=MI_REC_MUTE_CC_BASE&&cc<MI_REC_MUTE_CC_BASE+3u){
        s->rec_mute[cc-MI_REC_MUTE_CC_BASE]=(uint8_t)(value>=64u);
        return 1;
    }
    return 0;
}
const char *mi_wave_name(unsigned wave)
{
    static const char *const names[] = {"SINE","SAW","SQUARE","PULSE","MULTI","FMDRUM","PIANO"};
    return wave<=MI_WAVE_PIANO?names[wave]:"?";
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
const char *mi_lfo_dest_name(unsigned dest)
{
    static const char *const names[MI_LFO_DEST_COUNT]={
        "PITCH","PWM","AMP","SUB","CRUSH","CUTOFF","RES","OSC2 MIX","DETUNE","LEVEL","ATK","DEC","SUS","REL","FXA WET","FXA P1","FXA P2","FXB WET","FXB P1","FXB P2","DR TONE","DR DEC","DR COLOR","DR LEVEL","GLITCH","TILE","XSHIFT","YSHIFT","REC IN","REC OUT","REC PITCH","REC LEVEL","ARP RATE","ARP GATE","ARP OCT","BPM","SWING","TUNE","FXA PITCH","FXA REV","FXA SPREAD","FXA FDBK","FXB PITCH","FXB REV","FXB SPREAD","FXB FDBK","OSC2 OCT","OSC1 WAVE","OSC2 WAVE","OSC WAVE","FXA MODE","FXB MODE","ARP MODE","LFO RATE","LFO WAVE","LFO DEPTH","CLOCK","REC BARS"
    };
    return dest<MI_LFO_DEST_COUNT?names[dest]:"?";
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
        "OFF","REEL","DREAMY","DELAY","REVERB","CHORUS",
        "EUREKA","BITCRUSH","PITCHSHIFT","SLICER"
    };
    return (unsigned)effect < MI_FX_COUNT ? names[effect] : "?";
}
const char *mi_param_name(const mi_state_t *s, unsigned k)
{
    static const char *const names[MI_VIEW_COUNT][4] = {
        {"WAVE","SUB","CRUSH","LEVEL"}, {"WAVE","WIDTH","SUB","CRUSH"},
        {"MODE","WET","P3","P4"}, {"MODE","WET","P3","P4"},
        {"ATK","DEC","SUS","REL"}, {"RATE","WAVE","DEPTH","DEST"},
        {"MODE","RATE","GATE","OCT"}, {"IN","OUT","PITCH","LEVEL"},
        {"MIDI CH","CLOCK","PRESET","LOAD"},
        {"SLOT","ACTION","CONFIRM",""}
    };
    if (!s || k > 3 || (unsigned)s->view >= MI_VIEW_COUNT) return "?";
    if(s->view==MI_SEQ&&s->step_editor){
        static const char *const names[4]={"TRACK","PLAY","DIR","STEPS"};
        return names[k];
    }
    if(s->view==MI_SEQ&&s->rec_page==1u){
        static const char *const mix[4]={"MUTE","PAN","TRACK","BARS"};
        return mix[k];
    }
    if(s->view==MI_SEQ&&s->rec_page==2u){
        static const char *const transport[4]={"REV","QUANT","TRACK",""};
        return transport[k];
    }
    if(s->view==MI_VIDEO){
        static const char *const v[4]={"GLITCH","TILE","XSHIFT","YSHIFT"};
        return v[k];
    }
    if(s->view==MI_OSC&&s->osc_page==2u){
        static const char *const f[4]={"CUTOFF","RES","LEVEL","CRUSH"};
        return f[k];
    }
    if(s->view==MI_OSC&&s->wave==MI_WAVE_FMDRUM&&s->osc_page==0u){
        static const char *const d[4]={"TONE","DECAY","COLOR","D LEVEL"};
        return d[k];
    }
    if(s->view==MI_HOME){
        static const char *const multi1[4]={"WAVE","SUB","MIX2","LEVEL"};
        static const char *const multi2[4]={"WAVE","DETUNE","MIX2","LEVEL"};
        static const char *const pulse[4]={"WAVE","WIDTH","SUB","LEVEL"};
        static const char *const single[4]={"WAVE","SUB","CRUSH","LEVEL"};
        static const char *const drums[4]={"FMDRUM","TONE","DECAY","LEVEL"};
        if(s->wave==MI_WAVE_MULTI)
            return (s->osc_page==1u?multi2:multi1)[k];
        if(s->wave==MI_WAVE_FMDRUM)return drums[k];
        return s->wave==3u?pulse[k]:single[k];
    }
    if(s->view==MI_OSC&&s->wave==3u&&s->osc_page==0u&&k==1u)
        return "PWIDTH";
    if(s->view==MI_OSC&&s->wave==MI_WAVE_MULTI&&s->osc_page==0u&&k==0u)
        return "WAVE1";
    if (s->view==MI_OSC && s->osc_page==1u){
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
        if (fx == MI_FX_BITCRUSH) return k == 2 ? "BITS" : "HOLD";
        if (fx == MI_FX_PITCHSHIFT) return k == 2 ? "PITCH" : "GRAIN";
        if (fx == MI_FX_SLICER) return k == 2 ? "RATE" : "RND";
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
            int second=s->osc_page==1u;
            return k==0?(second?s->osc2_wave:s->osc1_wave):
                   k==1?(second?((int32_t)s->osc2_detune-64):(int32_t)s->sub):
                   k==2?s->osc2_level:s->level;
        }
        if(s->wave==MI_WAVE_FMDRUM)
            return k==0?s->wave:k==1?s->drum_tone:
                   k==2?s->drum_decay:s->level;
        if(s->wave==3u)
            return k==0?s->wave:k==1?s->width:
                   k==2?s->sub:s->level;
        return k==0?s->wave:k==1?s->sub:k==2?s->crush:s->level;
    case MI_OSC:
        if(s->osc_page==2u)return k==0?s->filter_cutoff:
            k==1?s->filter_resonance:k==2?s->level:s->crush;
        if(s->wave==MI_WAVE_FMDRUM)return k==0?s->drum_tone:
            k==1?s->drum_decay:k==2?s->drum_color:s->drum_level;
        if(s->osc_page==1u)
            return k==0?s->osc2_wave:
                   k==1?(int32_t)s->osc2_detune-64:
                   k==2?s->osc2_level:(int32_t)s->osc2_octave-2;
        return k==0?(s->wave==MI_WAVE_MULTI?s->osc1_wave:s->wave):
               k==1?s->width:k==2?s->sub:s->crush;
    case MI_VIDEO: return s->video_params[k];
    case MI_FX: {
        unsigned fx=s->fx_slot?s->fx2:s->fx;
        if(s->fx_page && (fx==MI_FX_DREAMY||fx==MI_FX_EUREKA))
            return k==0?((int32_t)s->fx_extra[fx][0]-64)*12/63:
                   k==1?(s->fx_extra[fx][1]>=64u?1:0):
                   (int32_t)s->fx_extra[fx][k];
        return k==0?(int32_t)fx:k==1?(int32_t)(s->fx_slot?s->wet2:s->wet):
            (fx==MI_FX_SLICER?(int32_t)s->fx_slicer[k-2]:
             fx<MI_FX_STORAGE_COUNT?(int32_t)s->fx_options[fx][k-2]:0);
    }
    case MI_ENV: return s->env[k];
    case MI_LFO: return s->lfo[k];
    case MI_ARP: return s->arp[k];
    case MI_SEQ:
        if(s->step_editor)
            return k==0?s->step_track+1u:k==1?s->step_play:
                   k==2?s->step_dir:s->step_len;
        if(s->rec_page==1u)return k==0?s->rec_mute[s->rec_track]:
            k==1?s->rec_pan[s->rec_track]:
            k==2?s->rec_track+1u:s->rec_bars;
        if(s->rec_page==2u)return k==0?s->rec_reverse[s->rec_track]:
            k==1?s->rec_quant[s->rec_track]:
            k==2?s->rec_track+1u:0;
        return s->rec_track?s->rec_extra[s->rec_track-1u][k]:s->seq[k];
    case MI_CONFIG: return k==0?s->midi_channel:k==1?s->global[3]:
                          k==2?s->project_slot:s->config_load_armed;
    case MI_SAVE: return k==0?s->project_slot:k==1?s->save_action:
                        k==2?s->save_armed:0;
    default: return 0;
    }
}
