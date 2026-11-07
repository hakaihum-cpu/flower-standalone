/* SPDX-License-Identifier: GPL-3.0-only
 * 8kHz G.711 u-law (8-bit) recorder, maximum 4 bars internal BPM / 2 bars
 * external MIDI clock. Audio sample storage never touches flash in IRQ.
 */
#include "mi_recorder.h"
#include "mi_memory.h"
static uint8_t clip[MI_REC_CAPACITY] __attribute__((section(".pool")));
static int8_t wave_min[224],wave_max[224];
static float bounded(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static uint8_t encode_ulaw(float x)
{
    int pcm=(int)(bounded(x,-1.f,1.f)*32767.f);
    unsigned mag,exponent=7u,mask,encoded;
    unsigned sign=pcm<0?0x7fu:0xffu;
    if(pcm<0)pcm=-pcm;
    if(pcm>32635)pcm=32635;
    mag=(unsigned)pcm+132u;
    mask=0x4000u;
    while(exponent>0u && !(mag&mask)){
        mask>>=1u;--exponent;
    }
    encoded=(exponent<<4u)|((mag>>(exponent+3u))&15u);
    return (uint8_t)(encoded^sign);
}
static float decode_ulaw(uint8_t data)
{
    unsigned v=(unsigned)(uint8_t)(~data);
    int t=((int)(v&15u)<<3)+132;
    t<<=(v>>4u)&7u;
    return bounded((float)((v&0x80u)?132-t:t-132)*(1.f/32768.f),-1.f,1.f);
}
static int16_t decode16(uint8_t data)
{
    return (int16_t)(decode_ulaw(data)*32767.f);
}
void mi_rec_peak(unsigned column,int8_t *lo,int8_t *hi)
{
    if(lo)*lo=column<224u?wave_min[column]:0;
    if(hi)*hi=column<224u?wave_max[column]:0;
}
int16_t mi_rec_sample(uint32_t i)
{
    return i<MI_REC_CAPACITY?decode16(clip[i]):0;
}
void mi_rec_init(mi_recorder_t *r)
{
    if(!r)return;
    mi_memzero(r,sizeof(*r));
    r->source_rate=44100.f;
    r->bpm=120u;
    r->limit_count=MI_REC_CAPACITY;
}
void mi_rec_config(mi_recorder_t *r,float sample_rate,unsigned bpm)
{
    if(!r)return;
    if(sample_rate>=8000.f&&sample_rate<=192000.f)r->source_rate=sample_rate;
    if(bpm>=40u&&bpm<=240u)r->bpm=bpm;
}
static void finish(mi_recorder_t *r)
{
    r->recording=r->overdubbing=r->clock_wait=r->clock_pad=0u;
    r->clock_mode=0u;
    r->playing=r->count>=4u;
    r->playback_pos=0.f;
}
void mi_rec_stop(mi_recorder_t *r)
{
    if(!r)return;
    r->recording=r->playing=r->overdubbing=0u;
    r->clock_wait=r->clock_mode=r->clock_pad=0u;
}
void mi_rec_clear(mi_recorder_t *r)
{
    if(!r)return;
    r->count=r->write_pos=0u;r->playback_pos=0.f;
    r->accumulator=r->accumulated=0.f;
    r->samples_accumulated=0u;
    r->capture_phase=0u;
    r->recording=r->playing=r->overdubbing=0u;
    r->clock_wait=r->clock_mode=r->clock_pad=0u;r->clock_ticks=0u;
    mi_memzero(wave_min,sizeof wave_min);
    mi_memzero(wave_max,sizeof wave_max);
}
void mi_rec_midi_realtime(mi_recorder_t *r,uint8_t status)
{
    if(!r)return;
    if(status==0xfau){ /* START establishes MIDI bar origin */
        r->clock_origin=r->clock_transport=r->clock_present=1u;
        r->clock_phase=0u;r->clock_age=0u;
        return;
    }
    if(status==0xfbu){ /* CONTINUE: no known bar origin */
        r->clock_origin=0u;r->clock_transport=r->clock_present=1u;
        r->clock_age=0u;
        return;
    }
    if(status==0xfcu){
        r->clock_transport=0u;
        if(r->clock_mode){
            /* A stopped external transport cannot complete two bars.
             * Release REC so the unit does not remain armed forever. */
            if(r->recording||r->clock_pad)finish(r);
            else r->clock_wait=0u;
        }
        return;
    }
    if(status!=0xf8u)return;
    r->clock_present=1u;r->clock_age=0u;
    /* F8-only controllers (without FA) are supported as free phase clocks. */
    if(r->clock_origin&&!r->clock_transport)return;
    r->clock_phase=(r->clock_phase+1u)%MI_REC_BAR_TICKS;
    if(r->clock_wait){
        if(!r->clock_origin||r->clock_phase==0u){
            r->clock_wait=0u;r->recording=1u;r->clock_ticks=0u;
            r->accumulator=r->accumulated=0.f;
            r->samples_accumulated=0u;
        }
        return;
    }
    if(r->clock_mode&&(r->recording||r->clock_pad)){
        ++r->clock_ticks;
        if(r->clock_ticks>=MI_REC_CLOCK_TICKS)finish(r);
    }
}
void mi_rec_toggle_record(mi_recorder_t *r)
{
    uint32_t max_count;
    if(!r)return;
    if(r->clock_wait){r->clock_wait=r->clock_mode=0u;return;}
    if(r->recording){
        if(r->clock_mode){r->clock_pad=1u;r->recording=0u;return;}
        finish(r);return;
    }
    if(r->clock_pad)return; /* retain the fixed 2-bar clock endpoint */
    if(r->count>=4u){
        r->overdubbing=1u;r->playing=1u;
        r->write_pos=0u;r->playback_pos=0.f;
    }else{
        mi_rec_clear(r);
        r->overdubbing=0u;
    }
    r->accumulator=r->accumulated=0.f;
    r->samples_accumulated=0u;
    /* 4/4: 4 bars = 16 quarters at the user's internal BPM. */
    max_count=(MI_REC_RATE*960u+(r->bpm/2u))/r->bpm;
    if(max_count>MI_REC_CAPACITY)max_count=MI_REC_CAPACITY;
    r->limit_count=max_count;
    if(r->clock_present){
        r->clock_mode=r->clock_wait=1u;
        r->clock_pad=r->recording=0u;
        r->clock_ticks=0u;
    }else r->recording=1u;
}
void mi_rec_toggle_play(mi_recorder_t *r)
{
    if(!r)return;
    if(r->clock_wait)return;
    if(r->recording){
        if(r->clock_mode){r->clock_pad=1u;r->recording=0u;return;}
        finish(r);return;
    }
    if(r->count<4u){r->playing=0u;return;}
    if(r->playing){r->playing=0u;return;}
    r->playback_pos=0.f;r->playing=1u;
}
float mi_rec_tick(mi_recorder_t *r,float input,const uint8_t c[4])
{
    static const float speed[7]={0.5f,0.667419927f,0.749153538f,
                                 1.f,1.334839854f,1.498307077f,2.f};
    uint32_t first,last,i,next,fade;
    float pos,fraction,out=0.f,level;
    if(!r||!c)return 0.f;
    if(r->clock_present){
        ++r->clock_age;
        if((float)r->clock_age>r->source_rate*0.4f){
            r->clock_present=0u;
            if(r->clock_wait)r->clock_wait=r->clock_mode=0u;
            if(r->clock_mode&&(r->recording||r->clock_pad))finish(r);
        }
    }
    if(r->playing&&r->count>=4u){
        first=(r->count-1u)*c[0]/127u;
        last=(r->count-1u)*c[1]/127u+1u;
        if(last>r->count)last=r->count;
        if(first+2u>last)r->playing=0u;
        else{
            if(r->playback_pos<(float)first||r->playback_pos>=(float)last)
                r->playback_pos=(float)first;
            pos=r->playback_pos;i=(uint32_t)pos;
            next=i+1u<last?i+1u:first;
            fraction=pos-(float)i;
            out=decode_ulaw(clip[i])*(1.f-fraction)+
                decode_ulaw(clip[next])*fraction;
            fade=(last-first)/8u;
            if(fade>128u)fade=128u;
            if(fade>=2u&&pos>=(float)(last-fade)){
                float headpos=(float)first+pos-(float)(last-fade);
                uint32_t j=(uint32_t)headpos;
                float part=headpos-(float)j;
                float hd=decode_ulaw(clip[j])*(1.f-part)+
                         decode_ulaw(clip[j+1u])*part;
                float mix=(pos-(float)(last-fade))/(float)fade;
                out=out*(1.f-mix)+hd*mix;
            }
            r->playback_pos+=speed[c[2]<=6u?c[2]:3u]*
                             (float)MI_REC_RATE/r->source_rate;
            if(r->playback_pos>=(float)last){
                r->playback_pos=(float)(first+fade)+
                                (r->playback_pos-(float)last);
                if(r->playback_pos>=(float)last)
                    r->playback_pos=(float)(first+fade);
            }
        }
    }
    if(r->recording||r->clock_pad){
        r->accumulator+=(float)MI_REC_RATE;
        r->accumulated+=r->recording?input:0.f;
        ++r->samples_accumulated;
        if(r->accumulator>=r->source_rate){
            float v=bounded(r->accumulated/(float)r->samples_accumulated,-1.f,1.f);
            r->accumulator-=r->source_rate;
            r->accumulated=0.f;
            r->samples_accumulated=0u;
            if(r->write_pos<MI_REC_CAPACITY){
                uint32_t column;
                if(r->overdubbing&&r->count>=4u){
                    if(r->clock_pad)v=decode_ulaw(clip[r->write_pos]);
                    else v=bounded(decode_ulaw(clip[r->write_pos])+v,-1.f,1.f);
                }
                column=r->write_pos*224u/MI_REC_CAPACITY;
                if(column>=224u)column=223u;
                {
                    int32_t amp=(int32_t)(v*110.f);
                    if(amp>110)amp=110;
                    if(amp< -110)amp=-110;
                    if(amp<wave_min[column])wave_min[column]=(int8_t)amp;
                    if(amp>wave_max[column])wave_max[column]=(int8_t)amp;
                }
                clip[r->write_pos++]=encode_ulaw(v);
                if(!r->overdubbing)r->count=r->write_pos;
            }
            if(r->overdubbing){
                if(r->write_pos>=r->count)r->write_pos=0u;
            }else if(r->write_pos>=r->limit_count||
                     r->write_pos>=MI_REC_CAPACITY){
                finish(r);
            }
        }
    }
    level=bounded((float)c[3]/127.f,0.f,1.f);
    return bounded(out*level,-1.f,1.f);
}
