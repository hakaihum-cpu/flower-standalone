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
    mi_button(&s, MI_B_EDIT, 0);
    mi_rotate(&s, MI_EN_K1, 3);
    assert(s.view == MI_OSC && s.wave == 3);
    mi_button(&s, MI_B_FX, 0);
    mi_rotate(&s, MI_EN_K1, 2);
    assert(s.fx == MI_FX_EUREKA);
    assert(!strcmp(mi_param_name(&s,2),"SIZE"));
    assert(!strcmp(mi_param_name(&s,3),"REPS"));
    assert(mi_param_value(&s,2) == 64 && mi_param_value(&s,3) == 80);
    mi_button(&s, MI_B_HOME, 0);
    mi_rotate(&s, MI_EN_PRESETS, 1);
    assert(s.preset == 1 && s.wave == 1);
    mi_rotate(&s, MI_EN_PRESETS, 4096);
    assert(s.preset == 2); /* physical detent: one preset per event */
    for(i=0;i<4;++i) mi_rotate(&s, MI_EN_PRESETS, 1);
    assert(s.preset == 5 && s.fx == MI_FX_EUREKA);
    s.wet=0;
    mi_button(&s, MI_B_SEL, 12);
    assert(s.view == MI_VIDEO && s.wet == 0 && s.fx == MI_FX_EUREKA);
    mi_button(&s, MI_B_FX, 13);
    mi_rotate(&s, MI_EN_K1, 1);
    assert(s.fx == MI_FX_DREAMY);
    mi_button(&s, MI_B_SEL, 14);
    assert(s.view == MI_VIDEO && s.fx == MI_FX_DREAMY && s.wet == 0);
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
    for(i=0;i<5;i++) mi_button(&s, MI_B_OCT_DOWN, 7001);
    assert(s.octave == -3);
    assert(MI_FX_COUNT == 7 && MI_VIEW_COUNT == 10);
    puts("PASS: independent MIYAKO control mapping and guarded save");
    return 0;
}
