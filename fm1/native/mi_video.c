/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO source-video scheduler for 144 EFFECTS and 576 EUREKA frames.
 * Both freeze on silence; no autonomous playback.
 */
#include "mi_video.h"
#include "mi_memory.h"
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
    mi_memzero(v,sizeof(*v));
    v->effects_rng=0x7E670103u;
    v->eureka_rng=0x2EC0A7E1u;
    v->eureka_frame=-1;
    v->effect_count=effect_count;
    v->effects_frame=effect_count?0:-1;
}
void mi_video_set_eureka(mi_video_t *v,uint16_t count)
{
    if(!v)return;
    /* Exact complete clips only: avoid random modulo by zero. */
    v->eureka_count=(count>=36u&&count%36u==0u)?count:0u;
    v->eureka_frame=v->eureka_count?0:-1;
}
void mi_video_tick(mi_video_t *v,float activity,int eureka_mode)
{
    int attack;
    if(!v)return;
    if(activity<0.f)activity=0.f;
    if(activity>1.f)activity=1.f;
    if(eureka_mode){
        if(!v->eureka_count)return;
        if(v->eureka_cooldown)--v->eureka_cooldown;
        attack=activity>v->effects_previous*1.28f+0.0015f;
        if(activity>0.003f&&(attack||!v->eureka_cooldown)){
            int frame=v->eureka_frame<0?0:v->eureka_frame;
            if(attack){
                unsigned clip=next_rng(&v->eureka_rng)%(v->eureka_count/36u);
                frame=(int)(clip*36u);
            }else{
                unsigned current=(unsigned)frame;
                frame=(int)((current+1u)%v->eureka_count);
            }
            v->eureka_frame=(int16_t)frame;
            v->eureka_cooldown=activity>0.14f?2u:4u;
        }
        v->effects_previous=activity;
        return;
    }
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
