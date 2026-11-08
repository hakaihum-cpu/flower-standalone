/* SPDX-License-Identifier: GPL-3.0-only
 * FM-1 native, finite-memory effects. The original MIYAKO parameter and
 * multi-FX semantics still require fidelity tests; EUREKA is a bounded,
 * simplified native implementation and not the original JUCE model.
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
/* exp(ln(2)*semitones/12) for -12..+12 semitones, no libm.
 * 6th-order polynomial absolute error below 0.0001 on this bounded span. */
static float pitch_ratio(unsigned knob)
{
    float x=((int)knob-64)*0.69314718056f/63.f;
    return 1.f+x*(1.f+x*(0.5f+x*(0.16666667f+
           x*(0.04166667f+x*(0.008333333f+x*0.001388889f)))));
}
void mi_fx_dsp_reset_shared(mi_fx_dsp_t *p)
{
    if(!p)return;
    /* O(1): never clear 64 KiB in the audio interrupt. Validity guards
     * prevent stale samples from DELAY/EUREKA being played back. */
    p->delay_write=0u;p->delay_valid=0u;
    p->eureka_write=0u;p->eureka_valid=0u;
    p->eureka_decimate=0u;p->eureka_heard=0u;
    p->eureka_content_valid=0u;p->eureka_quiet_samples=0u;
    p->eureka_sum=0.f;p->eureka_phase=0.f;p->eureka_filter=0.f;
    p->eureka_last_pitch=255u;
}
void mi_fx_dsp_reset_grain(mi_fx_dsp_t *p)
{
    if(!p)return;
    p->dreamy_write=0u;p->dreamy_valid=0u;
    p->dreamy_phase=0.f;p->dreamy_filter=0.f;
    p->dreamy_last_pitch=255u;
}
int mi_fx_dsp_eureka_ready(const mi_fx_dsp_t *p,unsigned time,unsigned haze)
{
    if(!p)return 0;
    if(time>127u)time=127u;
    if(haze>127u)haze=127u;
    /* Last grain read can be this far behind the write head. Delay wet
     * processing until the full lookback is actually captured; EUREKA's
     * processor itself continues returning wet-only audio. */
    float seconds=0.10f+1.60f*(float)time/127.f
                  +0.8f*(0.05f+0.24f*(float)haze/127.f);
    /* The raw 2-second buffer can be completely full of pre-roll SILENCE:
     * only samples written after actual input activity count as wet-ready. */
    return (float)p->eureka_valid>seconds*14700.f+2.f &&
           (float)p->eureka_content_valid>seconds*14700.f+2.f;
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
    p->dreamy_last_pitch=255u;
    mi_memzero(delay_memory,sizeof delay_memory);
    mi_memzero(chorus_memory,sizeof chorus_memory);
    mi_memzero(reverb_memory,sizeof reverb_memory);
    mi_memzero(dreamy_memory,sizeof dreamy_memory);
}
float mi_fx_dsp_process_ex(mi_fx_dsp_t *p,unsigned mode,float dry,
                           float sample_rate,unsigned p1,unsigned p2,
                           const uint8_t extra[4])
{
    unsigned pitch=extra?(unsigned)extra[0]:64u;
    unsigned reverse=extra?(unsigned)extra[1]:0u;
    unsigned density=extra?(unsigned)extra[2]:64u;
    unsigned feedback=extra?(unsigned)extra[3]:0u;
    if(pitch>127u)pitch=127u;
    if(reverse>127u)reverse=127u;
    if(density>127u)density=127u;
    if(feedback>127u)feedback=127u;
    if(!p)return dry;
    if(sample_rate<8000.f)sample_rate=44100.f;
    if(p1>127u)p1=127u;
    if(p2>127u)p2=127u;
    dry=bounded(dry,-1.f,1.f);
    if(mode==MI_FX_BITCRUSH){
        /* BITS 2..16 and HOLD 1..64 samples: real sample-rate/bit reduction. */
        unsigned bits=2u+(p1*14u/127u);
        unsigned hold=1u+(p2*63u/127u);
        if(!p->crush_hold_counter || p->crush_hold_counter>hold){
            float levels=(float)(1u<<(bits-1u));
            int32_t q=(int32_t)(dry*levels);
            p->crush_held=bounded((float)q/levels,-1.f,1.f);
            p->crush_hold_counter=hold;
        }
        --p->crush_hold_counter;
        return p->crush_held;
    }
    if(mode==MI_FX_EUREKA) {
        /* No heap or buffer clears on the audio thread. If playback starts
         * after idle, require NEW audible samples before enabling wet. Keep
         * captured audio/tails available for 2 seconds after the last sound. */
        if(dry>0.001f || dry< -0.001f){
            p->eureka_heard=1u;
            p->eureka_quiet_samples=0u;
        }else if(p->eureka_quiet_samples<88200u){
            if(++p->eureka_quiet_samples==88200u){
                p->eureka_heard=0u;
                p->eureka_content_valid=0u;
            }
        }
        /* Two-second short-buffer Haze adaptation. 14.7 kHz 16-bit
         * continuous capture; two crossfaded repeat grains with TIME
         * and HAZE. Wet only. Uses DELAY memory; cannot run with DELAY. */
        float haze=(float)p2/127.f;
        float grain=(0.05f+0.24f*haze)*(0.7f+0.6f*density/127.f);
        float history=0.10f+1.60f*(float)p1/127.f;
        float grain_samples=grain*14700.f;
        float base=history*14700.f;
        if(p->eureka_last_pitch!=(uint8_t)pitch){
            p->eureka_last_pitch=(uint8_t)pitch;
            p->eureka_pitch_cache=pitch_ratio(pitch);
        }
        float playback=reverse>=64u?-p->eureka_pitch_cache:p->eureka_pitch_cache;
        float sweep=1.f-playback;
        float ph0=p->eureka_phase,ph1=ph0+0.5f;
        float wet=0.f,weights=0.f;
        unsigned k;
        if(ph1>=1.f)ph1-=1.f;
        for(k=0;k<2u;k++){
            float ph=k==0u?ph0:ph1;
            /* A grain's read-head velocity is +pitch_ratio (or negative for
             * REVERSE), independently of the incoming note's pitch. */
            float behind=base+(ph-0.5f)*grain_samples*sweep;
            float triangle=ph<0.5f?ph*2.f:(1.f-ph)*2.f;
            if(behind>=2.f && behind<(float)MI_EUREKA_FRAMES-2.f &&
               (float)p->eureka_valid>behind+2.f){
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
        p->eureka_sum+=bounded(dry+wet*(0.35f*feedback/127.f),-1.f,1.f);
        if(++p->eureka_decimate==3u){
            delay_memory[p->eureka_write]=quant(p->eureka_sum*(1.f/3.f));
            p->eureka_sum=0.f;
            p->eureka_decimate=0u;
            if(++p->eureka_write==MI_EUREKA_FRAMES)p->eureka_write=0u;
            if(p->eureka_valid<MI_EUREKA_FRAMES)++p->eureka_valid;
            if(p->eureka_heard&&p->eureka_content_valid<MI_EUREKA_FRAMES)
                ++p->eureka_content_valid;
        }
        return wet;
    }
    if(mode==MI_FX_DREAMY||mode==MI_FX_PITCHSHIFT) {
        /* PITCHSHIFT shares the existing grain RAM, never an extra buffer.
         * UI: P1 = -12..+12 semitones, P2 = grain density. */
        if(mode==MI_FX_PITCHSHIFT){
            pitch=p1;density=p2;
            p1=72u;p2=64u;reverse=0u;feedback=0u;
        }
        /* Native bounded two-grain shifter. GRAIN and SPACE are preserved;
         * PITCH, REVERSE, DENSITY and FEEDBACK now change actual samples.
         * This is NOT the full original JUCE Dreamy implementation. */
        float size=0.08f+0.44f*(p1/127.f);
        float grain_samples=size*sample_rate*(1.3f-0.6f*density/127.f);
        if(p->dreamy_last_pitch!=(uint8_t)pitch){
            p->dreamy_last_pitch=(uint8_t)pitch;
            p->dreamy_pitch_cache=pitch_ratio(pitch);
        }
        float playback=p->dreamy_pitch_cache;
        int rev=reverse>=64u;
        float base,sweep,ph0,ph1,wet,weights=0.f,total=0.f;
        uint32_t write=p->dreamy_write&(MI_DREAM_N-1u);
        unsigned i;
        if(grain_samples<100.f)grain_samples=100.f;
        /* Bound the entire read-head excursion inside 32,768 samples
         * even at +/-12 semitones, max SPREAD and max SPACE. */
        if(rev && grain_samples>8000.f)grain_samples=8000.f;
        if(!rev && grain_samples>20000.f)grain_samples=20000.f;
        if(rev)playback=-playback;
        sweep=1.f-playback;
        base=(rev?2048.f+2.f*grain_samples:
                  4096.f+0.7f*grain_samples)
              +((int)p2-64)*12.f; /* SPACE shifts the grain's position */
        ph0=p->dreamy_phase;
        ph1=ph0+0.5f;
        if(ph1>=1.f)ph1-=1.f;
        for(i=0;i<2u;i++){
            float phase=i==0u?ph0:ph1;
            /* SPREAD offsets the two overlapping heads in opposite
             * directions, creating audible texture even at neutral pitch. */
            float spread=grain_samples*(0.01f+0.09f*density/127.f);
            float offset=base+(phase-0.5f)*grain_samples*sweep+
                         (i==0u?-spread:spread);
            unsigned at,delay;
            float fract,a,b,weight;
            if(offset<2.f)offset=2.f;
            if(offset>(float)MI_DREAM_N-3.f)offset=(float)MI_DREAM_N-3.f;
            delay=(unsigned)offset;
            fract=offset-(float)delay;
            at=(p->dreamy_write-delay)&(MI_DREAM_N-1u);
            a=read16(dreamy_memory[at]);
            b=read16(dreamy_memory[(at-1u)&(MI_DREAM_N-1u)]);
            weight=phase<0.5f?phase*2.f:(1.f-phase)*2.f;
            if(p->dreamy_valid>delay+2u){
                total+=(a+(b-a)*fract)*weight;
                weights+=weight;
            }
        }
        wet=weights>0.01f?total/weights:0.f;
        if(p->dreamy_valid<MI_DREAM_N)++p->dreamy_valid;
        /* No stale/empty unwritten memory may become the loop head. */
        dreamy_memory[write]=quant(dry+wet*(0.5f*feedback/127.f));
        ++p->dreamy_write;
        p->dreamy_phase+=1.f/grain_samples;
        if(p->dreamy_phase>=1.f)p->dreamy_phase-=1.f;
        /* The independent grain heads can jump at wrap/parameter changes.
         * A one-pole slew at the wet output reduces audible crackles while
         * keeping the fixed sample buffer and grain scheduler unchanged. */
        p->dreamy_filter+=(wet-p->dreamy_filter)*0.65f;
        return bounded(p->dreamy_filter,-1.f,1.f);
    }
    if(mode==MI_FX_DELAY) {
        /* 40..680 ms (fixed FM-1 buffer); one tap; 0..0.96 feedback */
        float ms=40.f+640.f*(p1/127.f);
        uint32_t n=(uint32_t)(sample_rate*ms/1000.f);
        if(n<2u)n=2u;
        if(n>=MI_DELAY_N)n=MI_DELAY_N-1u;
        uint32_t pos=p->delay_write&(MI_DELAY_N-1u);
        /* At write==n, sample zero is already recorded and is the
         * first valid delayed output. Strict > skips it forever for a
         * single impulse, producing no echo (observed in CI #264). */
        float delayed=p->delay_valid>=n?
            read16(delay_memory[(p->delay_write-n)&(MI_DELAY_N-1u)]):0.f;
        float feedback=0.96f*(p2/127.f);
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
        float sum=0.f,feedback=0.50f+0.465f*(p1/127.f);
        float damping=0.04f+0.85f*(p2/127.f);
        unsigned i;
        for(i=0;i<4u;i++) {
            uint32_t at=p->reverb_write[i]&(MI_REVERB_N-1u);
            float value=read16(reverb_memory[i][(p->reverb_write[i]-dlen[i])&(MI_REVERB_N-1u)]);
            p->reverb_damp[i]+=(value-p->reverb_damp[i])*(1.f-damping);
            reverb_memory[i][at]=quant(dry*0.48f+p->reverb_damp[i]*feedback);
            ++p->reverb_write[i];
            sum+=value*0.25f;
        }
        return bounded(sum,-1.f,1.f);
    }
    return dry;
}
/* Legacy host DSP tests and historical callers retain their original API. */
float mi_fx_dsp_process(mi_fx_dsp_t *p,unsigned mode,float dry,
                         float sample_rate,unsigned p1,unsigned p2)
{
    static const uint8_t defaults[4]={64u,0u,64u,0u};
    return mi_fx_dsp_process_ex(p,mode,dry,sample_rate,p1,p2,defaults);
}
