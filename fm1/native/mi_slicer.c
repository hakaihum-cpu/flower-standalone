/* SPDX-License-Identifier: GPL-3.0-only
 * Bounded MIDI-tempo-derived stereo random retrigger. No allocation.
 * Does NOT share DELAY/EUREKA/DREAMY buffers or alter recorder capture.
 */
#include "mi_slicer.h"
#include "mi_memory.h"
static int16_t history[MI_SLICER_FRAMES][2]
               __attribute__((section(".pool")));
static float bound(float x,float lo,float hi)
{
    return x<lo?lo:x>hi?hi:x;
}
static uint32_t random32(uint32_t *r)
{
    *r^=*r<<13;*r^=*r>>17;*r^=*r<<5;
    return *r;
}
void mi_slicer_init(mi_slicer_t *s)
{
    if(!s)return;
    mi_memzero(s,sizeof(*s));
    s->rng=0x534c4943u;
}
void mi_slicer_tick(mi_slicer_t *s,float in_l,float in_r,
                    float tempo_step,unsigned speed,unsigned randomness,
                    unsigned active,float *wet_l,float *wet_r)
{
    unsigned divisor,span;
    float out_l=in_l,out_r=in_r;
    if(wet_l)*wet_l=in_l;
    if(wet_r)*wet_r=in_r;
    if(!s||!wet_l||!wet_r)return;
    if(!active){
        s->active=0u;
        s->valid=0u;
        s->phase=0u;
        return;
    }
    if(!s->active){
        s->active=1u;
        s->write=0u;s->valid=0u;s->phase=0u;
        s->holding=0u;s->repeats=0u;
    }
    if(speed>127u)speed=127u;
    if(randomness>127u)randomness=127u;
    divisor=1u+(speed>>5u);
    if(tempo_step<32.f)tempo_step=32.f;
    span=(unsigned)(tempo_step/(float)divisor);
    if(span<32u)span=32u;
    /* One full slice must be entirely written before playback begins.
     * 2,048 points are about 46 ms at 44.1 kHz. */
    if(span>MI_SLICER_FRAMES/2u)span=MI_SLICER_FRAMES/2u;
    if(s->span!=span){
        s->span=span;
        s->phase=0u;
        s->holding=0u;s->repeats=0u;
    }
    if(s->phase==0u){
        if(s->holding&&s->repeats>0u)
            --s->repeats;
        else{
            s->holding=(uint8_t)(randomness>0u&&s->valid>=span&&
                               (random32(&s->rng)&127u)<randomness);
            s->repeats=s->holding?
                (uint8_t)(1u+(random32(&s->rng)%3u)):0u;
            s->source=(s->write-span)&(MI_SLICER_FRAMES-1u);
        }
    }
    if(s->holding && s->valid>=span){
        unsigned index=(s->source+s->phase)&(MI_SLICER_FRAMES-1u);
        float a=(float)history[index][0]*(1.f/32767.f);
        float b=(float)history[index][1]*(1.f/32767.f);
        /* Eight-sample edge fade towards dry input prevents harsh
         * discontinuities at re-trigger boundaries. */
        float edge=s->phase<8u?(float)s->phase/8.f:
                   (s->phase+8u>=span)?
                       (float)(span-1u-s->phase)/8.f:1.f;
        if(edge<0.f)edge=0.f;
        out_l=a*edge+in_l*(1.f-edge);
        out_r=b*edge+in_r*(1.f-edge);
    }
    history[s->write&(MI_SLICER_FRAMES-1u)][0]=
        (int16_t)(bound(in_l,-1.f,1.f)*32767.f);
    history[s->write&(MI_SLICER_FRAMES-1u)][1]=
        (int16_t)(bound(in_r,-1.f,1.f)*32767.f);
    ++s->write;
    if(s->valid<MI_SLICER_FRAMES)++s->valid;
    if(++s->phase>=span)s->phase=0u;
    *wet_l=bound(out_l,-1.f,1.f);
    *wet_r=bound(out_r,-1.f,1.f);
}
