/* SPDX-License-Identifier: GPL-2.0-or-later
 * Post-master 48kHz, 32-frame Q1.31 DSP; no allocation and no callbacks to UI.
 * REEL adapts the parameters and topology of MIYAKO MasterFx; original
 * float/JUCE DSP is not binary identical. CRUSH is standalone quantizer/hold.
 * The unstable EUREKA audio processor has been completely removed.
 * CPU/time and instrument testing required before SYX release.
 */
#include "rack.h"
#include <stdint.h>
#define SAMPLE_HZ 48000u
#define REEL_LEN 4096u
#define REEL_MASK (REEL_LEN-1u)
static const int16_t wave[256] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 
    9512, 10278, 11039, 11793, 12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594, 23170, 23731, 24279, 24811, 
    25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 
    32609, 32678, 32728, 32757, 32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 
    32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571, 30273, 29956, 29621, 29268, 
    28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731, 
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 
    15446, 14732, 14010, 13279, 12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 
    6393, 5602, 4808, 4011, 3212, 2410, 1608, 804, 0, -804, -1608, -2410, 
    -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793, 
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, 
    -20787, -21403, -22005, -22594, -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, 
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956, -30273, -30571, -30852, -31113, 
    -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757, 
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, 
    -31356, -31113, -30852, -30571, -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, 
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731, -23170, -22594, -22005, -21403, 
    -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279, 
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, 
    -3212, -2410, -1608, -804
};
static uint32_t noise_seed=0x4555524bu, reel_wow, reel_flutter;
static uint32_t reel_head;
static int16_t reel_history[2][REEL_LEN];
static int32_t reel_low[2], reel_high[2];
static uint32_t crush_counter;
static int16_t crush_hold[2];
static uint32_t rng(void)
{
    noise_seed ^= noise_seed<<13;
    noise_seed ^= noise_seed>>17;
    noise_seed ^= noise_seed<<5;
    return noise_seed;
}
static int32_t clamp16(int32_t v)
{
    return v>32767?32767:v< -32768?-32768:v;
}
static int32_t add_sat(int32_t a,int32_t b)
{
    return clamp16(a+b);
}
/* Real-time rule: all divisions for tape saturation run on UI/init context.
 * The audio IRQ only interpolates between 16-bit precomputed samples.
 * Reel has double-buffered 257-point curves updated off the audio IRQ.
 */
static int16_t reel_soft_table[2][257];
static volatile uint8_t reel_soft_index=0u;
static uint8_t reel_soft_drive=255u;
static uint8_t reel_wow_prepared=255u,reel_flutter_prepared=255u;
static uint32_t reel_wow_depth_q8,reel_flutter_depth_q8;

