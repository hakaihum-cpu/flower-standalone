/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO FM-1 fixed 44,100-sample recorder; duration follows tempo.
 * No heap, flash writes or PCM buffer clear in the audio IRQ.
 */
#include "mi_recorder.h"
#include "mi_memory.h"
static int16_t clip[MI_REC_CAPACITY] __attribute__((section(".pool")));
static int8_t wave_min[224],wave_max[224];
static float clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
int16_t mi_rec_sample(uint32_t sample){return sample<MI_REC_CAPACITY?clip[sample]:0;}
void mi_rec_peak(unsigned column,int8_t *lo,int8_t *hi)
{
    if(lo)*lo=column<224u?wave_min[column]:0;
    if(hi)*hi=column<224u?wave_max[column]:0;
}
void mi_rec_init(mi_recorder_t *r)
{
    if(!r)return;
    mi_memzero(r,sizeof(*r));
    r->bars=2u;
    r->capture_step=0.5f; /* legacy standalone host calls: 22.05kHz */
    r->play_step=0.5f;
}
static void finish(mi_recorder_t *r)
{
    r->recording=0u;r->overdubbing=0u;r->armed=0u;
    if(r->count>=4u){r->playing=1u;r->playback_pos=0.f;}
}
void mi_rec_stop(mi_recorder_t *r)
{
    if(!r)return;
    r->armed=0u;r->recording=0u;r->playing=0u;r->overdubbing=0u;
}
void mi_rec_clear(mi_recorder_t *r)
{
    if(!r)return;
    r->count=0u;r->write_pos=0u;r->playback_pos=0.f;
    r->accumulator=0.f;r->capture_phase=0u;
    r->capture_sum=0.f;r->capture_samples=0u;
    r->recording=0u;r->playing=0u;r->overdubbing=0u;r->armed=0u;
    mi_memzero(wave_min,sizeof wave_min);
    mi_memzero(wave_max,sizeof wave_max);
}
void mi_rec_configure(mi_recorder_t *r,unsigned bpm,unsigned bars,
                      float sample_rate,uint32_t sample_clock)
{
    float step;
    if(!r)return;
    if(sample_rate<8000.f)sample_rate=44100.f;
    if(bpm<40u)bpm=40u;
    if(bpm>240u)bpm=240u;
    if(r->clock_active && (uint32_t)(sample_clock-r->clock_last_sample)>
       (uint32_t)sample_rate){r->clock_active=0u;r->clock_period=0.f;}
    /* Do not change recorded duration mid-take because a UI parameter moved.
     * External clock takes precedence and is always exactly two bars. */
    if(r->clock_active)r->bars=2u;
    else if(!r->recording&&!r->playing&&!r->armed)
        r->bars=(uint8_t)(bars==4u?4u:2u);
    /* 44,100 PCM points span 2 or 4 bars, even at 40 BPM.
     * 4 bars therefore trades quality for duration, NOT extra RAM. */
    step=(float)MI_REC_CAPACITY*(float)bpm/
         (r->bars*4.f*60.f*sample_rate);
    if(r->clock_active&&r->clock_period>1.f)
        step=(float)MI_REC_CAPACITY/(192.f*r->clock_period);
    if(step>0.95f)step=0.95f;
    if(step<0.001f)step=0.001f;
    if((!r->recording && !r->playing) ||
       (r->recording && !r->overdubbing))r->capture_step=step;
    if(!r->playing&&!r->recording&&!r->armed)r->play_step=step;
    /* Once locked to a MIDI master, playback can follow tempo drift
     * without re-recording or adding storage. */
    if(r->clock_active && r->clock_period>1.f &&
       r->playing&&!r->recording&&r->count>=4u){
        r->play_step=(float)r->count/(192.f*r->clock_period);
    }
    /* Lost-clock fallback never leaves REC armed indefinitely. */
    if(r->armed&&!r->clock_active){
        r->armed=0u;r->recording=1u;r->write_pos=0u;
        r->capture_sum=0.f;r->capture_samples=0u;r->accumulator=0.f;
        if(r->count>=4u)r->overdubbing=1u;
        else{mi_rec_clear(r);r->recording=1u;}
    }
}
void mi_rec_external_tick(mi_recorder_t *r,uint32_t sample_clock)
{
    if(!r)return;
    if(r->clock_active){
        uint32_t interval=(uint32_t)(sample_clock-r->clock_last_sample);
        if(interval>100u&&interval<22050u){
            if(r->clock_period<=1.f)r->clock_period=(float)interval;
            else r->clock_period=r->clock_period*0.8f+0.2f*(float)interval;
        }
    }
    r->clock_last_sample=sample_clock;
    r->clock_active=1u;
    ++r->clock_ticks;
    /* Start at the NEXT 1/16-note MIDI boundary (6 clocks), not the
     * next full bar (96 clocks). REC should respond within a subdivision
     * while still recording exactly 192 external ticks (two bars).
     * The first clock after MIDI Start is the initial downbeat. */
    if(r->armed && ((r->clock_ticks-1u)%6u)==0u){
        uint8_t overdub=r->count>=4u;
        if(!overdub)mi_rec_clear(r);
        r->armed=0u;r->recording=1u;r->overdubbing=overdub;
        r->playing=overdub;
        r->write_pos=0u;r->playback_pos=0.f;
        r->capture_sum=0.f;r->capture_samples=0u;r->accumulator=0.f;
        r->clock_start_tick=r->clock_ticks;
        if(overdub)r->capture_step=r->play_step;
        else if(r->clock_period>1.f)
            r->capture_step=clamp((float)MI_REC_CAPACITY/
                                  (192.f*r->clock_period),0.001f,0.95f);
    }else if(r->recording &&
             (uint32_t)(r->clock_ticks-r->clock_start_tick)>=192u){
        finish(r);
    }
}
void mi_rec_clock_stop(mi_recorder_t *r)
{
    if(!r)return;
    r->clock_active=0u;r->clock_period=0.f;r->armed=0u;
    if(r->recording)finish(r);
}
void mi_rec_toggle_record(mi_recorder_t *r)
{
    uint8_t clock;
    if(!r)return;
    if(r->armed){r->armed=0u;return;}
    if(r->recording){finish(r);return;}
    if(r->clock_active){r->armed=1u;return;}
    clock=r->clock_active;
    if(r->count>=4u){
        r->overdubbing=1u;r->recording=1u;r->playing=1u;
        r->write_pos=0u;r->playback_pos=0.f;
        r->capture_step=r->play_step;
        r->capture_sum=0.f;r->capture_samples=0u;r->accumulator=0.f;
        return;
    }
    mi_rec_clear(r);
    r->clock_active=clock;
    r->recording=1u;
}
void mi_rec_toggle_play(mi_recorder_t *r)
{
    if(!r)return;
    if(r->recording){r->recording=0u;r->overdubbing=0u;}
    r->armed=0u;
    if(r->count<4u){r->playing=0u;return;}
    if(r->playing){r->playing=0u;return;}
    r->playback_pos=0.f;r->playing=1u;
}
float mi_rec_tick(mi_recorder_t *r,float input,const uint8_t controls[4])
{
    static const float speed[7]={0.5f,0.667419927f,0.749153538f,
                                 1.f,1.334839854f,1.498307077f,2.f};
    uint32_t first,last,i,next,fade;
    float pos,fraction,out=0.f,ratio,level;
    if(!r||!controls)return 0.f;
    if(r->playing&&r->count>=4u){
        first=(r->count-1u)*controls[0]/127u;
        last=(r->count-1u)*controls[1]/127u+1u;
        if(last>r->count)last=r->count;
        if(first+2u>last)r->playing=0u;
        else{
            if(r->playback_pos<(float)first||r->playback_pos>=(float)last)
                r->playback_pos=(float)first;
            pos=r->playback_pos;i=(uint32_t)pos;
            next=i+1u<last?i+1u:first;
            fraction=pos-(float)i;
            out=((float)clip[i]+((float)clip[next]-(float)clip[i])*fraction)*
                (1.f/32767.f);
            fade=(last-first)/8u;
            if(fade>128u)fade=128u;
            if(fade>=2u&&pos>=(float)(last-fade)){
                float headpos=(float)first+pos-(float)(last-fade);
                uint32_t j=(uint32_t)headpos;
                float part=headpos-(float)j;
                float head=((float)clip[j]+
                            ((float)clip[j+1u]-(float)clip[j])*part)*
                           (1.f/32767.f);
                float mix=(pos-(float)(last-fade))/(float)fade;
                out=out*(1.f-mix)+head*mix;
            }
            ratio=speed[controls[2]<=6u?controls[2]:3u];
            r->playback_pos+=ratio*r->play_step;
            if(r->playback_pos>=(float)last){
                r->playback_pos=(float)(first+fade)+
                                (r->playback_pos-(float)last);
                if(r->playback_pos>=(float)last)
                    r->playback_pos=(float)(first+fade);
            }
        }
    }
    if(r->recording){
        r->capture_sum+=input;
        if(r->capture_samples<65535u)++r->capture_samples;
        r->accumulator+=r->capture_step;
        if(r->accumulator>=1.f){
            float v=clamp(r->capture_sum/(float)r->capture_samples,-1.f,1.f);
            r->capture_sum=0.f;r->capture_samples=0u;
            r->accumulator-=1.f;
            if(r->write_pos<MI_REC_CAPACITY){
                uint32_t column=r->write_pos*224u/MI_REC_CAPACITY;
                int32_t amp;
                if(r->overdubbing&&r->count>=4u)
                    v=clamp((float)clip[r->write_pos]*(1.f/32767.f)+v,-1.f,1.f);
                amp=(int32_t)(v*110.f);
                if(amp>110)amp=110;
                if(amp< -110)amp=-110;
                if(amp<wave_min[column])wave_min[column]=(int8_t)amp;
                if(amp>wave_max[column])wave_max[column]=(int8_t)amp;
                clip[r->write_pos++]=(int16_t)(v*32767.f);
                if(!r->overdubbing)r->count=r->write_pos;
            }
            if(r->overdubbing){
                if(r->write_pos>=r->count)r->write_pos=0u;
            }else if(r->write_pos>=MI_REC_CAPACITY){
                /* Under clock, hold to the exact 192-pulse bar boundary. */
                r->play_step=r->capture_step;
                if(!r->clock_active)finish(r);
            }
        }
    }
    if(r->recording&&!r->overdubbing)r->play_step=r->capture_step;
    level=clamp((float)controls[3]/127.f,0.f,1.f);
    return clamp(out*level,-1.f,1.f);
}
