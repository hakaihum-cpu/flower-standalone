/* SPDX-License-Identifier: GPL-3.0-only
 * FM-1 native, finite-memory effects. The original MIYAKO parameter and
 * multi-FX semantics still require fidelity tests; EUREKA is out of scope.
 */
#include "mi_fx_dsp.h"
#include "mi_control.h"
#include <string.h>
#define MI_DELAY_N 32768u
#define MI_CHORUS_N 2048u
#define MI_REVERB_N 4096u
static int16_t delay_memory[MI_DELAY_N] __attribute__((section(".pool")));
static int16_t chorus_memory[MI_CHORUS_N] __attribute__((section(".pool")));
static int16_t reverb_memory[4][MI_REVERB_N] __attribute__((section(".pool")));
static float bounded(float v,float lo,float hi) {return v<lo?lo:v>hi?hi:v;}
static int16_t quant(float v){return (int16_t)(bounded(v,-1.f,1.f)*32767.f);}
static float read16(int16_t v){return (float)v*(1.f/32767.f);}
static float sine(float x)
{
    x-= (int)x;
    if(x<0.f)x+=1.f;
    int negative=x>=0.5f;
    if(negative)x-=0.5f;
    float y=x*2.f;
    float p=4.f*y*(1.f-y);
    p=0.775f*p+0.225f*p*p;
    return negative?-p:p;
}
void mi_fx_dsp_init(mi_fx_dsp_t *p)
{
    if(!p)return;
    memset(p,0,sizeof(*p));
    memset(delay_memory,0,sizeof delay_memory);
    memset(chorus_memory,0,sizeof chorus_memory);
    memset(reverb_memory,0,sizeof reverb_memory);
}
float mi_fx_dsp_process(mi_fx_dsp_t *p,unsigned mode,float dry,
                        float sample_rate,unsigned p1,unsigned p2)
{
    if(!p)return dry;
    if(sample_rate<8000.f)sample_rate=44100.f;
    if(p1>127u)p1=127u;
    if(p2>127u)p2=127u;
    dry=bounded(dry,-1.f,1.f);
    if(mode==MI_FX_DELAY) {
        /* 40..680 ms; one tap; 0..0.85 feedback */
        float ms=40.f+640.f*(p1/127.f);
        uint32_t n=(uint32_t)(sample_rate*ms/1000.f);
        if(n<2u)n=2u;
        if(n>=MI_DELAY_N)n=MI_DELAY_N-1u;
        uint32_t pos=p->delay_write&(MI_DELAY_N-1u);
        float delayed=read16(delay_memory[(p->delay_write-n)&(MI_DELAY_N-1u)]);
        float feedback=0.85f*(p2/127.f);
        delay_memory[pos]=quant(dry+delayed*feedback);
        ++p->delay_write;
        return bounded(delayed,-1.f,1.f);
    }
    if(mode==MI_FX_CHORUS) {
        float speed=0.08f+5.0f*(p1/127.f);
        float depth=2.f+18.f*(p2/127.f);
        p->chorus_phase+=speed/sample_rate;
        if(p->chorus_phase>=1.f)p->chorus_phase-=1.f;
        float distance=(10.f+depth*(0.5f+0.5f*sine(p->chorus_phase)))*sample_rate*0.001f;
        if(distance>(float)MI_CHORUS_N-2.f)distance=(float)MI_CHORUS_N-2.f;
        uint32_t back=(uint32_t)distance;
        float fraction=distance-(float)back;
        float a=read16(chorus_memory[(p->chorus_write-back)&(MI_CHORUS_N-1u)]);
        float b=read16(chorus_memory[(p->chorus_write-back-1u)&(MI_CHORUS_N-1u)]);
        float out=a+(b-a)*fraction;
        chorus_memory[p->chorus_write&(MI_CHORUS_N-1u)]=quant(dry);
        ++p->chorus_write;
        return bounded(out,-1.f,1.f);
    }
    if(mode==MI_FX_REVERB) {
        static const unsigned dlen[4]={1423u,2017u,2789u,3911u};
        float sum=0.f,feedback=0.45f+0.44f*(p1/127.f);
        float damping=0.04f+0.85f*(p2/127.f);
        unsigned i;
        for(i=0;i<4u;i++) {
            uint32_t at=p->reverb_write[i]&(MI_REVERB_N-1u);
            float value=read16(reverb_memory[i][(p->reverb_write[i]-dlen[i])&(MI_REVERB_N-1u)]);
            p->reverb_damp[i]+=(value-p->reverb_damp[i])*(1.f-damping);
            reverb_memory[i][at]=quant(dry*0.35f+p->reverb_damp[i]*feedback);
            ++p->reverb_write[i];
            sum+=value*0.25f;
        }
        return bounded(sum,-1.f,1.f);
    }
    return dry;
}
