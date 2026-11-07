/* Standalone host regression: cc -std=c99 -Wall -Wextra -Werror
 * fm1/native/mi_control.c fm1/native/test_mi_control.c -o /tmp/mi_control_test
 * /tmp/mi_control_test
 */
#include "mi_control.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    mi_state_t s;
    mi_result_t r;
    unsigned i;
    mi_init(&s);
    assert(s.view == MI_HOME && s.wave == 0 && s.fx == MI_FX_OFF);
    assert(!strcmp(mi_param_name(&s,0),"WAVE"));
    assert(!strcmp(mi_wave_name(0),"SINE"));
    assert(!strcmp(mi_wave_name(1),"SAW"));
    assert(!strcmp(mi_wave_name(2),"SQUARE"));
    assert(!strcmp(mi_wave_name(3),"PULSE"));
    assert(!strcmp(mi_wave_name(MI_WAVE_MULTI),"MULTI"));
    assert(!strcmp(mi_arp_mode_name(4),"RND"));
    mi_button(&s, MI_B_EDIT, 0);
    mi_rotate(&s, MI_EN_K1, 3);
    assert(s.view == MI_OSC && s.wave == 3);
    mi_button(&s, MI_B_FX, 0);
    mi_rotate(&s, MI_EN_K1, 2);
    assert(s.fx == MI_FX_DREAMY);
    assert(!strcmp(mi_param_name(&s,0),"PITCH"));
    assert(!strcmp(mi_param_name(&s,2),"SPREAD"));
    assert(mi_param_value(&s,2)==64 && mi_param_value(&s,3)==64);
    mi_button(&s, MI_B_HOME, 0);
    mi_rotate(&s, MI_EN_PRESETS, 1);
    assert(s.preset == 1 && s.wave == 1);
    mi_rotate(&s, MI_EN_PRESETS, 4096);
    assert(s.preset == 2); /* physical detent: one preset per event */
    for(i=0;i<4;++i) mi_rotate(&s, MI_EN_PRESETS, 1);
    assert(s.preset == 5 && s.fx == MI_FX_CHORUS);
    s.wet=0;
    mi_button(&s, MI_B_SEL, 12);
    assert(s.view == MI_VIDEO && s.wet == 0 && s.fx == MI_FX_CHORUS);
    assert(!strcmp(mi_param_name(&s,0),""));
    mi_rotate(&s, MI_EN_K1, 60); /* SEL cannot silently change current FX */
    assert(s.fx == MI_FX_CHORUS);
    mi_button(&s, MI_B_FX, 13);
    mi_rotate(&s, MI_EN_K1, -1);
    assert(s.fx == MI_FX_REVERB);
    mi_button(&s, MI_B_SEL, 14);
    assert(s.view == MI_VIDEO && s.fx == MI_FX_REVERB && s.wet == 0);
    assert(!strcmp(mi_fx_name(MI_FX_REVERB), "REVERB"));
    mi_button(&s, MI_B_GLO, 12);
    mi_rotate(&s, MI_EN_K1, 2000);
    assert(s.global[0] == 240);
    mi_rotate(&s, MI_EN_K3, -2000);
    assert(s.global[2] == -50);
    assert(mi_button(&s, MI_B_SAVE, 100).action == MI_ACTION_NONE);
    assert(s.view == MI_SAVE && !s.save_armed);
    mi_rotate(&s, MI_EN_K1, 1);
    assert(s.project_slot == 2);
    assert(mi_button(&s, MI_B_SAVE, 200).action == MI_ACTION_NONE);
    r = mi_button(&s, MI_B_SAVE, 500);
    assert(r.action == MI_ACTION_SAVE && r.slot == 2);
    mi_button(&s, MI_B_HOME, 1000);
    mi_button(&s, MI_B_SAVE, 1100);
    mi_button(&s, MI_B_SAVE, 1200);
    r=mi_button(&s, MI_B_SAVE, 7000);
    assert(r.action == MI_ACTION_NONE && s.save_armed);
    assert(mi_button(&s, MI_B_PLAY, 7001).action == MI_ACTION_PLAY);
    assert(mi_button(&s, MI_B_REC, 7001).action == MI_ACTION_RECORD);
    assert(s.view==MI_SEQ);
    assert(!strcmp(mi_view_name(s.view),"RECORDER"));
    assert(!strcmp(mi_param_name(&s,0),"IN"));
    assert(!strcmp(mi_param_name(&s,2),"PITCH"));
    mi_rotate(&s,MI_EN_K1,24);
    mi_rotate(&s,MI_EN_K2,-20);
    mi_rotate(&s,MI_EN_K3,3);
    assert(s.seq[0]==24&&s.seq[1]==107&&s.seq[2]==6);
    mi_rotate(&s,MI_EN_K2,-500);
    assert(s.seq[1]==25);
    for(i=0;i<5;i++) mi_button(&s, MI_B_OCT_DOWN, 7001);
    assert(s.octave == -3);
    assert(MI_FX_COUNT == 7 && MI_VIEW_COUNT == 10);
    assert(!strcmp(mi_fx_name(MI_FX_EUREKA),"EUREKA"));
    assert(mi_fx_pair_supported(MI_FX_DREAMY,MI_FX_EUREKA));
    assert(!mi_fx_pair_supported(MI_FX_DELAY,MI_FX_EUREKA));
    assert(!mi_fx_pair_supported(MI_FX_REVERB,MI_FX_REVERB));
    mi_button(&s,MI_B_ARP,8000);
    mi_rotate(&s,MI_EN_K1,100);
    mi_rotate(&s,MI_EN_K2,100);
    mi_rotate(&s,MI_EN_K3,-100);
    mi_rotate(&s,MI_EN_K4,100);
    assert(s.arp[0]==4 && s.arp[1]==3 && s.arp[2]==10 && s.arp[3]==4);
    assert(!strcmp(mi_param_name(&s,0),"MODE"));
    assert(!strcmp(mi_param_name(&s,2),"GATE"));
    mi_button(&s,MI_B_LFO,8001);
    mi_rotate(&s,MI_EN_K2,20);
    mi_rotate(&s,MI_EN_K4,20);
    assert(s.lfo[1]==3 && s.lfo[3]==2);
    assert(!strcmp(mi_param_name(&s,2),"DEPTH"));
    mi_button(&s,MI_B_ENV,8002);
    assert(!strcmp(mi_param_name(&s,0),"ATK"));
    assert(!strcmp(mi_param_name(&s,3),"REL"));
    mi_button(&s,MI_B_FX,9000);
    assert(s.fx_slot==0u);
    mi_rotate(&s,MI_EN_K1,-50);
    mi_rotate(&s,MI_EN_K1,3); /* A=DELAY */
    assert(s.fx==MI_FX_DELAY);
    mi_button(&s,MI_B_FX,9001);
    assert(s.fx_slot==1u);
    mi_rotate(&s,MI_EN_K1,3); /* A=DELAY, skips duplicate DELAY */
    assert(s.fx2==MI_FX_REVERB);
    assert(mi_fx_pair_supported(s.fx,s.fx2));
    mi_button(&s,MI_B_FX,9002);
    assert(s.fx_slot==0u);

    /* OSC2 is NOT the former SUB; the original OSC1 page stays intact. */
    mi_button(&s,MI_B_EDIT,9010);
    assert(s.view==MI_OSC && s.osc_page==0u);
    mi_rotate(&s,MI_EN_K1,-50); /* original OSC1 waveform */
    assert(s.wave==0u);
    mi_button(&s,MI_B_EDIT,9011); /* repeated EDIT changes page */
    assert(s.osc_page==1u);
    assert(!strcmp(mi_param_name(&s,0),"WAVE2"));
    assert(!strcmp(mi_param_name(&s,1),"DETUNE"));
    mi_rotate(&s,MI_EN_K1,1);
    mi_rotate(&s,MI_EN_K2,10);
    mi_rotate(&s,MI_EN_K3,70);
    mi_rotate(&s,MI_EN_K4,1);
    assert(s.osc2_wave==1u && s.osc2_detune==74u &&
           s.osc2_level==70u && s.osc2_octave==3u);
    assert(s.wave==0u && mi_param_value(&s,1)==10);
    assert(mi_param_value(&s,3)==1);
    mi_button(&s,MI_B_EDIT,9012);
    assert(s.osc_page==0u && !strcmp(mi_param_name(&s,2),"SUB"));
    mi_button(&s,MI_B_HOME,9013);
    /* The second oscillator's waveform and tuning cannot be confused
     * with legacy SUB/CRUSH or corrupted saved-state values. */
    mi_init(&s);
    assert(s.osc2_level==0u && s.osc2_octave==2u && s.osc2_detune==64u);
    assert(mi_state_valid(&s));
    s.osc2_wave=4u; assert(!mi_state_valid(&s)); s.osc2_wave=0u;
    s.osc2_octave=5u; assert(!mi_state_valid(&s)); s.osc2_octave=2u;
    s.osc_page=2u; assert(!mi_state_valid(&s)); s.osc_page=0u;
    s.osc2_level=255u; assert(!mi_state_valid(&s)); s.osc2_level=0u;
    assert(mi_state_valid(&s));
    /* Extra FX controls must not steal SELECT visual-bank or FX A/B routing.
     * PRESET encoder in DREAMY/EUREKA cycles P1/P2 without loading presets. */
    mi_init(&s);
    mi_button(&s,MI_B_FX,9200);
    mi_rotate(&s,MI_EN_K1,2); /* DREAMY */
    assert(s.fx==MI_FX_DREAMY);
    assert(s.fx_page==1u); /* PITCH visible immediately after selection */
    mi_rotate(&s,MI_EN_PRESETS,1);
    assert(s.fx_page==0u && s.fx==MI_FX_DREAMY);
    mi_rotate(&s,MI_EN_PRESETS,1);
    assert(s.fx_page==1u && s.fx==MI_FX_DREAMY);
    assert(!strcmp(mi_param_name(&s,0),"PITCH"));
    assert(!strcmp(mi_param_name(&s,1),"REVERSE"));
    assert(!strcmp(mi_param_name(&s,2),"SPREAD"));
    mi_rotate(&s,MI_EN_K1,12); /* 64 -> 76 => 2 semitones */
    mi_rotate(&s,MI_EN_K2,127);
    mi_rotate(&s,MI_EN_K3,20);
    mi_rotate(&s,MI_EN_K4,10);
    assert(mi_param_value(&s,0)==2);
    assert(mi_param_value(&s,1)==1);
    assert(s.fx_extra[MI_FX_DREAMY][2]==84u);
    assert(s.fx_extra[MI_FX_DREAMY][3]==74u);
    assert(s.fx_options[MI_FX_DREAMY][0]==64u);
    assert(s.fx_options[MI_FX_DREAMY][1]==80u);
    mi_rotate(&s,MI_EN_PRESETS,-1);
    assert(s.fx_page==0u);
    assert(!strcmp(mi_param_name(&s,2),"GRAIN"));
    mi_rotate(&s,MI_EN_K1,4); /* EUREKA, same selected FX slot */
    assert(s.fx==MI_FX_EUREKA);
    assert(s.fx_page==1u);
    assert(!strcmp(mi_param_name(&s,0),"PITCH"));
    assert(!strcmp(mi_param_name(&s,2),"GRAIN"));
    mi_rotate(&s,MI_EN_PRESETS,1);
    assert(s.fx_page==0u);
    assert(!strcmp(mi_param_name(&s,2),"TIME"));
    assert(s.fx_extra[MI_FX_EUREKA][0]==64u);
    mi_button(&s,MI_B_FX,9201);
    assert(s.fx_slot==1u&&s.fx_page==0u);

    /* HOME WAVE MULTI must genuinely expose two editable oscillators. */
    mi_init(&s);
    mi_rotate(&s,MI_EN_K1,4);
    assert(s.view==MI_HOME && s.wave==MI_WAVE_MULTI);
    assert(s.osc2_level==64u && s.osc1_wave==0u);
    assert(!strcmp(mi_param_name(&s,0),"WAVE"));
    assert(!strcmp(mi_param_name(&s,1),"MODE"));
    assert(!strcmp(mi_param_name(&s,2),"DETUNE"));
    mi_rotate(&s,MI_EN_K2,1);  /* MODE -> OSC2 */
    assert(s.osc_page==1u);
    mi_rotate(&s,MI_EN_K3,10); /* HOME WET now edits DETUNE */
    assert(s.osc2_detune==74u&&mi_param_value(&s,2)==10);
    mi_button(&s,MI_B_EDIT,9300);
    assert(s.view==MI_OSC&&s.osc_page==1u);
    mi_rotate(&s,MI_EN_K1,1); /* OSC2 wave only */
    assert(s.osc2_wave==1u&&s.osc1_wave==0u&&s.wave==MI_WAVE_MULTI);
    mi_button(&s,MI_B_HOME,9301);
    mi_rotate(&s,MI_EN_K2,-1); /* MODE -> OSC1 */
    mi_button(&s,MI_B_EDIT,9302);
    assert(s.osc_page==0u);
    mi_rotate(&s,MI_EN_K1,2);
    assert(s.osc1_wave==2u&&s.osc2_wave==1u&&s.wave==MI_WAVE_MULTI);
    assert(mi_state_valid(&s));
    s.osc1_wave=4u;
    assert(!mi_state_valid(&s));

    /* Validate real saved-state bounds: corrupted storage must never reach
     * UI lookup tables, oscillator routing or REC IN/OUT arithmetic. */
    mi_init(&s);
    assert(mi_state_valid(&s));
    s.seq[0]=128u; assert(!mi_state_valid(&s)); s.seq[0]=0u;
    s.seq[1]=128u; assert(!mi_state_valid(&s)); s.seq[1]=127u;
    s.seq[0]=127u; assert(!mi_state_valid(&s)); s.seq[0]=0u;
    s.seq[2]=7u; assert(!mi_state_valid(&s)); s.seq[2]=3u;
    s.seq[3]=128u; assert(!mi_state_valid(&s)); s.seq[3]=100u;
    s.env[2]=255u; assert(!mi_state_valid(&s)); s.env[2]=115u;
    s.lfo[1]=4u; assert(!mi_state_valid(&s)); s.lfo[1]=0u;
    s.lfo[3]=3u; assert(!mi_state_valid(&s)); s.lfo[3]=0u;
    s.arp[0]=5u; assert(!mi_state_valid(&s)); s.arp[0]=0u;
    s.arp[2]=9u; assert(!mi_state_valid(&s)); s.arp[2]=70u;
    s.arp[3]=0u; assert(!mi_state_valid(&s)); s.arp[3]=1u;
    s.global[0]=39; assert(!mi_state_valid(&s)); s.global[0]=90;
    s.global[1]=101; assert(!mi_state_valid(&s)); s.global[1]=0;
    s.global[2]=51; assert(!mi_state_valid(&s)); s.global[2]=0;
    s.global[3]=3; assert(!mi_state_valid(&s)); s.global[3]=0;
    s.fx_options[MI_FX_EUREKA][1]=128u;
    assert(!mi_state_valid(&s)); s.fx_options[MI_FX_EUREKA][1]=64u;
    s.fx=MI_FX_DELAY; s.fx2=MI_FX_EUREKA;
    assert(!mi_state_valid(&s)); s.fx=MI_FX_OFF; s.fx2=MI_FX_OFF;
    assert(mi_state_valid(&s));

    puts("PASS: REC, FX conflicts, EUREKA, controls, and saved-state corruption bounds");
    return 0;
}
