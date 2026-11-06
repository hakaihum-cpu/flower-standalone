/* SPDX-License-Identifier: GPL-3.0-only
 * FM-1 native, finite-memory effects. The original MIYAKO parameter and
 * multi-FX semantics still require fidelity tests; EUREKA is out of scope.
 */
#include "mi_fx_dsp.h"
#include "mi_control.h"
#include "mi_memory.h"
#define MI_DELAY_N 32768u
#define MI_CHORUS_N 2048u
#define MI_REVERB_N 4096u
#define MI_DREAM_N 32768u
#define MI_EUREKA_FRAMES 29400u /* exactly 2 s at 14.7 kHz */
static int16_t delay_memory[MI_DELAY_N] __attribute__((section(".pool")));
static int16_t chorus_memory[MI_CHORUS_N] __attribute__((section(".pool")));
static int16_t reverb_memory[4][MI_REVERB_N] __attribute__((section(".pool")));
static int16_t dreamy_memory[MI_DREAM_N] __attribute__((section(".pool")));
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
void mi_fx_dsp_reset_shared(mi_fx_dsp_t *p)
{
    if(!p)return;
    /* O(1): never clear 64 KiB in the audio interrupt. Validity guards
     * prevent stale samples from DELAY/EUREKA being played back. */
    p->delay_write=0u;p->delay_valid=0u;
    p->eureka_write=0u;p->eureka_valid=0u;
    p->eureka_decimate=0u;
    p->eureka_sum=0.f;p->eureka_phase=0.f;p->eureka_filter=0.f;
}
static float eu_read(const mi_fx_dsp_t *p,float behind)
{
    float at=(float)p->eureka_write-behind;
    while(at<0.f)at+=(float)MI_EUREKA_FRAMES;
    while(at>=(float)MI_EUREKA_FRAMES)at-=(float)MI_EUREKA_FRAMES;
    uint32_t i=(uint32_t)at;
    uint32_t j=i+1u<MI_EUREKA_FRAMES?i+1u:0u;
    float f=at-(float)i;
    return read16(delay_memory[i])*(1.f-f)+read16(delay_memory[j])*f;
}
void mi_fx_dsp_init(mi_fx_dsp_t *p)
{
    if(!p)return;
    mi_memzero(p,sizeof(*p));
    mi_fx_dsp_reset_shared(p);
    mi_memzero(delay_memory,sizeof delay_memory);
    mi_memzero(chorus_memory,sizeof chorus_memory);
    mi_memzero(reverb_memory,sizeof reverb_memory);
    mi_memzero(dreamy_memory,sizeof dreamy_memory);
}
float mi_fx_dsp_process(mi_fx_dsp_t *p,unsigned mode,float dry,
                        float sample_rate,unsigned p1,unsigned p2)
{
    if(!p)return dry;
    if(sample_rate<8000.f)sample_rate=44100.f;
    if(p1>127u)p1=127u;
    if(p2>127u)p2=127u;
    dry=bounded(dry,-1.f,1.f);
    if(mode==MI_FX_EUREKA) {
        /* Two-second short-buffer Haze adaptation. 14.7 kHz 16-bit
         * continuous capture; two crossfaded repeat grains with TIME
         * and HAZE. Wet only. Uses DELAY memory; cannot run with DELAY. */
        float haze=(float)p2/127.f;
        float grain=0.05f+0.24f*haze;
        float history=0.10f+1.60f*(float)p1/127.f;
        float grain_samples=grain*14700.f;
        float base=history*14700.f;
        float ph0=p->eureka_phase,ph1=ph0+0.5f;
        float wet=0.f,weights=0.f;
        unsigned k;
        if(ph1>=1.f)ph1-=1.f;
        for(k=0;k<2u;k++){
            float ph=k==0u?ph0:ph1;
            float behind=base+ph*grain_samples*0.8f;
            float triangle=ph<0.5f?ph*2.f:(1.f-ph)*2.f;
            if((float)p->eureka_valid>behind+2.f){
                wet+=eu_read(p,behind)*triangle;
                weights+=triangle;
            }
        }
        if(weights>0.01f)wet/=weights;
        else wet=0.f;
        /* Gentle haze filter; never introduce an unprocessed wet bleed. */
        p->eureka_filter+=(wet-p->eureka_filter)*(0.4f-0.3f*haze);
        wet=bounded(p->eureka_filter,-1.f,1.f);
        p->eureka_phase+=1.f/(grain*sample_rate);
        if(p->eureka_phase>=1.f)p->eureka_phase-=1.f;
        p->eureka_sum+=dry;
        if(++p->eureka_decimate==3u){
            delay_memory[p->eureka_write]=quant(p->eureka_sum*(1.f/3.f));
            p->eureka_sum=0.f;
            p->eureka_decimate=0u;
            if(++p->eureka_write==MI_EUREKA_FRAMES)p->eureka_write=0u;
            if(p->eureka_valid<MI_EUREKA_FRAMES)++p->eureka_valid;
        }
        return wet;
    }
    if(mode==MI_FX_DREAMY) {
        /* Two staggered, overlap-crossfaded grains from a bounded
         * 16-bit mono history. Parameters: GRAIN length / SPACE feedback.
         * Native FM-1 implementation; not a bit-exact JUCE Dreamy model.
         * At most 64 KiB additional pool memory; no heap or libc. */
        float size=0.08f+0.44f*(p1/127.f);
        float grain_samples=size*sample_rate;
        float ph0=p->dreamy_phase;
        float ph1=ph0+0.5f;
        float samples[2],windows[2];
        uint32_t write=p->dreamy_write&(MI_DREAM_N-1u);
        unsigned i;
        if(ph1>=1.f)ph1-=1.f;
        for(i=0;i<2u;i++){
            float phase=i==0u?ph0:ph1;
            float offset=2048.f+phase*grain_samples*0.5f;
            unsigned delay=(unsigned)offset;
            float fract=offset-(float)delay;
            unsigned at=(p->dreamy_write-delay)&(MI_DREAM_N-1u);
            float a=read16(dreamy_memory[at]);
            float b=read16(dreamy_memory[(at-1u)&(MI_DREAM_N-1u)]);
            samples[i]=a+(b-a)*fract;
            windows[i]=1.f-(phase<0.5f?1.f-2.f*phase:2.f*phase-1.f);
        }
        float denom=windows[0]+windows[1];
        float wet=(samples[0]*windows[0]+samples[1]*windows[1])/
                  (denom>0.01f?denom:1.f);
        if(p->dreamy_valid<MI_DREAM_N)++p->dreamy_valid;
        if(p->dreamy_valid<(uint32_t)(grain_samples+2048.f))wet=0.f;
        float fb=0.42f*(p2/127.f);
        dreamy_memory[write]=quant(dry+wet*fb);
        ++p->dreamy_write;
        p->dreamy_phase+=1.f/grain_samples;
        if(p->dreamy_phase>=1.f)p->dreamy_phase-=1.f;
        return bounded(wet,-1.f,1.f);
    }
    if(mode==MI_FX_DELAY) {
        /* 40..680 ms; one tap; 0..0.85 feedback */
        float ms=40.f+640.f*(p1/127.f);
        uint32_t n=(uint32_t)(sample_rate*ms/1000.f);
        if(n<2u)n=2u;
        if(n>=MI_DELAY_N)n=MI_DELAY_N-1u;
        uint32_t pos=p->delay_write&(MI_DELAY_N-1u);
        float delayed=p->delay_valid>n?
            read16(delay_memory[(p->delay_write-n)&(MI_DELAY_N-1u)]):0.f;
        float feedback=0.85f*(p2/127.f);
        delay_memory[pos]=quant(dry+delayed*feedback);
        ++p->delay_write;
        if(p->delay_valid<MI_DELAY_N)++p->delay_valid;
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
