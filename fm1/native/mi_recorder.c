/* SPDX-License-Identifier: GPL-3.0-only
 * One bounded clip, no heap allocation, disk writes or audio-thread flash IO.
 * 44.1 kHz native output is pair-averaged to 22.05 kHz 16-bit PCM.
 */
#include "mi_recorder.h"
#include "mi_memory.h"
static int16_t clip[MI_REC_CAPACITY] __attribute__((section(".pool")));
static int8_t wave_min[224],wave_max[224];
void mi_rec_peak(unsigned column,int8_t *lo,int8_t *hi)
{
    if(lo)*lo=column<224u?wave_min[column]:0;
    if(hi)*hi=column<224u?wave_max[column]:0;
}
static float clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
int16_t mi_rec_sample(uint32_t i){return i<MI_REC_CAPACITY?clip[i]:0;}
void mi_rec_init(mi_recorder_t *r){if(r)mi_memzero(r,sizeof(*r));}
void mi_rec_stop(mi_recorder_t *r)
{
    if(!r)return;
    r->recording=0u;r->playing=0u;
}
void mi_rec_toggle_record(mi_recorder_t *r)
{
    if(!r)return;
    if(r->recording){r->recording=0u;return;}
    r->recording=1u;r->playing=0u;
    r->count=0u;r->write_pos=0u;r->capture_phase=0u;
    r->accumulator=0.f;r->playback_pos=0.f;
    mi_memzero(wave_min,sizeof wave_min);
    mi_memzero(wave_max,sizeof wave_max);
}
void mi_rec_toggle_play(mi_recorder_t *r)
{
    if(!r)return;
    if(r->recording)r->recording=0u;
    if(r->count<4u){r->playing=0u;return;}
    if(r->playing){r->playing=0u;return;}
    r->playback_pos=0.f;r->playing=1u;
}
float mi_rec_tick(mi_recorder_t *r,float internal_audio,const uint8_t controls[4])
{
    static const float speed[7]={0.5f,0.667419927f,0.749153538f,
                                 1.f,1.334839854f,1.498307077f,2.f};
    uint32_t first,last,i,next;
    float pos,fraction,out=0.f,ratio,level;
    if(!r||!controls)return 0.f;
    if(r->recording){
        if(!r->capture_phase){
            r->accumulator=internal_audio;
            r->capture_phase=1u;
        }else{
            float v=clamp((r->accumulator+internal_audio)*0.5f,-1.f,1.f);
            if(r->write_pos<MI_REC_CAPACITY){
                uint32_t column=r->write_pos*224u/MI_REC_CAPACITY;
                int32_t amp=(int32_t)(v*110.f);
                if(amp>110)amp=110;
                if(amp< -110)amp=-110;
                if(amp<wave_min[column])wave_min[column]=(int8_t)amp;
                if(amp>wave_max[column])wave_max[column]=(int8_t)amp;
                clip[r->write_pos++]=(int16_t)(v*32767.f);
                r->count=r->write_pos;
            }
            r->capture_phase=0u;
            if(r->write_pos>=MI_REC_CAPACITY)r->recording=0u;
        }
    }
    if(!r->playing||r->count<4u)return 0.f;
    first=(r->count-1u)*controls[0]/127u;
    last=(r->count-1u)*controls[1]/127u+1u;
    if(last>r->count)last=r->count;
    if(first+2u>last){r->playing=0u;return 0.f;}
    if(r->playback_pos<(float)first||r->playback_pos>=(float)last)
        r->playback_pos=(float)first;
    pos=r->playback_pos;
    i=(uint32_t)pos;
    next=i+1u<last?i+1u:first;
    fraction=pos-(float)i;
    out=((float)clip[i]+((float)clip[next]-(float)clip[i])*fraction)*(1.f/32767.f);
    ratio=speed[controls[2]<=6u?controls[2]:3u];
    /* 22.05k sampled clip plays into 44.1k output, preserving 0-semitone time. */
    r->playback_pos+=ratio*0.5f;
    if(r->playback_pos>=(float)last){
        r->playback_pos=(float)first+(r->playback_pos-(float)last);
        if(r->playback_pos>=(float)last)
            r->playback_pos=(float)first;
    }
    level=clamp((float)controls[3]/127.f,0.f,1.f);
    return clamp(out*level,-1.f,1.f);
}
