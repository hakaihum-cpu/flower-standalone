/* SPDX-License-Identifier: GPL-3.0-only
 * Native FM-1 oscillator with initial Delay, Reverb and Chorus DSP.
 * Not all original MIYAKO FX are implemented or fidelity-tested.
 * EUREKA and DREAMY here are simplified finite-memory native implementations,
 * not complete source-faithful ports of the original MIYAKO processors.
 */
#include "mi_audio.h"
#include "mi_fx_dsp.h"
#include "mi_memory.h"
#include "mi_piano_set1.h"
static mi_fx_dsp_t native_fx;
static float clampf(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
/* Peak compression keeps the slope continuous at +/-0.75, unlike a hard
 * clamp. It removes clipped edges from overlapping voices and serial FX
 * without changing quiet notes or allocating an audio buffer. */
static float mi_soft_limit(float x)
{
    const float threshold=0.75f;
    if(x>threshold){
        float over=x-threshold;
        return threshold+0.25f*over/(0.25f+over);
    }
    if(x< -threshold){
        float over=-x-threshold;
        return -(threshold+0.25f*over/(0.25f+over));
    }
    return x;
}
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
    a->step_random=0x53455131u;
    mi_fx_dsp_init(&native_fx);
    mi_rec_init(&a->recorder);
    mi_slicer_init(&a->slicer);
    a->video_effective[2]=64u;a->video_effective[3]=64u;
    a->last_mode=MI_FX_OFF;
}
/* Nearest original keys: MIDI 51, 58, 65. */
static unsigned piano_zone(uint8_t note){return note<=54u?0u:note<=61u?1u:2u;}
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
    v->drum_type=0u;v->drum_variant=0u;v->drum_from_step=0u;
    v->piano_zone=(uint8_t)piano_zone(note);
    v->piano_pos=0.f;v->piano_gain=1.f;
    v->piano_step=(float)MI_PIANO_SET1_RATE/a->sample_rate*
                  note_freq((int)note)/
                  note_freq((int)mi_piano_set1_roots[v->piano_zone]);
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
/* The six FM drum voices are synthesized; no sampled drum ROM is needed.
 * Six consecutive local keys: kick, snare, closed hat, open hat, crash,
 * low tom. Higher six-key groups vary tuning, envelope and FM index. */
static void drum_on(mi_audio_t *a,const mi_state_t *s,
                    uint8_t note,uint8_t velocity,uint8_t from_step)
{
    unsigned i,idx,variant;
    static const float hz[6]={55.f,182.f,2460.f,1760.f,1120.f,84.f};
    static const float decay[6]={0.65f,0.24f,0.075f,0.40f,1.32f,0.55f};
    mi_voice_t *v=0;
    /* NOTE 53 is the first physical kick. Octave shifts preserve each
     * six-key drum order, including notes below 53. */
    idx=((unsigned)note+1u)%6u;
    if(note>=53u)variant=(((unsigned)note-53u)/6u)%4u;
    else variant=((53u-(unsigned)note)/6u)%4u;
    mi_audio_note_on(a,note,velocity);
    for(i=0u;i<MI_NATIVE_POLY;i++)
        if(a->v[i].active&&a->v[i].age==a->age_counter){v=&a->v[i];break;}
    if(!v)return;
    v->drum_type=(uint8_t)(idx+1u);
    v->drum_variant=(uint8_t)variant;
    v->drum_from_step=from_step;
    v->env=1.f;v->env_stage=2u;v->held=0u;
    v->drum_pitch_env=1.f;
    v->increment=hz[idx]*(0.7f+(float)s->drum_tone/256.f+
                (float)variant*0.07f)/a->sample_rate;
    {
        float duration=decay[idx]*(0.45f+(float)s->drum_decay/127.f)
                       *(0.8f+0.15f*(float)variant);
        v->drum_decay_rate=clampf(1.f-7.f/(duration*a->sample_rate),
                                 0.85f,0.99998f);
    }
}
/* Independent 4-track FMDrum sequencer; its notes are never routed through
 * live held-key state or the selected oscillator, and its PCM reaches REC.
 * Sequence scheduling runs at the audio sample clock, not LCD frame rate. */
