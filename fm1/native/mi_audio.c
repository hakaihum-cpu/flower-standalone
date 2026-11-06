/* SPDX-License-Identifier: GPL-3.0-only
 * Native FM-1 oscillator with initial Delay, Reverb and Chorus DSP.
 * Not all original MIYAKO FX are implemented or fidelity-tested.
 * EUREKA is not part of this branch, by the user's later instruction.
 */
#include "mi_audio.h"
#include "mi_fx_dsp.h"
#include "mi_memory.h"
static mi_fx_dsp_t native_fx;
static float clampf(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static float wrap01(float x){while(x>=1.f)x-=1.f;while(x<0.f)x+=1.f;return x;}
static float note_freq(int note)
{
    static const float ratio[12]={1.f,1.059463094f,1.122462048f,1.189207115f,
        1.25992105f,1.334839854f,1.414213562f,1.498307077f,1.587401052f,
        1.681792831f,1.781797436f,1.887748625f};
    int oct=0,step=note-69;
    while(step<0){step+=12;--oct;}
    while(step>=12){step-=12;++oct;}
    float f=440.f*ratio[step];
    while(oct>0){f*=2.f;--oct;}
    while(oct<0){f*=0.5f;++oct;}
    return f;
}
/* Piecewise cubic sine approximation; deterministic with no external math runtime. */
static float sin01(float phase)
{
    float x=wrap01(phase),q;
    int neg=x>=0.5f;
    if(neg)x-=0.5f;
    q=x*2.f;
    /* Parabolic sine with a smoothly rounded peak. */
    float y=4.f*q*(1.f-q);
    y=0.775f*y+0.225f*y*y;
    return neg?-y:y;
}
static float osc(uint8_t wave,float phase,float duty)
{
    switch(wave){
    case 0:return sin01(phase);
    case 1:return phase*2.f-1.f;
    case 2:return phase<0.5f?1.f:-1.f;
    default:return phase<duty?1.f:-1.f;
    }
}
void mi_audio_init(mi_audio_t *a,float rate)
{
    mi_memzero(a,sizeof(*a));
    a->sample_rate=rate>=8000.f&&rate<=192000.f?rate:44100.f;
    a->rand_state=0x4d495941u;
    mi_fx_dsp_init(&native_fx);
    a->last_mode=MI_FX_OFF;
}
void mi_audio_note_on(mi_audio_t *a,uint8_t note,uint8_t velocity)
{
    unsigned i,pick=0;float quiet=2.f;uint32_t oldest=0xffffffffu;
    for(i=0;i<MI_NATIVE_POLY;i++){
        mi_voice_t *v=&a->v[i];
        if(!v->active){pick=i;break;}
        if(!v->held){
            if(v->env<quiet){quiet=v->env;pick=i;}
        }else if(quiet>=2.f&&v->age<oldest){
            oldest=v->age;pick=i;
        }
    }
    mi_voice_t *v=&a->v[pick];
    v->note=note;v->vel=velocity?velocity:1;v->active=1;v->held=1;
    v->phase=0.f;v->subphase=0.f;v->increment=note_freq(note)/a->sample_rate;
    v->env=0.f;v->age=++a->age_counter;
}
void mi_audio_note_off(mi_audio_t *a,uint8_t note)
{
    unsigned i;
    for(i=0;i<MI_NATIVE_POLY;i++)
        if(a->v[i].active&&a->v[i].note==note)a->v[i].held=0;
}
void mi_audio_panic(mi_audio_t *a)
{
    unsigned i;for(i=0;i<MI_NATIVE_POLY;i++){a->v[i].active=0;a->v[i].held=0;a->v[i].env=0.f;}
}
static float effect(mi_audio_t *a,const mi_state_t *s,float dry)
{
    unsigned mode=s->fx;
    float target=mode==MI_FX_OFF?0.f:s->wet/127.f;
    float wet=dry;
    if(mode>=MI_FX_COUNT)mode=MI_FX_OFF;
    if(mode!=a->last_mode){
        a->wr=a->valid=a->phase=a->segment=0;
        a->wet_current=0.f;
        a->last_mode=(uint8_t)mode;
    }
    if(mode==MI_FX_REEL){
        float wow_depth=s->fx_options[MI_FX_REEL][0]/127.f;
        float flutter_depth=s->fx_options[MI_FX_REEL][1]/127.f;
        a->wow=wrap01(a->wow+0.4f/a->sample_rate);
        a->flutter=wrap01(a->flutter+6.8f/a->sample_rate);
        float d=256.f+90.f*wow_depth*sin01(a->wow)+21.f*flutter_depth*sin01(a->flutter);
        uint32_t whole=(uint32_t)clampf(d,64.f,512.f);
        float frac=d-(float)whole;
        if(a->valid>whole+1u){
            int16_t x=a->ring[(a->wr-whole)&(MI_NATIVE_RING-1u)];
            int16_t y=a->ring[(a->wr-whole-1u)&(MI_NATIVE_RING-1u)];
            wet=(x+(y-x)*frac)/32767.f;
            wet=wet/(1.f+0.23f*(wet<0.f?-wet:wet));
        }
        a->ring[a->wr&(MI_NATIVE_RING-1u)]=(int16_t)(clampf(dry,-1.f,1.f)*32767.f);
        ++a->wr;if(a->valid<MI_NATIVE_RING)++a->valid;
    }else if(mode==MI_FX_DELAY||mode==MI_FX_REVERB||mode==MI_FX_CHORUS){
        wet=mi_fx_dsp_process(&native_fx,mode,dry,a->sample_rate,
            s->fx_options[mode][0],s->fx_options[mode][1]);
    }else if(mode==MI_FX_DREAMY){
        /* Dreamy requires a separate approved, source-accurate implementation.
         * No false effect processing or reduced pitch-grain imitation. */
        target=0.f;
    }else target=0.f;
    a->wet_current+=(target-a->wet_current)*0.001f;
    return dry+(wet-dry)*a->wet_current;
}
void mi_audio_render(mi_audio_t *a,const mi_state_t *s,int32_t *lr,uint32_t n,uint32_t master_q12)
{
    uint32_t i,k;
    if(!a||!s||!lr)return;
    float scale=clampf(master_q12/4096.f,0.f,1.f)*clampf(s->level/127.f,0.f,1.f)*5000000.f;
    uint8_t wave=s->wave>3?0:s->wave;
    float duty=0.05f+0.9f*(s->width/127.f);
    for(i=0;i<n;i++){
        float sample=0.f;
        for(k=0;k<MI_NATIVE_POLY;k++){
            mi_voice_t *v=&a->v[k];
            if(!v->active)continue;
            float step=v->held?0.002f:-0.0005f;
            v->env=clampf(v->env+step,0.f,1.f);
            if(!v->held&&v->env==0.f){v->active=0;continue;}
            float main=osc(wave,v->phase,duty),sub=sin01(v->subphase);
            float submix=s->sub/127.f;
            float value=main*(1.f-submix)+sub*submix;
            if(s->crush){
                int bits=16-(int)(s->crush*13u/127u);
                float levels=(float)(1u<<bits);
                float quant=(float)(int)(value*levels);
                value=quant/levels;
            }
            sample+=value*v->env*(v->vel/127.f)*0.18f;
            v->phase=wrap01(v->phase+v->increment);
            v->subphase=wrap01(v->subphase+v->increment*0.5f);
        }
        sample=clampf(sample,-1.f,1.f);
        sample=effect(a,s,sample);
        int32_t out=(int32_t)(clampf(sample,-1.f,1.f)*scale);
        lr[2u*i]=out;lr[2u*i+1u]=out;
    }
}
