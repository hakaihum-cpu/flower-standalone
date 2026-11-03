/* SPDX-License-Identifier: GPL-2.0-or-later
 * Post-master 48kHz, 32-frame Q1.31 DSP; no allocation and no callbacks to UI.
 * REEL adapts the parameters and topology of MIYAKO MasterFx; original
 * float/JUCE DSP is not binary identical. CRUSH is standalone quantizer/hold.
 * EUREKA ports the MIYAKO Haze controls with independent stereo 10.92s
 * capture. This is a source port, NOT yet validated for sonic parity.
 * CPU/time and instrument testing required before SYX release.
 */
#include "rack.h"
#include <stdint.h>
#define SAMPLE_HZ 48000u
#define REEL_LEN 4096u
#define REEL_MASK (REEL_LEN-1u)
#define EUREKA_LEN 524288u /* 10.92 seconds stereo int16, 2 MiB in bulk */
#define EUREKA_MASK (EUREKA_LEN-1u)
#define EUREKA_BULK 0x47900000u /* claim via mod.json resources.regions */
#define VERB_LEN 4096u
#define VERB_MASK (VERB_LEN-1u)
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
static uint32_t haze_write,haze_valid,haze_phase[2],haze_start[2];
static uint32_t haze_span[2],haze_target[2];
static int32_t haze_current_haze;
static uint8_t haze_primed[2];
static int32_t haze_filter[2],haze_verb_pos;
static int16_t haze_verb[2][VERB_LEN];
static int8_t haze_reverse[2];
static uint32_t haze_lfo;
#if defined(RACK_HOST_TEST)
static int16_t haze_host_memory[EUREKA_LEN*2u];
static int16_t *const haze_ring=haze_host_memory;
#else
static int16_t *const haze_ring=(int16_t *)(uintptr_t)EUREKA_BULK;
#endif
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
/* Normalized saturating transfer: no floating-point or 64-bit in callback. */
static int32_t tape_soft(int32_t x,uint32_t drive256)
{
    int32_t a=x<0?-x:x;
    int32_t y=(x*(int32_t)drive256)/
        (256+(((int32_t)drive256-256)*a>>15));
    return clamp16(y);
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
    uint32_t drive256=256u+(6u*256u*(uint32_t)s->value[0][0])/127u;
    /* At 48k: base head 18ms, +/-4ms WOW and +/-1ms flutter. */
    int32_t d_0=(int32_t)(864*256);
    int32_t d_w=(int32_t)((192u*256u*wow)/127u)*(int32_t)wave[reel_wow>>24]/32767;
    int32_t d_f=(int32_t)((48u*256u*flutter)/127u)*(int32_t)wave[reel_flutter>>24]/32767;
    uint32_t delayq8=(uint32_t)(d_0+d_w+d_f);
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
        soft=tape_soft(filtered,drive256);
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
    int32_t unit,step;
    unsigned ch;
    if(bits<4u) bits=4u;
    if(bits>16u) bits=16u;
    if(rate<1u) rate=1u;
    if(rate>32u) rate=32u;
    if(crush_counter==0u) {
        step=1<<(16-bits);
        for(ch=0u;ch<2u;++ch) {
            int32_t sample=dry[ch],quant;
            unit=sample>=0?(sample+step/2)/step:(sample-step/2)/step;
            quant=clamp16(unit*step);
            crush_hold[ch]=(int16_t)quant;
        }
    }
    crush_counter=(crush_counter+1u)%rate;
    for(ch=0u;ch<2u;++ch) {
        int32_t v=dry[ch]+(((int32_t)crush_hold[ch]-(int32_t)dry[ch])*mix)/127;
        out[ch]=clamp16((v*gain)/100); /* OUT 100: exact unity */
    }
}
/* MIYAKO Haze's TIME couples opposite stereo loop lengths. Compute once
 * per block (not every sample), independent of Mod Menu and animation.
 * The record history resides in the profile's uncached safe bulk region.
 */