static int32_t tape_lookup(int32_t value,const int16_t *table)
{
    uint32_t absval=(uint32_t)(value<0?-value:value);
    uint32_t index,frac;
    int32_t y;
    if(absval>32767u) absval=32767u;
    index=absval>>7;
    frac=absval&127u;
    y=(int32_t)table[index]+
        (((int32_t)table[index+1u]-(int32_t)table[index])*(int32_t)frac>>7);
    return value<0?-y:y;
}
void rack_audio_prepare_reel(uint8_t drive)
{
    uint32_t drive256;
    uint8_t next;
    unsigned i;
    uint8_t wow=rack_state[DNFX_REEL].value[0][1];
    uint8_t flutter=rack_state[DNFX_REEL].value[0][2];
    if(wow!=reel_wow_prepared) {
        reel_wow_depth_q8=49152u*(uint32_t)wow/127u;
        reel_wow_prepared=wow;
    }
    if(flutter!=reel_flutter_prepared) {
        reel_flutter_depth_q8=12288u*(uint32_t)flutter/127u;
        reel_flutter_prepared=flutter;
    }
    if(drive>127u) drive=127u;
    if(reel_soft_drive==drive) return;
    drive256=256u+(1536u*(uint32_t)drive)/127u;
    next=(uint8_t)(reel_soft_index^1u);
    for(i=0u;i<=256u;++i) {
        int32_t x=(int32_t)(i*128u);
        int32_t denom=256+(((int32_t)drive256-256)*x>>15);
        int32_t y=(x*(int32_t)drive256)/denom;
        reel_soft_table[next][i]=(int16_t)clamp16(y);
    }
    reel_soft_drive=drive;
    reel_soft_index=next; /* atomic 8-bit commit: table is complete */
}
static int32_t lerp16(int32_t a,int32_t b,uint32_t frac256)
{
    return a+((b-a)*(int32_t)frac256)/256;
}
static int32_t ring_read(int ch,uint32_t posq8)
{
    uint32_t i=(posq8>>8)&REEL_MASK,j=(i+1u)&REEL_MASK;
    return lerp16(reel_history[ch][i],reel_history[ch][j],posq8&255u);
}
static void reel_frame(int32_t *out,const int16_t dry[2],const struct dnfx_state *s)
{
    unsigned ch;
    uint32_t wow=(uint32_t)s->value[0][1],flutter=(uint32_t)s->value[0][2];
    uint32_t wet=(uint32_t)s->value[0][4];
    const int16_t *curve=reel_soft_table[reel_soft_index];
    /* 18ms head; per-knob modulation depths are computed in the UI.
     * 32-bit products fit: 49152*32767 < INT32_MAX. */
    int32_t d_0=(int32_t)(864*256);
    int32_t d_w=((int32_t)reel_wow_depth_q8*
                  (int32_t)wave[reel_wow>>24])>>15;
    int32_t d_f=((int32_t)reel_flutter_depth_q8*
                  (int32_t)wave[reel_flutter>>24])>>15;
    uint32_t delayq8=(uint32_t)(d_0+d_w+d_f);
    (void)wow;(void)flutter;
    reel_wow+=49213u;  /* 0.55 Hz, phase 2^32 at 48 kHz */
    reel_flutter+=715828u; /* 8 Hz */
    for(ch=0u;ch<2u;++ch) {
        uint32_t readq8=((reel_head+REEL_LEN)*256u-delayq8)&(REEL_LEN*256u-1u);
        int32_t read,filtered,soft,hiss,processed;
        reel_history[ch][reel_head]=dry[ch];
        read=ring_read((int)ch,readq8);
        reel_low[ch]+=((read-reel_low[ch])*25518)>>15; /* ~11.5k lowpass */
        reel_high[ch]+=((reel_low[ch]-reel_high[ch])*180)>>15; /* 42Hz hipass */
        filtered=reel_low[ch]-reel_high[ch];
        soft=tape_lookup(filtered,curve);
        hiss=((int32_t)((rng()>>16)&65535u)-32768)*
             (int32_t)s->value[0][3]/(127*77); /* <= ~1.3% */
        processed=add_sat(soft,hiss);
        out[ch]=(int32_t)clamp16(dry[ch]+
             ((processed-(int32_t)dry[ch])*(int32_t)wet)/127);
    }
    reel_head=(reel_head+1u)&REEL_MASK;
}
static void crush_frame(int32_t *out,const int16_t dry[2],const struct dnfx_state *s)
{
    uint32_t bits=(uint32_t)s->value[0][0];
    uint32_t rate=(uint32_t)s->value[0][1];
    int32_t mix=s->value[0][2],gain=s->value[0][3];
    int32_t step;
    unsigned ch;
    if(bits<4u) bits=4u;
    if(bits>16u) bits=16u;
    if(rate<1u) rate=1u;
    if(rate>32u) rate=32u;
    if(crush_counter==0u) {
        step=1<<(16-bits);
        for(ch=0u;ch<2u;++ch) {
            int32_t sample=dry[ch],quant;
            /* step=2^shift: rounded signed quantization, division-free */
            {
                unsigned shift=16u-bits;
                int32_t positive=sample<0?-sample:sample;
                int32_t rounded=((positive+step/2)>>shift)<<shift;
                quant=clamp16(sample<0?-rounded:rounded);
            }
            crush_hold[ch]=(int16_t)quant;
        }
    }
    ++crush_counter;
    if(crush_counter>=rate) crush_counter=0u;
    for(ch=0u;ch<2u;++ch) {
        int32_t v=dry[ch]+(((int32_t)crush_hold[ch]-(int32_t)dry[ch])*mix)/127;
        out[ch]=clamp16((v*gain)/100); /* OUT 100: exact unity */
    }
}
void rack_audio_reset(int mode)
{
    unsigned i,j;
    if(mode==DNFX_REEL) {
    } else if(mode==DNFX_REEL) {
        rack_audio_prepare_reel(rack_state[DNFX_REEL].value[0][0]);
        reel_head=reel_wow=reel_flutter=0u;
        for(i=0;i<2u;++i) {
            reel_low[i]=reel_high[i]=0;
            for(j=0;j<REEL_LEN;++j) reel_history[i][j]=0;
        }
    } else if(mode==DNFX_CRUSH) {
        crush_counter=0u;crush_hold[0]=crush_hold[1]=0;
    }
}
void rack_audio_render(int32_t *out,const int32_t *in,int mode)
{
    unsigned n,ch;
    const struct dnfx_state *s=&rack_state[(unsigned)mode];
    for(n=0u;n<32u;++n) {
        int16_t dry[2];
        int32_t wet[2]={0,0};
        for(ch=0u;ch<2u;++ch)
            dry[ch]=(int16_t)(in[n*2u+ch]>>16);
        if(mode==DNFX_REEL) reel_frame(wet,dry,s);
        else if(mode==DNFX_CRUSH) crush_frame(wet,dry,s);
        else {wet[0]=dry[0];wet[1]=dry[1];}
        for(ch=0u;ch<2u;++ch) {
            /* Neutral REEL and CRUSH must preserve the complete
             * 24-bit Digitone master output, not discard eight bits. */
            if((mode==DNFX_REEL && s->value[0][4]==0u) ||
               (mode==DNFX_CRUSH && s->value[0][2]==0u &&
                s->value[0][3]==100u))
                out[n*2u+ch]=in[n*2u+ch];
            else out[n*2u+ch]=wet[ch]*65536;
        }
    }
}
