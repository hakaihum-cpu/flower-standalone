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
    v->effects_rng=0x45464658u; /* MIYAKO original EFFECTS seed */
    v->eureka_rng=0x4555524bu;
    v->eureka_clip=-1; /* first source clip selected only on first tick */
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
    v->eureka_clip=-1;
    v->eureka_playhead=0.f;
    v->eureka_previous=0.f;
}
/* Retain original 36-frame clip ordering and attack-driven clip jumps.
 * Advance within the clip on each audible video tick to avoid a permanent
 * freeze with steady held notes; silence still freezes the last frame. */
static void choose_eureka_clip(mi_video_t *v)
{
    unsigned clips=(unsigned)v->eureka_count/36u;
    unsigned next=next_rng(&v->eureka_rng)%clips;
    if(clips>1u && (int)next==(int)v->eureka_clip)
        next=(next+1u)%clips;
    v->eureka_clip=(int16_t)next;
    v->eureka_playhead=0.f;
}
void mi_video_tick(mi_video_t *v,float activity,int eureka_mode)
{
    int attack;
    if(!v)return;
    if(activity<0.f)activity=0.f;
    if(activity>1.f)activity=1.f;
    if(eureka_mode){
        float previous,change,advance;
        int audio_present,onset;
        unsigned index;
        if(!v->eureka_count)return;
        previous=v->eureka_previous;
        audio_present=activity>0.006f;
        onset=audio_present && activity>0.008f &&
              activity>previous*1.45f+0.003f;
        change=activity>previous?activity-previous:previous-activity;
        v->eureka_previous=activity;
        if(v->eureka_clip<0)choose_eureka_clip(v);
        if(audio_present){
            /* Preserve selective random clip jumps on note attacks.
             * A held tone must still advance frames within its clip. */
            if(onset && (v->eureka_rng%5u)==0u)
                choose_eureka_clip(v);
            else{
                advance=1.f+change*65.f+activity*2.f;
                if(advance<1.f)advance=1.f;
                if(advance>4.f)advance=4.f;
                v->eureka_playhead+=advance;
                if(v->eureka_playhead>=36.f)choose_eureka_clip(v);
            }
        }
        index=(unsigned)v->eureka_clip*36u+
              (unsigned)v->eureka_playhead;
        if(index>=(unsigned)v->eureka_count)
            index=(unsigned)v->eureka_count-1u;
        v->eureka_frame=(int16_t)index;
        return;
    }
    /* Separate EFFECTS' source activity history from EUREKA history. */
    v->eureka_previous=0.f;
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