static uint32_t haze_loop_frames(unsigned ch,uint32_t t)
{
    uint32_t n;
    n=ch==0u ? t : 127u-t; /* MIYAKO: opposite 10-second TIME halves */
    n=1200u+(n*478800u)/127u;
    return n>EUREKA_LEN-16u?EUREKA_LEN-16u:n;
}
static void haze_setup(const struct dnfx_state *s)
{
    unsigned ch;
    int32_t lfo=(int32_t)wave[haze_lfo>>24];
    int32_t mod=(int32_t)s->value[0][5]-64;
    int32_t depth=mod<0?-mod:mod;
    int32_t shift=(lfo*depth)/32767;
    int32_t t=s->value[0][1],r=s->value[0][4];
    haze_current_haze=(int32_t)s->value[0][2];
    /* MIYAKO Haze MOD: below noon affects time/repeat, above noon HAZE. */
    if(mod<0) {
        t+=shift/4;
        r+=shift/3;
    } else if(mod>0) {
        haze_current_haze+=shift/2;
    }
    if(t<0) t=0; if(t>127) t=127;
    if(r<0) r=0; if(r>127) r=127;
    if(haze_current_haze<0) haze_current_haze=0;
    if(haze_current_haze>127) haze_current_haze=127;
    for(ch=0u;ch<2u;++ch) {
        uint32_t loop=haze_loop_frames(ch,(uint32_t)t);
        uint32_t span=loop, min=(s->value[1][4]?24u:4320u);
        span=(span*(127u-(uint32_t)r))/127u;
        span=(span*(127u-(uint32_t)r))/127u;
        if(span<min) span=min;
        if(span>loop) span=loop;
        /* A new effect must make sound after 90ms of fresh input, not
         * wait 5 seconds for the full default TIME=5s. Start from a short
         * safe phrase and grow on successive loop boundaries. */
        if(haze_valid>96u && span>haze_valid-96u)
            span=haze_valid-96u;
        if(span<min) span=min;
        haze_target[ch]=span;
        if(!haze_span[ch]) {
            haze_span[ch]=min;
            haze_phase[ch]=0u;
        }
    }
}
static int32_t haze_read(unsigned ch,uint32_t off,uint32_t q8)
{
    uint32_t frame=(off+(q8>>8))&EUREKA_MASK;
    uint32_t frame2=(frame+1u)&EUREKA_MASK;
    uint32_t a=(uint32_t)haze_ring[frame*2u+ch];
    uint32_t b=(uint32_t)haze_ring[frame2*2u+ch];
    return lerp16((int16_t)a,(int16_t)b,q8&255u);
}
static int32_t haze_voice(unsigned ch,const struct dnfx_state *s,uint32_t speed)
{
    uint32_t span=haze_span[ch],phase=haze_phase[ch],spq8=span<<8;
    uint32_t seam= s->value[1][4] ? 12u : 768u;
    int32_t wet,more;
    if(span<24u || !haze_primed[ch] || haze_valid<span+64u) return 0;
    if(seam>span/4u) seam=span/4u;
    if(seam<1u) seam=1u;
    if(phase>=spq8) phase=0;
    wet=haze_read(ch,haze_start[ch],phase);
    if(!s->value[1][4] && phase>=(span-seam)*256u) {
        uint32_t ahead=(phase-(span-seam)*256u);
        uint32_t next=(haze_write+EUREKA_LEN-span-32u)&EUREKA_MASK;
        more=haze_read(ch,next,ahead);
        wet=lerp16(wet,more,ahead/(seam>0u?seam:1u));
    }
    if(haze_reverse[ch]) {
        if(phase<speed) phase=spq8-(speed-phase)%spq8;
        else phase-=speed;
    } else phase+=speed;
    if(phase>=spq8) {
        phase%=spq8;
        {
            uint32_t target=haze_target[ch],step=span/4u+1u;
            if(target>span) span+=(target-span>step?step:target-span);
            else if(target<span) span=target;
            haze_span[ch]=span;
            haze_start[ch]=(haze_write+EUREKA_LEN-span-64u)&EUREKA_MASK;
        }
        if(s->value[1][4] && (rng()&255u)<(uint32_t)s->value[0][2])
            haze_reverse[ch]=(int8_t)!haze_reverse[ch];
        else if(!s->value[1][4]) haze_reverse[ch]=0;
    }
    haze_phase[ch]=phase;
    /* MIYAKO HAZE: bit reduction and random noise increase toward extremes */
    {
        int32_t haz=haze_current_haze-64;
        uint32_t amount=(uint32_t)(haz<0?-haz:haz)*2u;
        unsigned bits=16u-(unsigned)((amount*(s->value[1][4]?9u:6u))/127u);
        int32_t step=1<<(16-bits);
        int32_t quant=(wet/step)*step;
        int32_t noise=((int32_t)(rng()&65535u)-32768)*(int32_t)amount/
                      (s->value[1][4]? 3800:10000);
        wet=clamp16(quant+noise);
    }
    return wet;
}
static void haze_frame(int32_t *out,const int16_t dry[2],const struct dnfx_state *s)
{
    unsigned ch;
    uint32_t speed,loopmode=(uint32_t)s->value[1][0];
    int32_t wet[2],m=(int32_t)s->value[0][0];
    uint32_t required=haze_span[0]>haze_span[1]?haze_span[0]:haze_span[1];
    /* Record before reading; invalid bulk bytes never get read before filled. */
    if(!s->value[1][5]) {
        for(ch=0u;ch<2u;++ch) {
            int32_t v=dry[ch];
            if(s->value[1][6]) v=clamp16(v*4); /* +12dB HIGH GAIN */
            if(s->value[1][3] && haze_valid==EUREKA_LEN)
                v=(v+(int32_t)haze_ring[haze_write*2u+ch])/2;
            haze_ring[haze_write*2u+ch]=(int16_t)v;
        }
        haze_write=(haze_write+1u)&EUREKA_MASK;
        if(haze_valid<EUREKA_LEN) ++haze_valid;
    }
    for(ch=0u;ch<2u;++ch) {
        if(!haze_primed[ch] && haze_valid>=haze_span[ch]+64u) {
            haze_start[ch]=(haze_write+EUREKA_LEN-haze_span[ch]-64u)&EUREKA_MASK;
            haze_phase[ch]=0u;
            haze_primed[ch]=1u;
        }
    }
    speed=s->value[0][7]==0u?128u:s->value[0][7]==2u?512u:256u;
    if(s->value[1][2]) speed=(speed*3u)/4u; /* transpose */
    haze_lfo+=29528u; /* .33 Hz */
    if(s->value[1][1]) {
        int32_t drift=((int32_t)wave[haze_lfo>>24]*
                       (int32_t)s->value[1][1])/32767;
        speed=(uint32_t)((int32_t)speed+drift);
    }
    wet[0]=haze_voice(0u,s,speed);
    wet[1]=loopmode==0u?wet[0]:haze_voice(1u,s,loopmode==2u?speed*2u:speed);
    if(!s->value[1][7] && loopmode!=0u) {
        int32_t sum=(wet[0]+wet[1])/2;
        wet[0]=wet[1]=sum;
    }
    for(ch=0u;ch<2u;++ch) {
        int32_t filtered=wet[ch],mix=m;
        int32_t dist=(int32_t)s->value[0][3]-64;
        int32_t amount=dist<0?-dist:dist;
        if(amount>1) {
            /* Filter attenuation with smoothing; right/left of midpoint */
            uint32_t alpha=(uint32_t)(32000/(1u+(uint32_t)amount));
            haze_filter[ch]+=((filtered-haze_filter[ch])*(int32_t)alpha)>>15;
            filtered=dist<0?haze_filter[ch]:filtered-haze_filter[ch];
        }
        filtered=tape_soft(filtered,s->value[1][4]?422u:333u);
        if(haze_valid<required+32u) mix=0;
        filtered=(filtered*mix)/127; /* 100% wet: no dry added */
        if(s->value[0][6]) {
            uint32_t p=(haze_verb_pos+(ch?VERB_LEN-3599u:VERB_LEN-2951u))&VERB_MASK;
            int32_t delay=haze_verb[ch][p];
            int32_t verbWet=(delay*(int32_t)s->value[0][6])/244;
            haze_verb[ch][haze_verb_pos]=(int16_t)clamp16(filtered+delay*55/100);
            filtered=clamp16(filtered+verbWet);
        }
        out[ch]=clamp16(filtered);
    }
    haze_verb_pos=(haze_verb_pos+1u)&VERB_MASK;
}
void rack_audio_reset(int mode)
{
    unsigned i,j;
    if(mode==DNFX_EUREKA) {
        haze_write=haze_valid=haze_verb_pos=0u;
        haze_lfo=0u;
        haze_current_haze=64;
        for(i=0;i<2u;++i) {
            haze_phase[i]=haze_span[i]=haze_target[i]=0u;
            haze_start[i]=0u;haze_reverse[i]=0;haze_primed[i]=0u;
            haze_filter[i]=0;
            for(j=0;j<VERB_LEN;++j) haze_verb[i][j]=0;
        }
        /* Bulk buffer MUST NOT be cleared in ISR; validity guards reads. */
    } else if(mode==DNFX_REEL) {
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
    if(mode==DNFX_EUREKA) haze_setup(s);
    for(n=0u;n<32u;++n) {
        int16_t dry[2];
        int32_t wet[2]={0,0};
        for(ch=0u;ch<2u;++ch)
            dry[ch]=(int16_t)(in[n*2u+ch]>>16);
        if(mode==DNFX_REEL) reel_frame(wet,dry,s);
        else if(mode==DNFX_CRUSH) crush_frame(wet,dry,s);
        else if(mode==DNFX_EUREKA) haze_frame(wet,dry,s);
        else {wet[0]=dry[0];wet[1]=dry[1];}
        for(ch=0u;ch<2u;++ch)
            out[n*2u+ch]=wet[ch]*65536;
    }
}
