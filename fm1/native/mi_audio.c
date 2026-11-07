/* SPDX-License-Identifier: GPL-3.0-only
 * Native FM-1 oscillator with initial Delay, Reverb and Chorus DSP.
 * Not all original MIYAKO FX are implemented or fidelity-tested.
 * EUREKA and DREAMY here are simplified finite-memory native implementations,
 * not complete source-faithful ports of the original MIYAKO processors.
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
    a->arp_random=0x50414e5au;
    mi_fx_dsp_init(&native_fx);
    mi_rec_init(&a->recorder);
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
    v->phase=0.f;v->phase2=0.f;v->subphase=0.f;
    v->increment=note_freq(note)/a->sample_rate;
    v->env=0.f;v->env_stage=0;v->release_step=0.f;v->age=++a->age_counter;
}
void mi_audio_note_off(mi_audio_t *a,uint8_t note)
{
    unsigned i;
    for(i=0;i<MI_NATIVE_POLY;i++)
        if(a->v[i].active&&a->v[i].note==note)a->v[i].held=0;
}
static void all_voices_off(mi_audio_t *a)
{
    unsigned i;
    for(i=0;i<MI_NATIVE_POLY;i++){
        a->v[i].active=0;a->v[i].held=0;a->v[i].env=0.f;
    }
}
void mi_audio_panic(mi_audio_t *a)
{
    if(!a)return;
    all_voices_off(a);
    mi_memzero(a->held_keys,sizeof a->held_keys);
    mi_memzero(a->held_velocity,sizeof a->held_velocity);
    mi_memzero(a->held_refs,sizeof a->held_refs);
    a->arp_clock=0;a->arp_gate_clock=0;a->arp_sounding=0;
    a->held_total=0;
    mi_rec_stop(&a->recorder);
}
/* Keep raw keyboard/midi held notes; arpeggiator and live voice routing
 * share the same source so release cannot leave stuck repeating notes. */
