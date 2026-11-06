/* SPDX-License-Identifier: GPL-3.0-only
 * Original MIYAKO EFFECTS video scheduler only. Audio-reactive frame
 * selection from original 144 user-provided frames; no EUREKA.
 */
#include "mi_video.h"
#include <string.h>
static uint32_t next_rng(uint32_t *v)
{
    *v ^= *v << 13;
    *v ^= *v >> 17;
    *v ^= *v << 5;
    return *v;
}
void mi_video_init(mi_video_t *v,uint16_t effect_count)
{
    if(!v)return;
    memset(v,0,sizeof(*v));
    v->effects_rng=0x7E670103u;
    v->effect_count=effect_count;
    v->effects_frame=effect_count?0:-1;
}
void mi_video_tick(mi_video_t *v,float activity,int unused)
{
    int attack;
    (void)unused;
    if(!v)return;
    if(activity<0.f)activity=0.f;
    if(activity>1.f)activity=1.f;
    if(!v->effect_count)return;
    if(v->effects_cooldown)--v->effects_cooldown;
    attack=activity>v->effects_previous*1.28f+0.0015f;
    if(activity>0.003f&&(attack||!v->effects_cooldown)){
        uint32_t rnd=next_rng(&v->effects_rng);
        int next=(int)(rnd%v->effect_count);
        if(v->effect_count>1&&next==v->effects_frame)next=(next+1)%v->effect_count;
        v->effects_frame=(int16_t)next;
        v->effects_cooldown=4;
        int ticks=2+(int)(activity*6.f+0.5f);
        if(ticks<2)ticks=2;
        if(ticks>8)ticks=8;
        v->effects_hold_ticks=(uint8_t)ticks;
    }
    v->effects_previous=activity;
}