static void mi_step_tick(mi_audio_t *a,const mi_state_t *s)
{
    static const uint8_t notes[4]={53u,54u,55u,58u};
    unsigned t,dir=s->step_dir,len=s->step_len,pos,bpm;
    uint32_t period;
    if(!s->step_play){
        /* Preserve next position for MIDI Continue after Stop. Internal
         * K2 STOP still restarts the pattern from its beginning. */
        if(!a->step_midi_mode)a->step_active=0u;
        a->step_wait=0u;
        return;
    }
    if(len<8u||len>16u)len=8u;
    if(dir>2u)dir=0u;
    if(!a->step_active||a->step_last_dir!=dir||a->step_last_len!=len){
        if(!a->step_active&&!a->step_midi_mode)
            mi_rec_internal_start(&a->recorder);
        a->step_active=1u;
        a->step_wait=0u;
        a->step_next=(uint8_t)(dir==1u?len-1u:0u);
        a->step_last_dir=(uint8_t)dir;
        a->step_last_len=(uint8_t)len;
    }
    if(a->step_midi_mode){
        /* A 1/16 note is 6 MIDI Clock pulses, never a calculated BPM.
         * Clock messages are collected by the audio-thread MIDI poll. */
        if(!a->step_clock_pending)return;
        a->step_clock_pending=0u;
    }else if(a->step_wait){--a->step_wait;return;}
    pos=a->step_next;
    if(pos>=len)pos=0u;
    a->step_current=(uint8_t)pos;
    for(t=0u;t<4u;t++)
        if(s->step_pattern[t]&(uint16_t)(1u<<pos))
            drum_on(a,s,notes[t],112u,1u);
    if(dir==1u)a->step_next=(uint8_t)(pos==0u?len-1u:pos-1u);
    else if(dir==2u){
        a->step_random^=a->step_random<<13;
        a->step_random^=a->step_random>>17;
        a->step_random^=a->step_random<<5;
        a->step_next=(uint8_t)(a->step_random%len);
    }else a->step_next=(uint8_t)((pos+1u)%len);
    if(!a->step_midi_mode){
        bpm=s->global[0]<40?40u:(s->global[0]>240?240u:(unsigned)s->global[0]);
        /* Internal fallback: one 1/16 note = 15/BPM seconds. */
        period=(uint32_t)(a->sample_rate*15.f/(float)bpm+0.5f);
        if(period<1u)period=1u;
        a->step_wait=period-1u;
    }
}
static float drum_sample(mi_audio_t *a,const mi_state_t *s,mi_voice_t *v)
{
    float carrier=sin01(v->phase);
    float mod=sin01(v->phase2);
    float noise,voice=0.f;
    float brightness=0.15f+1.6f*(float)s->drum_color/127.f;
    a->rand_state^=a->rand_state<<13;
    a->rand_state^=a->rand_state>>17;
    a->rand_state^=a->rand_state<<5;
    noise=((int32_t)(a->rand_state>>16)-32768)*(1.f/32768.f);
    switch(v->drum_type){
    case 1u: /* kick: sine with exponentially dropping FM pitch */
        voice=sin01(v->phase+mod*0.17f*v->drum_pitch_env);
        break;
    case 2u: /* snare: noisy metallic FM plus a low pitched body */
        voice=0.55f*sin01(v->phase+mod*brightness*0.21f)+
              0.45f*noise;
        break;
    case 3u: /* closed hi-hat: short inharmonic FM and white noise */
        voice=0.60f*noise+0.40f*sin01(v->phase+mod*brightness);
        break;
    case 4u: /* open hi-hat: longer ring, softer attack */
        voice=0.48f*noise+0.52f*sin01(v->phase+mod*brightness*0.9f);
        break;
    case 5u: /* crash: multiple beating FM partials + noise */
        voice=0.62f*noise+0.38f*sin01(v->phase+
                       mod*(2.0f+brightness));
        break;
    case 6u: /* low tom: pitch drop and low inharmonic modulation */
        voice=0.87f*sin01(v->phase+mod*0.22f*v->drum_pitch_env)+
              0.13f*noise*v->drum_pitch_env;
        break;
    default:voice=carrier;break;
    }
    v->phase=wrap01(v->phase+v->increment*
                     (1.f+(v->drum_type==1u?5.f:
                            v->drum_type==6u?2.5f:0.3f)*v->drum_pitch_env));
    v->phase2=wrap01(v->phase2+v->increment*
                     (1.41421356f+0.12f*(float)v->drum_variant));
    v->drum_pitch_env*=0.9975f;
    v->env*=v->drum_decay_rate;
    if(v->env<0.0008f)v->active=0u;
    return voice*v->env*(v->vel/127.f)*
           ((float)s->drum_level/127.f)*0.31f;
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
    if(s->arp[0]==0u && a->held_refs[note]==1u){
        if(s->wave==MI_WAVE_FMDRUM)drum_on(a,s,note,velocity,0u);
        else mi_audio_note_on(a,note,velocity);
    }
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
    if(s->arp[0]==0u&&s->wave!=MI_WAVE_FMDRUM)mi_audio_note_off(a,note);
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
    a->arp_visual_frame=mode?0xffffu:0u;
    ++a->arp_visual_event;
    if(!mode){
        unsigned note;
        for(note=0;note<128u;note++)
            if(a->held_keys[note>>5]&(1u<<(note&31u))){
                /* Turning ARP off must restore the selected FMDrum voice,
                 * not an ordinary oscillator. */
                if(s->wave==MI_WAVE_FMDRUM)
                    drum_on(a,s,(uint8_t)note,a->held_velocity[note],0u);
                else
                    mi_audio_note_on(a,(uint8_t)note,a->held_velocity[note]);
            }
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
        if(s->wave==MI_WAVE_FMDRUM)drum_on(a,s,a->arp_note,vel?vel:100u,0u);
        else mi_audio_note_on(a,a->arp_note,vel?vel:100u);
    }
    a->arp_sounding=1u;
    /* Advance only when the ARP engine actually triggers a note:
     * intro once; carousel 81..161 loops, with no extra audio work. */
    if(a->arp_visual_frame==0xffffu) a->arp_visual_frame=0u;
    else if(a->arp_visual_frame>=161u) a->arp_visual_frame=81u;
    else ++a->arp_visual_frame;
    ++a->arp_visual_event;
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
/* LFO destinations are actual editable parameters, not display-only labels.
 * Targets are modulated on a local block snapshot; saved state is immutable.
 * Pitch/PWM/AMP (0..2) retain audio-rate modulation below. */
static uint8_t mod_u8(uint8_t base,float depth,unsigned hi)
{
    float v=(float)base+depth*64.f;
    if(v<0.f)v=0.f;
    if(v>(float)hi)v=(float)hi;
    return (uint8_t)(v+0.5f);
}
static void mod_fx_option(mi_state_t *m,unsigned mode,unsigned which,float depth)
{
    if(which>=2u)return;
    if(mode==MI_FX_SLICER)
        m->fx_slicer[which]=mod_u8(m->fx_slicer[which],depth,127u);
    else if(mode<MI_FX_STORAGE_COUNT)
        m->fx_options[mode][which]=mod_u8(m->fx_options[mode][which],depth,127u);
}
static void mod_fx_extra(mi_state_t *m,unsigned mode,unsigned which,float depth)
{
    /* SLICER uses two dedicated controls; all old FX extra offsets stay fixed. */
    if(mode<MI_FX_STORAGE_COUNT&&which<4u)
        m->fx_extra[mode][which]=mod_u8(m->fx_extra[mode][which],depth,127u);
}
static void apply_lfo_destination(mi_state_t *m,unsigned dest,float depth)
{
    unsigned first=m->fx<MI_FX_COUNT?m->fx:MI_FX_OFF;
    unsigned second=m->fx2<MI_FX_COUNT?m->fx2:MI_FX_OFF;
#define MOD(K,FIELD,HIGH) case K: m->FIELD=mod_u8(m->FIELD,depth,HIGH);break
    switch(dest){
    MOD(3,sub,127u);MOD(4,crush,127u);
    MOD(5,filter_cutoff,127u);MOD(6,filter_resonance,127u);
    MOD(7,osc2_level,127u);MOD(8,osc2_detune,127u);
    MOD(9,level,127u);
    MOD(10,env[0],127u);MOD(11,env[1],127u);
    MOD(12,env[2],127u);MOD(13,env[3],127u);
    MOD(14,wet,127u);
    case 15:mod_fx_option(m,first,0u,depth);break;
    case 16:mod_fx_option(m,first,1u,depth);break;
    MOD(17,wet2,127u);
    case 18:mod_fx_option(m,second,0u,depth);break;
    case 19:mod_fx_option(m,second,1u,depth);break;
    MOD(20,drum_tone,127u);MOD(21,drum_decay,127u);
    MOD(22,drum_color,127u);MOD(23,drum_level,127u);
    MOD(24,video_params[0],127u);MOD(25,video_params[1],127u);
    MOD(26,video_params[2],127u);MOD(27,video_params[3],127u);
    case 28:case 29:case 30:case 31: {
        /* REC LFO destinations always address the selected loop track,
         * as do the physical REC knobs and MIDI CC21 pitch control. */
        unsigned which=dest-28u;
        unsigned hi=which==0u?126u:which==2u?6u:127u;
        uint8_t *parameters=m->rec_track?
            m->rec_extra[m->rec_track-1u]:m->seq;
        parameters[which]=mod_u8(parameters[which],depth,hi);
        break;
    }
    MOD(32,arp[1],3u);MOD(33,arp[2],100u);
    MOD(34,arp[3],4u);
    case 35:m->global[0]=(int16_t)clampf((float)m->global[0]+depth*60.f,40.f,240.f);break;
    case 36:m->global[1]=(int16_t)clampf((float)m->global[1]+depth*50.f,0.f,100.f);break;
    case 37:m->global[2]=(int16_t)clampf((float)m->global[2]+depth*50.f,-50.f,50.f);break;
    case 38:mod_fx_extra(m,first,0u,depth);break;
    case 39:mod_fx_extra(m,first,1u,depth);break;
    case 40:mod_fx_extra(m,first,2u,depth);break;
    case 41:mod_fx_extra(m,first,3u,depth);break;
    case 42:mod_fx_extra(m,second,0u,depth);break;
    case 43:mod_fx_extra(m,second,1u,depth);break;
    case 44:mod_fx_extra(m,second,2u,depth);break;
    case 45:mod_fx_extra(m,second,3u,depth);break;
    MOD(46,osc2_octave,4u);MOD(47,osc1_wave,3u);
    MOD(48,osc2_wave,3u);MOD(49,wave,MI_WAVE_PIANO);
    MOD(50,fx,MI_FX_COUNT-1u);MOD(51,fx2,MI_FX_COUNT-1u);
    MOD(52,arp[0],4u);MOD(53,lfo[0],127u);
    MOD(54,lfo[1],3u);MOD(55,lfo[2],127u);
    case 56:m->global[3]=(int16_t)clampf((float)m->global[3]+depth*2.f,0.f,2.f);break;
    case 57:m->rec_bars=depth>=0.f?4u:2u;break;
    default:break;
    }
#undef MOD
    if(dest==28u||dest==29u){
        uint8_t *parameters=m->rec_track?
            m->rec_extra[m->rec_track-1u]:m->seq;
        if(parameters[0]>=parameters[1]){
            if(dest==28u)parameters[0]=parameters[1]>0u?
                         parameters[1]-1u:0u;
            else parameters[1]=parameters[0]<127u?
                         parameters[0]+1u:127u;
        }
    }
    if(m->arp[2]<10u)m->arp[2]=10u;
    if(m->arp[3]<1u)m->arp[3]=1u;
    if(!mi_fx_pair_supported(m->fx,m->fx2)){
        m->fx=first;m->fx2=second;
    }
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
    }else if(mode==MI_FX_SLICER){
        /* SLICER processes the final stereo mix, not the OSC-only FX bus. */
        target=0.f;
    }else if(mode!=MI_FX_OFF&&mode<MI_FX_STORAGE_COUNT){
        wet=mi_fx_dsp_process_ex(&native_fx,mode,dry,a->sample_rate,
                                s->fx_options[mode][0],s->fx_options[mode][1],
                                s->fx_extra[mode]);
    }else target=0.f;
    /* Delay/reverb maximum was too subtle on the FM-1. Increase audible
     * wet return progressively toward high WET settings, but preserve
     * existing low-WET balance and master output soft limiting. */
    if(mode==MI_FX_REVERB)wet*=1.f+0.75f*target*target;
    else if(mode==MI_FX_DELAY)wet*=1.f+0.35f*target*target;
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
    return mi_soft_limit(dry+(wet-dry)*(*smooth));
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
        {
            unsigned prev_grain=(a->last_mode==MI_FX_DREAMY||
                                  a->last_mode2==MI_FX_DREAMY)?1u:
                                 (a->last_mode==MI_FX_PITCHSHIFT||
                                  a->last_mode2==MI_FX_PITCHSHIFT)?2u:0u;
            unsigned new_grain=(first==MI_FX_DREAMY||second==MI_FX_DREAMY)?1u:
                               (first==MI_FX_PITCHSHIFT||second==MI_FX_PITCHSHIFT)?2u:0u;
            if(prev_grain!=new_grain)mi_fx_dsp_reset_grain(&native_fx);
        }
        if((first==MI_FX_REEL||second==MI_FX_REEL)!=
           (a->last_mode==MI_FX_REEL||a->last_mode2==MI_FX_REEL))
            a->wr=a->valid=a->phase=a->segment=0u;
        a->last_mode=(uint8_t)first;a->last_mode2=(uint8_t)second;
        a->wet_current=0.f;a->wet_current2=0.f;
    }
    dry=fx_stage(a,s,first,s->wet,dry,&a->wet_current);
    return fx_stage(a,s,second,s->wet2,dry,&a->wet_current2);
}
void mi_audio_midi_clock(mi_audio_t *a,mi_state_t *s,unsigned status)
{
    if(!a||!s)return;
    if(status==0xFAu){ /* MIDI Start: Recorder clock path unchanged. */
        {unsigned t;
         for(t=0u;t<MI_REC_TRACKS;t++){
             a->recorder.tracks[t].clock_ticks=0u;
             a->recorder.tracks[t].clock_period=0.f;
         }}
        if(s->global[3]!=1)return; /* SEQ CLOCK INTERNAL ignores MIDI transport */
        a->step_midi_mode=1u;
        a->step_clock_phase=0u;
        a->step_clock_pending=0u;
        a->step_active=0u;
        a->step_wait=0u;
        s->step_play=1u;
    }else if(status==0xFBu){ /* Continue: keep next step and pulse phase */
        if(s->global[3]!=1)return;
        a->step_midi_mode=1u;
        s->step_play=1u;
    }else if(status==0xFCu){ /* Stop: preserve next step for Continue */
        if(s->global[3]!=1){
            mi_rec_clock_stop(&a->recorder);
            return;
        }
        s->step_play=0u;
        a->step_clock_pending=0u;
        a->step_midi_mode=1u;
        mi_rec_clock_stop(&a->recorder);
    }else if(status==0xF8u){
        /* The RECORDER continues receiving the identical raw MIDI clock. */
        mi_rec_external_tick(&a->recorder,a->rendered_samples);
        if(s->global[3]!=1)return; /* REC MIDI clock remains unchanged */
        a->step_midi_mode=1u;
        if(s->step_play){
            if(a->step_clock_phase==0u)a->step_clock_pending=1u;
            a->step_clock_phase=(uint8_t)((a->step_clock_phase+1u)%6u);
        }
    }else return;
    a->step_last_clock_sample=a->rendered_samples;
}
void mi_audio_rec_request(mi_audio_t *a,unsigned command)
{
    if(a && (command==1u || command==2u || command==3u))
        a->rec_command=(uint8_t)command;
}
void mi_audio_render(mi_audio_t *a,const mi_state_t *s,int32_t *lr,uint32_t n,uint32_t master_q12)
{
    uint32_t i,k;
    uint8_t scope_selected,scope_audible=0u;
    float attack_step,decay_step,release_samples,sustain,lfo_speed;
    mi_state_t snapshot;
    const mi_state_t *rs;
    if(!a||!s||!lr)return;
    snapshot=*s;
    if(s->lfo[2] && s->lfo[3]>=3u)
        apply_lfo_destination(&snapshot,s->lfo[3],
            lfo_value(s->lfo[1],a->lfo_phase)*(float)s->lfo[2]/127.f);
    rs=&snapshot;
    /* GLO CLOCK is an explicit SEQ source, never a hidden MIDI override.
     * MIDI mode waits for 24-PPQN F8 pulses; INTERNAL uses configured BPM.
     * The RECORDER keeps its existing independent MIDI auto-clock path. */
    if(s->global[3]==1){
        if(!a->step_midi_mode){
            a->step_midi_mode=1u;
            a->step_clock_phase=0u;a->step_clock_pending=0u;
            a->step_active=0u;a->step_wait=0u;
        }
    }else if(a->step_midi_mode){
        a->step_midi_mode=0u;
        a->step_clock_phase=0u;a->step_clock_pending=0u;
        a->step_active=0u;a->step_wait=0u;
    }
    for(k=0u;k<4u;k++)a->video_effective[k]=snapshot.video_params[k];
    float scale=clampf(master_q12/4096.f,0.f,1.f)*clampf(rs->level/127.f,0.f,1.f)*5000000.f;
    uint8_t wave=rs->wave==MI_WAVE_MULTI?rs->osc1_wave:
                 rs->wave>3u?0u:rs->wave;
    uint8_t wave2=rs->osc2_wave>3?0:rs->osc2_wave;
    /* OSC2 tuning = octave (-2..+2) and +/-50 cents fine detune.
     * Bounded 2nd-order exp approximation, no freestanding libm needed. */
    const float fine=((int)rs->osc2_detune-64)*(0.5f/64.f);
    const float ln2x=fine*0.05776226505f;
    float osc2_ratio=1.f+ln2x+0.5f*ln2x*ln2x;
    /* OSC2 is active ONLY when HOME WAVE explicitly selects MULTI. */
    float osc2_mix=rs->wave==MI_WAVE_MULTI?
                   (float)rs->osc2_level/127.f*0.5f:0.f;
    switch(rs->osc2_octave){
    case 0u:osc2_ratio*=0.25f;break;
    case 1u:osc2_ratio*=0.5f;break;
    case 3u:osc2_ratio*=2.f;break;
    case 4u:osc2_ratio*=4.f;break;
    default:break;
    }
    /* Select the oscillator currently being edited. */
    scope_selected=(uint8_t)(rs->wave==MI_WAVE_MULTI&&rs->osc_page==1u);
    if(a->osc_scope_selected!=scope_selected){
        mi_memzero(a->osc_scope,sizeof a->osc_scope);
        a->osc_scope_selected=scope_selected;
    }
    {
        float inc=0.f;
        for(k=0;k<MI_NATIVE_POLY;k++)if(a->v[k].active){
            inc=a->v[k].increment;
            break;
        }
        if(scope_selected)inc*=osc2_ratio;
        if(inc>0.f){
            unsigned step=(unsigned)(2.f/(224.f*inc));
            if(step<1u)step=1u;
            if(step>4u)step=4u;
            a->osc_scope_stride=(uint8_t)step;
        }else a->osc_scope_stride=1u;
    }
    float duty=0.05f+0.9f*(rs->width/127.f);
    float atk=(float)rs->env[0]/127.f,dec=(float)rs->env[1]/127.f;
    float rel=(float)rs->env[3]/127.f;
    sustain=clampf((float)rs->env[2]/127.f,0.f,1.f);
    attack_step=1.f/(a->sample_rate*(0.002f+2.f*atk*atk));
    decay_step=(1.f-sustain)/(a->sample_rate*(0.005f+2.f*dec*dec));
    release_samples=a->sample_rate*(0.005f+3.f*rel*rel);
    lfo_speed=(0.07f+15.f*(float)rs->lfo[0]/127.f)/a->sample_rate;
    mi_rec_select(&a->recorder,rs->rec_track);
    for(k=0u;k<MI_REC_TRACKS;k++)
        mi_rec_set_transport(&a->recorder,k,rs->rec_reverse[k],rs->rec_quant[k]);
    mi_rec_set_track(&a->recorder,0u,rs->seq,rs->rec_mute[0],rs->rec_pan[0]);
    mi_rec_set_track(&a->recorder,1u,rs->rec_extra[0],rs->rec_mute[1],rs->rec_pan[1]);
    mi_rec_set_track(&a->recorder,2u,rs->rec_extra[1],rs->rec_mute[2],rs->rec_pan[2]);
    mi_rec_configure(&a->recorder,(unsigned)rs->global[0],rs->rec_bars,
                     a->sample_rate,a->rendered_samples);
    arp_sync(a,rs);
    /* Postmix SLICER sees all three LOOP outputs and both FMDrum sources.
     * Prefer measured external MIDI period; fall back to internal BPM.
     * The small sample history bounds slices to micro-stutter lengths. */
    unsigned slicer_slot=rs->fx==MI_FX_SLICER?0u:
                         rs->fx2==MI_FX_SLICER?1u:2u;
    float slicer_target=slicer_slot==0u?(float)rs->wet/127.f:
                        slicer_slot==1u?(float)rs->wet2/127.f:0.f;
    float tempo_step=a->sample_rate*15.f/(float)rs->global[0];
    if(a->recorder.tracks[0].clock_active &&
       a->recorder.tracks[0].clock_period>1.f)
        tempo_step=6.f*a->recorder.tracks[0].clock_period;
    /* UI updates only an 8-bit command; the audio thread owns the samples. */
    if(a->rec_command){
        uint8_t cmd=a->rec_command;
        a->rec_command=0u;
        if(cmd==1u)mi_rec_toggle_record(&a->recorder);
        if(cmd==2u)mi_rec_toggle_play(&a->recorder);
        if(cmd==3u)mi_rec_clear(&a->recorder);
    }
    for(i=0;i<n;i++){
        float sample=0.f,drum_manual=0.f,drum_seq=0.f,osc_capture=0.f,lfo,mod,voice_duty=duty;
        mi_step_tick(a,rs);
        arp_tick(a,rs);
        a->lfo_phase=wrap01(a->lfo_phase+lfo_speed);
        lfo=lfo_value(rs->lfo[1],a->lfo_phase);
        mod=lfo*(float)rs->lfo[2]/127.f;
        if(rs->lfo[3]==1u)voice_duty=clampf(duty+mod*0.3f,0.03f,0.97f);
        for(k=0;k<MI_NATIVE_POLY;k++){
            mi_voice_t *v=&a->v[k];
            if(!v->active)continue;
            if(v->drum_type){
                float drum=drum_sample(a,rs,v);
                if(v->drum_from_step)drum_seq+=drum;
                else drum_manual+=drum;
                osc_capture+=drum*0.6f;
                continue;
            }
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
            if(rs->wave==MI_WAVE_PIANO){
                /* Flash-backed Wingie2 SET1 goes through the normal OSC
                 * FILTER, FX and REC capture buses, never through DRUM SEQ.
                 * A short crossfade holds only the note sustain in XIP. */
                float pitch_mod=rs->lfo[3]==0u?
                    clampf(1.f+mod*0.059f,0.8f,1.2f):1.f;
                float value=mi_piano_set1_tick(v->piano_zone,&v->piano_pos,
                                               v->piano_step*pitch_mod);
                float submix=(float)rs->sub/127.f;
                if(submix>0.f)
                    value=value*(1.f-submix)+
                          sin01(v->subphase)*submix;
                if(rs->crush){
                    int bits=16-(int)(rs->crush*13u/127u);
                    float levels=(float)(1u<<bits);
                    value=(float)(int)(value*levels)/levels;
                }
                if(rs->lfo[3]==2u)
                    value*=clampf(1.f-0.5f*(float)rs->lfo[2]/127.f*
                                   (1.f-lfo),0.f,1.f);
                value*=v->piano_gain*v->env*(v->vel/127.f)*0.18f;
                sample+=value;
                osc_capture+=value;
                v->subphase=wrap01(v->subphase+
                                  v->increment*0.5f*pitch_mod);
                if(v->piano_pos>=(float)mi_piano_set1_loop_start[v->piano_zone])
                    v->piano_gain*=0.999985f;
                continue;
            }
            {
                float main=osc(wave,v->phase,voice_duty),sub=sin01(v->subphase);
                float submix=rs->sub/127.f;
                float base=main*(1.f-submix)+sub*submix;
                float value=base;
                /* No unnecessary second waveform evaluation when OSC2 is off.
                 * Preserve the previous OSC1/SUB output exactly by default. */
                if(osc2_mix>0.f){
                    float second=osc(wave2,v->phase2,voice_duty);
                    value=base*(1.f-osc2_mix)+second*osc2_mix;
                    if(scope_selected)osc_capture+=second*osc2_mix*
                        v->env*(v->vel/127.f)*0.18f;
                }
                if(!scope_selected)osc_capture+=main*(1.f-osc2_mix)*
                    v->env*(v->vel/127.f)*0.18f;
                if(rs->crush){
                    int bits=16-(int)(rs->crush*13u/127u);
                    float levels=(float)(1u<<bits);
                    float quant=(float)(int)(value*levels);
                    value=quant/levels;
                }
                if(rs->lfo[3]==2u)
                    value*=clampf(1.f-0.5f*(float)rs->lfo[2]/127.f*(1.f-lfo),0.f,1.f);
                sample+=value*v->env*(v->vel/127.f)*0.18f;
                {
                    float pitch_mod=rs->lfo[3]==0u?clampf(1.f+mod*0.059f,0.8f,1.2f):1.f;
                    v->phase=wrap01(v->phase+v->increment*pitch_mod);
                    if(osc2_mix>0.f)
                        v->phase2=wrap01(v->phase2+v->increment*osc2_ratio*pitch_mod);
                    v->subphase=wrap01(v->subphase+v->increment*0.5f*pitch_mod);
                }
            }
        }
        /* Display-only snapshot of REAL generated oscillator PCM. */
        a->osc_scope[a->osc_scope_head&(MI_OSC_SCOPE_SAMPLES-1u)]=
            (int8_t)(clampf(osc_capture*4.f,-1.f,1.f)*127.f);
        ++a->osc_scope_head;
        if(osc_capture>0.0003f||osc_capture< -0.0003f)
            scope_audible=1u;
        if(rs->filter_cutoff<127u){
            float cut=(float)rs->filter_cutoff/127.f;
            float alpha=0.004f+0.88f*cut*cut;
            float fb=(float)rs->filter_resonance/127.f*0.72f;
            float x=sample-fb*(a->filter_b-a->filter_a);
            a->filter_a+=(x-a->filter_a)*alpha;
            a->filter_b+=(a->filter_a-a->filter_b)*alpha;
            sample=a->filter_b;
        }else{a->filter_a=sample;a->filter_b=sample;}
        sample=mi_soft_limit(sample);
        sample=effect(a,rs,sample); /* effects process OSC only, NEVER FMDrum */
        sample=mi_soft_limit(sample+drum_manual); /* live FMDrum enters REC; sequencer never does */
        /* Existing FX wet-smoothing principle: crossfade the recorder monitor
         * in/out over milliseconds. Simply adding two full-scale signals
         * clipped during layered playback and caused sharp transitions. */
        {
            float rec_l=0.f,rec_r=0.f,play_mix;
            float out_l,out_r;
            mi_rec_tick_stereo(&a->recorder,sample,rs->seq,&rec_l,&rec_r);
            /* A muted REC must not duck the live oscillator and drums. */
            play_mix=mi_rec_any_audible(&a->recorder)?1.f:0.f;
            a->rec_monitor_mix+=(play_mix-a->rec_monitor_mix)*0.0015f;
            /* Each REC track has independent balance pan; the oscillator
             * and manual FMDrum remain centre. Gain and soft limiting are
             * applied after all three playback heads are summed. */
            out_l=mi_soft_limit(sample*(1.f-0.45f*a->rec_monitor_mix)+
                    rec_l*(0.75f*a->rec_monitor_mix)+drum_seq);
            out_r=mi_soft_limit(sample*(1.f-0.45f*a->rec_monitor_mix)+
                    rec_r*(0.75f*a->rec_monitor_mix)+drum_seq);
            {
                float st_l=out_l,st_r=out_r;
                /* The approved FX-OFF path must not spend an extra DSP
                 * call per output sample. Drain only an in-flight crossfade. */
                unsigned slicer_active=(slicer_target>0.0001f||
                                        a->slicer_mix>0.0001f);
                if(slicer_active||a->slicer.active)
                    mi_slicer_tick(&a->slicer,out_l,out_r,tempo_step,
                                   rs->fx_slicer[0],rs->fx_slicer[1],
                                   slicer_active,&st_l,&st_r);
                a->slicer_mix+=(slicer_target-a->slicer_mix)*0.0015f;
                /* OFF is bit-for-bit the old mixer path: do not apply a
                 * second soft limiter when the user has disabled SLICER. */
                if(a->slicer_mix>0.0001f){
                    out_l=mi_soft_limit(out_l+(st_l-out_l)*a->slicer_mix);
                    out_r=mi_soft_limit(out_r+(st_r-out_r)*a->slicer_mix);
                }
            }
            lr[2u*i]=(int32_t)(clampf(out_l,-1.f,1.f)*scale);
            lr[2u*i+1u]=(int32_t)(clampf(out_r,-1.f,1.f)*scale);
        }
    }
    a->osc_scope_active=scope_audible;
    a->rendered_samples+=n;
}