void mi_audio_input_on(mi_audio_t *a,const mi_state_t *s,
                       uint8_t note,uint8_t velocity)
{
    if(!a||!s||note>=128u)return;
    if(!a->held_refs[note]){
        ++a->held_total;
        a->held_keys[note>>5]|=1u<<(note&31u);
    }
    if(a->held_refs[note]<255u)++a->held_refs[note];
    a->held_velocity[note]=velocity?velocity:1u;
    if(s->arp[0]==0u && a->held_refs[note]==1u)
        mi_audio_note_on(a,note,velocity);
}
void mi_audio_input_off(mi_audio_t *a,const mi_state_t *s,uint8_t note)
{
    if(!a||!s||note>=128u)return;
    if(!a->held_refs[note])return;
    --a->held_refs[note];
    if(a->held_refs[note])return;
    --a->held_total;
    a->held_keys[note>>5]&=~(1u<<(note&31u));
    a->held_velocity[note]=0;
    if(s->arp[0]==0u)mi_audio_note_off(a,note);
}
static unsigned held_count(const mi_audio_t *a){return a->held_total;}
static uint8_t held_nth(const mi_audio_t *a,unsigned index)
{
    unsigned n;
    for(n=0;n<128u;n++){
        if(a->held_keys[n>>5]&(1u<<(n&31u))){
            if(index==0u)return (uint8_t)n;
            --index;
        }
    }
    return 0u;
}
static void arp_sync(mi_audio_t *a,const mi_state_t *s)
{
    unsigned mode=s->arp[0]<=4u?s->arp[0]:0u;
    if(mode==a->arp_mode)return;
    all_voices_off(a);
    a->arp_mode=(uint8_t)mode;
    a->arp_clock=0;a->arp_gate_clock=0;
    a->arp_pos=0;a->arp_dir=0;a->arp_sounding=0;
    if(!mode){
        unsigned note;
        for(note=0;note<128u;note++)
            if(a->held_keys[note>>5]&(1u<<(note&31u)))
                mi_audio_note_on(a,(uint8_t)note,a->held_velocity[note]);
    }
}
static void arp_tick(mi_audio_t *a,const mi_state_t *s)
{
    unsigned n,octaves,total,index,base,octave;
    uint32_t period,gate;
    int bpm=s->global[0];
    if(a->arp_mode==0u)return;
    if(a->arp_sounding&&a->arp_gate_clock){
        --a->arp_gate_clock;
        if(!a->arp_gate_clock){
            mi_audio_note_off(a,a->arp_note);
            a->arp_sounding=0;
        }
    }
    n=held_count(a);
    if(n==0u){
        if(a->arp_sounding)mi_audio_note_off(a,a->arp_note);
        a->arp_sounding=0;a->arp_clock=0;a->arp_pos=0;
        return;
    }
    if(a->arp_clock){
        --a->arp_clock;
        return;
    }
    if(bpm<40)bpm=40;
    if(bpm>240)bpm=240;
    octaves=s->arp[3];
    if(octaves<1u)octaves=1u;
    if(octaves>4u)octaves=4u;
    total=n*octaves;
    period=(uint32_t)(a->sample_rate*60.f/(float)bpm);
    period>>=(s->arp[1]<=3u?s->arp[1]:3u);
    if(period<16u)period=16u;
    gate=period*(uint32_t)(s->arp[2]?s->arp[2]:70u)/100u;
    if(gate<2u)gate=2u;
    if(gate>=period)gate=period-1u;
    if(a->arp_sounding){
        mi_audio_note_off(a,a->arp_note);
        a->arp_sounding=0;
    }
    if(total==1u)index=0u;
    else if(a->arp_mode==4u){
        a->arp_random^=a->arp_random<<13;
        a->arp_random^=a->arp_random>>17;
        a->arp_random^=a->arp_random<<5;
        index=a->arp_random%total;
    }else if(a->arp_mode==3u){
        unsigned sequence=2u*(total-1u);
        unsigned step=a->arp_pos%sequence;
        index=step<total?step:sequence-step;
    }else if(a->arp_mode==2u){
        index=total-1u-a->arp_pos%total;
    }else{
        index=a->arp_pos%total;
    }
    base=index%n;octave=index/n;
    {
        unsigned note=(unsigned)held_nth(a,base)+12u*octave;
        uint8_t vel=a->held_velocity[held_nth(a,base)];
        if(note>127u)note=127u;
        a->arp_note=(uint8_t)note;
        mi_audio_note_on(a,a->arp_note,vel?vel:100u);
    }
    a->arp_sounding=1u;
    a->arp_gate_clock=gate;
    a->arp_clock=period-1u;
    ++a->arp_pos;
}
static float lfo_value(unsigned wave,float phase)
{
    if(wave==1u)return phase<0.5f?4.f*phase-1.f:3.f-4.f*phase;
    if(wave==2u)return 2.f*phase-1.f;
    if(wave==3u)return phase<0.5f?1.f:-1.f;
    return sin01(phase);
}
static float fx_stage(mi_audio_t *a,const mi_state_t *s,
                      unsigned mode,unsigned wet_knob,float dry,float *smooth)
{
    float target=mode==MI_FX_OFF?0.f:(float)wet_knob/127.f;
    float wet=dry;
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
    }else if(mode!=MI_FX_OFF&&mode<MI_FX_COUNT){
        wet=mi_fx_dsp_process_ex(&native_fx,mode,dry,a->sample_rate,
                                s->fx_options[mode][0],s->fx_options[mode][1],
                                s->fx_extra[mode]);
    }else target=0.f;
    /* EUREKA is genuinely wet-only and its history starts empty. Previously
     * the wet mixer muted a newly played note before the TIME+HAZE history
     * was available (observed in #266's first-second host WAV).
     * Keep dry audible until all two crossfade-grain lookbacks are valid. */
    if(mode==MI_FX_EUREKA &&
       !mi_fx_dsp_eureka_ready(&native_fx,
           s->fx_options[MI_FX_EUREKA][0],
           s->fx_options[MI_FX_EUREKA][1]))
        target=0.f;
    *smooth+=(target-*smooth)*0.001f;
    return clampf(dry+(wet-dry)*(*smooth),-1.f,1.f);
}
static float effect(mi_audio_t *a,const mi_state_t *s,float dry)
{
    unsigned first=s->fx<MI_FX_COUNT?s->fx:MI_FX_OFF;
    unsigned second=s->fx2<MI_FX_COUNT?s->fx2:MI_FX_OFF;
    unsigned prev_shared,new_shared;
    if(!mi_fx_pair_supported(first,second))second=MI_FX_OFF;
    if(first!=a->last_mode||second!=a->last_mode2){
        prev_shared=a->last_mode==MI_FX_DELAY||a->last_mode==MI_FX_EUREKA?
                    a->last_mode:
                    a->last_mode2==MI_FX_DELAY||a->last_mode2==MI_FX_EUREKA?
                    a->last_mode2:MI_FX_OFF;
        new_shared=first==MI_FX_DELAY||first==MI_FX_EUREKA?first:
                   second==MI_FX_DELAY||second==MI_FX_EUREKA?second:MI_FX_OFF;
        if(prev_shared!=new_shared)
            mi_fx_dsp_reset_shared(&native_fx);
        if((first==MI_FX_REEL||second==MI_FX_REEL)!=
           (a->last_mode==MI_FX_REEL||a->last_mode2==MI_FX_REEL))
            a->wr=a->valid=a->phase=a->segment=0u;
        a->last_mode=(uint8_t)first;a->last_mode2=(uint8_t)second;
        a->wet_current=0.f;a->wet_current2=0.f;
    }
    dry=fx_stage(a,s,first,s->wet,dry,&a->wet_current);
    return fx_stage(a,s,second,s->wet2,dry,&a->wet_current2);
}
void mi_audio_rec_request(mi_audio_t *a,unsigned command)
{
    if(a && (command==1u || command==2u || command==3u))
        a->rec_command=(uint8_t)command;
}
void mi_audio_render(mi_audio_t *a,const mi_state_t *s,int32_t *lr,uint32_t n,uint32_t master_q12)
{
    uint32_t i,k;
    float attack_step,decay_step,release_samples,sustain,lfo_speed;
    if(!a||!s||!lr)return;
    float scale=clampf(master_q12/4096.f,0.f,1.f)*clampf(s->level/127.f,0.f,1.f)*5000000.f;
    uint8_t wave=s->wave==MI_WAVE_MULTI?s->osc1_wave:
                 s->wave>3u?0u:s->wave;
    uint8_t wave2=s->osc2_wave>3?0:s->osc2_wave;
    /* OSC2 tuning = octave (-2..+2) and +/-50 cents fine detune.
     * Bounded 2nd-order exp approximation, no freestanding libm needed. */
    const float fine=((int)s->osc2_detune-64)*(0.5f/64.f);
    const float ln2x=fine*0.05776226505f;
    float osc2_ratio=1.f+ln2x+0.5f*ln2x*ln2x;
    /* OSC2 is active ONLY when HOME WAVE explicitly selects MULTI. */
    float osc2_mix=s->wave==MI_WAVE_MULTI?
                   (float)s->osc2_level/127.f*0.5f:0.f;
    switch(s->osc2_octave){
    case 0u:osc2_ratio*=0.25f;break;
    case 1u:osc2_ratio*=0.5f;break;
    case 3u:osc2_ratio*=2.f;break;
    case 4u:osc2_ratio*=4.f;break;
    default:break;
    }
    float duty=0.05f+0.9f*(s->width/127.f);
    float atk=(float)s->env[0]/127.f,dec=(float)s->env[1]/127.f;
    float rel=(float)s->env[3]/127.f;
    sustain=clampf((float)s->env[2]/127.f,0.f,1.f);
    attack_step=1.f/(a->sample_rate*(0.002f+2.f*atk*atk));
    decay_step=(1.f-sustain)/(a->sample_rate*(0.005f+2.f*dec*dec));
    release_samples=a->sample_rate*(0.005f+3.f*rel*rel);
    lfo_speed=(0.07f+15.f*(float)s->lfo[0]/127.f)/a->sample_rate;
    arp_sync(a,s);
    /* UI updates only an 8-bit command; the audio thread owns the samples. */
    if(a->rec_command){
        uint8_t cmd=a->rec_command;
        a->rec_command=0u;
        if(cmd==1u)mi_rec_toggle_record(&a->recorder);
        if(cmd==2u)mi_rec_toggle_play(&a->recorder);
        if(cmd==3u)mi_rec_clear(&a->recorder);
    }
    for(i=0;i<n;i++){
        float sample=0.f,lfo,mod,voice_duty=duty;
        arp_tick(a,s);
        a->lfo_phase=wrap01(a->lfo_phase+lfo_speed);
        lfo=lfo_value(s->lfo[1],a->lfo_phase);
        mod=lfo*(float)s->lfo[2]/127.f;
        if(s->lfo[3]==1u)voice_duty=clampf(duty+mod*0.3f,0.03f,0.97f);
        for(k=0;k<MI_NATIVE_POLY;k++){
            mi_voice_t *v=&a->v[k];
            if(!v->active)continue;
            if(!v->held&&v->env_stage!=3u){
                v->env_stage=3u;
                v->release_step=v->env/release_samples;
            }
            if(v->env_stage==0u){
                v->env+=attack_step;
                if(v->env>=1.f){v->env=1.f;v->env_stage=1u;}
            }else if(v->env_stage==1u){
                v->env-=decay_step;
                if(v->env<=sustain){v->env=sustain;v->env_stage=2u;}
            }else if(v->env_stage==2u){
                v->env=sustain;
            }else{
                v->env-=v->release_step;
                if(v->env<=0.f){v->env=0.f;v->active=0u;continue;}
            }
            {
                float main=osc(wave,v->phase,voice_duty),sub=sin01(v->subphase);
                float submix=s->sub/127.f;
                float base=main*(1.f-submix)+sub*submix;
                float value=base;
                /* No unnecessary second waveform evaluation when OSC2 is off.
                 * Preserve the previous OSC1/SUB output exactly by default. */
                if(osc2_mix>0.f){
                    float second=osc(wave2,v->phase2,voice_duty);
                    value=base*(1.f-osc2_mix)+second*osc2_mix;
                }
                if(s->crush){
                    int bits=16-(int)(s->crush*13u/127u);
                    float levels=(float)(1u<<bits);
                    float quant=(float)(int)(value*levels);
                    value=quant/levels;
                }
                if(s->lfo[3]==2u)
                    value*=clampf(1.f-0.5f*(float)s->lfo[2]/127.f*(1.f-lfo),0.f,1.f);
                sample+=value*v->env*(v->vel/127.f)*0.18f;
                {
                    float pitch_mod=s->lfo[3]==0u?clampf(1.f+mod*0.059f,0.8f,1.2f):1.f;
                    v->phase=wrap01(v->phase+v->increment*pitch_mod);
                    if(osc2_mix>0.f)
                        v->phase2=wrap01(v->phase2+v->increment*osc2_ratio*pitch_mod);
                    v->subphase=wrap01(v->subphase+v->increment*0.5f*pitch_mod);
                }
            }
        }
        sample=clampf(sample,-1.f,1.f);
        sample=effect(a,s,sample);
        /* Existing FX wet-smoothing principle: crossfade the recorder monitor
         * in/out over milliseconds. Simply adding two full-scale signals
         * clipped during layered playback and caused sharp transitions. */
        {
            float monitored=mi_rec_tick(&a->recorder,sample,s->seq);
            float target=a->recorder.playing?1.f:0.f;
            a->rec_monitor_mix+=(target-a->rec_monitor_mix)*0.0015f;
            sample=clampf(sample*(1.f-0.5f*a->rec_monitor_mix)+
                         monitored*(0.5f*a->rec_monitor_mix),-1.f,1.f);
        }
        {
            int32_t out=(int32_t)(clampf(sample,-1.f,1.f)*scale);
            lr[2u*i]=out;lr[2u*i+1u]=out;
        }
    }
}
