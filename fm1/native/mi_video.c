/* SPDX-License-Identifier: GPL-3.0-only
 * Faithful control flow of MIYAKO VisualizerComponent::setAudioActivity
 * feature/miyako-ep-sampler-16part-20261030 (2026-11-06 inspection).
 * This is frame-index selection only: original media packing and rendering
 * are REQUIRED separately. No generated images.
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
static void choose_eureka(mi_video_t *v)
{
    int32_t clip=(int32_t)(next_rng(&v->eureka_rng)%16u);
    if(clip==v->eureka_clip)clip=(clip+1)%16;
    v->eureka_clip=(int16_t)clip;
    v->eureka_playhead=0.f;
    v->eureka_frame=(int16_t)(clip*36);
}
void mi_video_init(mi_video_t *v,uint16_t effect_count)
{
    memset(v,0,sizeof(*v));
    v->effects_rng=0x7E670103u;
    v->eureka_rng=0xA18CE431u;
    v->effect_count=effect_count;
    v->effects_frame=-1;
    v->eureka_frame=-1;
    v->eureka_clip=-1;
}
void mi_video_tick(mi_video_t *v,float activity,int eureka_enabled)
{
    float change;
    int attack;
    if(!v)return;
    if(activity<0.f)activity=0.f;
    if(activity>1.f)activity=1.f;
    if(eureka_enabled){
        float prior=v->eureka_previous;
        int present=activity>0.006f;
        int onset=present&&activity>0.008f&&activity>prior*1.45f+0.003f;
        change=activity>prior?activity-prior:prior-activity;
        v->eureka_previous=activity;
        if(v->eureka_clip<0)choose_eureka(v);
        if(present&&(onset||change>0.0025f)){
            if(onset&&(v->eureka_rng%5u)==0u){
                choose_eureka(v);
            }else{
                float step=1.f+change*65.f+activity*2.f;
                if(step<1.f)step=1.f;
                if(step>4.f)step=4.f;
                v->eureka_playhead+=step;
                if(v->eureka_playhead>=36.f)choose_eureka(v);
            }
        }
        v->eureka_frame=(int16_t)(v->eureka_clip*36+(int)v->eureka_playhead);
    }else{
        v->eureka_previous=0.f;
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
        if(ticks<2)ticks=2;if(ticks>8)ticks=8;
        v->effects_hold_ticks=(uint8_t)ticks;
    }
    v->effects_previous=activity;
}
