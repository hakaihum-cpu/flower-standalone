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
    assert(!strcmp(mi_param_name(&s,0),"MODE"));
    assert(!strcmp(mi_param_name(&s,2),"GRAIN"));
    assert(mi_param_value(&s,2)==64 && mi_param_value(&s,3)==80);
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
    assert(!strcmp(mi_param_name(&s,0),"GLITCH"));
    mi_rotate(&s, MI_EN_K1, 60); /* SEL cannot silently change current FX */
    assert(s.video_params[0]==120u && s.fx == MI_FX_CHORUS);
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
    assert(s.seq[0]==48&&s.seq[1]==87&&s.seq[2]==6);
    mi_rotate(&s,MI_EN_K2,-500);
    assert(s.seq[1]==49);
    for(i=0;i<5;i++) mi_button(&s, MI_B_OCT_DOWN, 7001);
    assert(s.octave == -3);
    assert(MI_FX_COUNT == 9 && MI_VIEW_COUNT == 10);
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
    assert(s.lfo[1]==3 && s.lfo[3]==20);
    assert(!strcmp(mi_param_name(&s,2),"DEPTH"));
    mi_button(&s,MI_B_ENV,8002);
    assert(!strcmp(mi_param_name(&s,0),"ATK"));
    assert(!strcmp(mi_param_name(&s,3),"REL"));
    /* ADSR: four points/detent, 32 detents cover the full 0..127 range.
     * Other continuous parameters use two; enum selectors stay one. */
    mi_rotate(&s,MI_EN_K1,3);
    mi_rotate(&s,MI_EN_K2,-2);
    mi_rotate(&s,MI_EN_K3,3);
    mi_rotate(&s,MI_EN_K4,2);
    assert(s.env[0]==16u&&s.env[1]==57u&&
           s.env[2]==127u&&s.env[3]==53u);
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

    /* OSC page 0 keeps PULSE WIDTH, page 2 exposes real FILTER.
     * OSC2 editing is accessible only as part of MULTI. */
    mi_button(&s,MI_B_EDIT,9010);
    assert(s.view==MI_OSC&&s.osc_page==0u);
    mi_rotate(&s,MI_EN_K1,-50);
    assert(s.wave==0u);
    mi_button(&s,MI_B_EDIT,9011);
    assert(s.osc_page==2u&&!strcmp(mi_param_name(&s,0),"CUTOFF"));
    mi_rotate(&s,MI_EN_K1,-20);
    mi_rotate(&s,MI_EN_K2,10);
    assert(s.filter_cutoff==87u&&s.filter_resonance==20u);
    mi_button(&s,MI_B_EDIT,9012);
    assert(s.osc_page==0u&&!strcmp(mi_param_name(&s,1),"WIDTH"));
    mi_button(&s,MI_B_HOME,9013);
    mi_rotate(&s,MI_EN_K1,4);
    assert(s.wave==MI_WAVE_MULTI);
    mi_button(&s,MI_B_EDIT,9014);
    mi_button(&s,MI_B_EDIT,9015);
    assert(s.osc_page==1u&&!strcmp(mi_param_name(&s,0),"WAVE2"));
    mi_rotate(&s,MI_EN_K1,1);
    mi_rotate(&s,MI_EN_K2,10);
    mi_rotate(&s,MI_EN_K3,70);
    mi_rotate(&s,MI_EN_K4,1);
    assert(s.osc2_wave==1u&&s.osc2_detune==84u&&
           s.osc2_level==127u&&s.osc2_octave==3u);
    mi_button(&s,MI_B_EDIT,9016);
    assert(s.osc_page==2u);
    mi_button(&s,MI_B_EDIT,9017);
    assert(s.osc_page==0u);
    mi_init(&s);
    assert(s.osc2_level==0u&&s.osc2_octave==2u&&s.osc2_detune==64u);
    assert(mi_state_valid(&s));
    s.osc2_wave=4u;assert(!mi_state_valid(&s));s.osc2_wave=0u;
    s.osc2_octave=5u;assert(!mi_state_valid(&s));s.osc2_octave=2u;
    s.osc_page=3u;assert(!mi_state_valid(&s));s.osc_page=0u;
    s.osc2_level=255u;assert(!mi_state_valid(&s));s.osc2_level=0u;
    s.filter_cutoff=200u;assert(!mi_state_valid(&s));s.filter_cutoff=127u;
    assert(mi_state_valid(&s));
    /* The FM-1 FX page always enters P1 (MODE/WET), including EUREKA and
     * DREAMY. PRESET detents explicitly toggle P2 and back; K1 must
     * never unexpectedly change PITCH after selecting an effect. */
    mi_init(&s);
    mi_button(&s,MI_B_FX,9200);
    mi_rotate(&s,MI_EN_K1,2);
    assert(s.fx==MI_FX_DREAMY && s.fx_page==0u);
    assert(!strcmp(mi_param_name(&s,0),"MODE"));
    assert(!strcmp(mi_param_name(&s,2),"GRAIN"));
    mi_rotate(&s,MI_EN_PRESETS,1);
    assert(s.fx_page==1u && s.fx==MI_FX_DREAMY);
    assert(!strcmp(mi_param_name(&s,0),"PITCH"));
    assert(!strcmp(mi_param_name(&s,1),"REVERSE"));
    mi_rotate(&s,MI_EN_K1,12);
    mi_rotate(&s,MI_EN_K2,127);
    mi_rotate(&s,MI_EN_K3,20);
    mi_rotate(&s,MI_EN_K4,10);
    assert(mi_param_value(&s,0)==4 && mi_param_value(&s,1)==1);
    assert(s.fx_extra[MI_FX_DREAMY][2]==104u);
    assert(s.fx_extra[MI_FX_DREAMY][3]==84u);
    assert(s.fx_options[MI_FX_DREAMY][0]==64u);
    assert(s.fx_options[MI_FX_DREAMY][1]==80u);
    mi_rotate(&s,MI_EN_PRESETS,-1);
    assert(s.fx_page==0u);
    mi_rotate(&s,MI_EN_K1,4);
    assert(s.fx==MI_FX_EUREKA && s.fx_page==0u);
    mi_rotate(&s,MI_EN_PRESETS,1);
    assert(s.fx_page==1u && !strcmp(mi_param_name(&s,0),"PITCH"));
    mi_rotate(&s,MI_EN_PRESETS,1);
    assert(s.fx_page==0u && !strcmp(mi_param_name(&s,2),"TIME"));
    mi_rotate(&s,MI_EN_SELECT,1);
    assert(s.view==MI_VIDEO&&s.fx==MI_FX_EUREKA);
    mi_button(&s,MI_B_FX,92005);
    mi_rotate(&s,MI_EN_SELECT,2);
    assert(s.view==MI_ENV); /* multiple detents traverse multiple pages */
    mi_button(&s,MI_B_FX,92006);
    mi_button(&s,MI_B_FX,9201);
    assert(s.fx_slot==1u&&s.fx_page==0u);

    /* Repeated single hardware detents can cross DREAMY to EUREKA
     * without an undocumented preliminary page switch. */
    mi_init(&s);
    mi_button(&s,MI_B_FX,9240);
    for(i=0;i<6u;i++)mi_rotate(&s,MI_EN_K1,1);
    assert(s.fx==MI_FX_EUREKA && s.fx_page==0u);
    mi_rotate(&s,MI_EN_PRESETS,1);
    assert(s.fx_page==1u);
    mi_button(&s,MI_B_SEL,9241);
    mi_button(&s,MI_B_FX,9242);
    assert(s.fx_page==0u && s.fx_slot==0u);

    /* EUREKA + REVERB A/B selection still skips the DELAY memory clash. */
    mi_init(&s);
    s.fx=MI_FX_REEL;
    mi_button(&s,MI_B_FX,9250);
    for(i=0;i<5u;i++)mi_rotate(&s,MI_EN_K1,1);
    assert(s.fx==MI_FX_EUREKA && s.fx_page==0u);
    mi_button(&s,MI_B_SEL,9251);
    mi_button(&s,MI_B_FX,9252);
    mi_button(&s,MI_B_FX,9253);
    assert(s.fx_slot==1u && s.fx_page==0u);
    mi_rotate(&s,MI_EN_K1,2);
    assert(s.fx2==MI_FX_DREAMY && s.fx_page==0u);
    mi_rotate(&s,MI_EN_K1,1);
    assert(s.fx==MI_FX_EUREKA && s.fx2==MI_FX_REVERB);
    assert(mi_fx_pair_supported(s.fx,s.fx2));

    /* HOME K1/K2 independently edit actual MULTI OSC1/OSC2 waveforms,
     * K3 edits MIX2; none of these controls secretly modifies FX. */
    mi_init(&s);
    assert(!strcmp(mi_param_name(&s,1),"SUB"));
    assert(!strcmp(mi_param_name(&s,2),"CRUSH"));
    mi_rotate(&s,MI_EN_K2,12);
    mi_rotate(&s,MI_EN_K3,6);
    assert(s.sub==24u&&s.crush==12u&&s.fx==MI_FX_OFF);
    mi_rotate(&s,MI_EN_K1,4);
    assert(s.wave==MI_WAVE_MULTI&&s.osc2_level==64u);
    assert(!strcmp(mi_param_name(&s,0),"OSC1"));
    assert(!strcmp(mi_param_name(&s,1),"OSC2"));
    assert(!strcmp(mi_param_name(&s,2),"MIX2"));
    mi_rotate(&s,MI_EN_K1,2);
    mi_rotate(&s,MI_EN_K2,2);
    assert(s.wave==MI_WAVE_MULTI&&s.osc1_wave==2u&&s.osc2_wave==2u);
    assert(mi_param_value(&s,0)==2&&mi_param_value(&s,1)==2);
    mi_rotate(&s,MI_EN_K3,10);
    assert(s.osc2_level==84u&&mi_param_value(&s,2)==84);
    mi_button(&s,MI_B_EDIT,9300);
    assert(s.view==MI_OSC&&s.osc_page==0u);
    mi_rotate(&s,MI_EN_K1,1);
    assert(s.osc1_wave==3u&&s.osc2_wave==2u);
    mi_button(&s,MI_B_EDIT,9301);
    assert(s.osc_page==1u);
    mi_rotate(&s,MI_EN_K1,1);
    assert(s.osc1_wave==3u&&s.osc2_wave==3u);
    mi_button(&s,MI_B_EDIT,9302);
    assert(s.osc_page==2u&&!strcmp(mi_param_name(&s,1),"RES"));
    mi_button(&s,MI_B_EDIT,9303);
    assert(s.osc_page==0u);
    mi_rotate(&s,MI_EN_K1,-1);
    assert(s.osc1_wave==2u&&s.osc2_wave==3u);
    mi_button(&s,MI_B_HOME,9303);
    mi_rotate(&s,MI_EN_ALGORITHM,-1);
    assert(s.wave==3u&&mi_state_valid(&s));
    mi_rotate(&s,MI_EN_K1,2); /* PULSE -> MULTI -> FMDRUM */
    assert(s.wave==MI_WAVE_FMDRUM);
    assert(!strcmp(mi_wave_name(s.wave),"FMDRUM"));
    mi_button(&s,MI_B_EDIT,9304);
    assert(!strcmp(mi_param_name(&s,0),"TONE"));
    mi_rotate(&s,MI_EN_K1,10);
    assert(s.drum_tone==84u);
    mi_button(&s,MI_B_EDIT,9305);
    assert(!strcmp(mi_param_name(&s,0),"CUTOFF"));
    mi_init(&s);
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
    s.lfo[3]=58u; assert(!mi_state_valid(&s)); s.lfo[3]=0u;
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

    /* New FX, LFO, 4-bar and SEL control regressions. */
    mi_init(&s);
    assert(!strcmp(mi_fx_name(MI_FX_BITCRUSH),"BITCRUSH"));
    assert(!strcmp(mi_fx_name(MI_FX_PITCHSHIFT),"PITCHSHIFT"));
    assert(!mi_fx_pair_supported(MI_FX_DREAMY,MI_FX_PITCHSHIFT));
    mi_button(&s,MI_B_FX,9500);
    mi_rotate(&s,MI_EN_K1,7);
    assert(s.fx==MI_FX_BITCRUSH);
    assert(!strcmp(mi_param_name(&s,2),"BITS"));
    mi_rotate(&s,MI_EN_K1,1);
    assert(s.fx==MI_FX_PITCHSHIFT);
    assert(!strcmp(mi_param_name(&s,2),"PITCH"));
    mi_button(&s,MI_B_LFO,9501);
    mi_rotate(&s,MI_EN_K4,57);
    assert(s.lfo[3]==57u);
    assert(!strcmp(mi_lfo_dest_name(57u),"REC BARS"));
    mi_button(&s,MI_B_SEL,9502);
    mi_rotate(&s,MI_EN_K1,50);
    assert(s.video_params[0]==100u);
    mi_rotate(&s,MI_EN_K2,20);
    assert(s.video_params[1]==40u);
    mi_button(&s,MI_B_SEQ,9503);
    assert(s.rec_bars==2u);
    mi_rotate(&s,MI_EN_PRESETS,1);
    assert(s.rec_bars==4u);
    mi_rotate(&s,MI_EN_PRESETS,-1);
    assert(s.rec_bars==2u);
    assert(mi_state_valid(&s));
    /* Short SEQ remains the RECORDER; the long-hold editor has independent
     * discrete controls and all 16 step positions without touching REC. */
    mi_init(&s);
    mi_button(&s,MI_B_SEQ,9700u);
    assert(s.view==MI_SEQ && !s.step_editor && s.step_len==8u);
    s.step_editor=1u;
    assert(!strcmp(mi_param_name(&s,0),"TRACK"));
    assert(!strcmp(mi_param_name(&s,3),"STEPS"));
    mi_rotate(&s,MI_EN_K1,1);
    mi_rotate(&s,MI_EN_K2,1);
    mi_rotate(&s,MI_EN_K3,2);
    mi_rotate(&s,MI_EN_K4,8);
    assert(s.step_track==1u && s.step_play==1u &&
           s.step_dir==2u && s.step_len==16u);
    mi_rotate(&s,MI_EN_K2,-1);
    mi_rotate(&s,MI_EN_K4,-100);
    assert(s.step_play==0u && s.step_len==1u);
    s.step_pattern[0]=0x8001u;
    assert(mi_state_valid(&s));
    s.step_len=17u;assert(!mi_state_valid(&s));s.step_len=8u;
    s.step_track=4u;assert(!mi_state_valid(&s));s.step_track=1u;
    s.step_dir=3u;assert(!mi_state_valid(&s));s.step_dir=2u;
    s.step_editor=2u;assert(!mi_state_valid(&s));s.step_editor=1u;
    mi_button(&s,MI_B_HOME,9701u);
    assert(s.view==MI_HOME && s.step_editor==0u);
    mi_button(&s,MI_B_SEQ,9702u);
    assert(s.view==MI_SEQ && s.step_editor==0u);
    puts("PASS: REC, FMDrum, OSC/FILTER, new FX, SEL, 4x16 sequencer and state bounds");
    return 0;
}
