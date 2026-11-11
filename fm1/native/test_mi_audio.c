#include "mi_audio.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
 mi_audio_t a;mi_state_t s;int32_t lr[512];int32_t peaks=0;unsigned i,j;
 mi_init(&s);mi_audio_init(&a,44100.f);
 mi_audio_note_on(&a,69,127);
 for(i=0;i<80;i++){mi_audio_render(&a,&s,lr,256,4096);
     for(j=0;j<256;j++)if(lr[2*j]>100||lr[2*j]<-100)peaks++;
 }
 assert(peaks>1000);
 assert(a.v[0].active && a.v[0].held);
 mi_audio_note_off(&a,69);
 for(i=0;i<250;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(!a.v[0].active);
 s.fx=MI_FX_REEL;s.wet=127;
 mi_audio_note_on(&a,60,120);
 for(i=0;i<15;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.wr>0&&a.valid>0);
 s.fx=MI_FX_DELAY;
 for(i=0;i<150;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.last_mode==MI_FX_DELAY);
 s.fx=MI_FX_CHORUS;
 for(i=0;i<15;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.last_mode==MI_FX_CHORUS);
 s.fx=MI_FX_REVERB;
 for(i=0;i<15;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.last_mode==MI_FX_REVERB);
 s.fx=MI_FX_DREAMY; /* native dual-grain FX now active */
 for(i=0;i<4;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.last_mode==MI_FX_DREAMY);
 mi_audio_panic(&a);
 for(i=0;i<MI_NATIVE_POLY;i++)assert(!a.v[i].active);
 /* All eight slots full: the ninth note must steal the oldest held voice.
  * Keep the test parameterized by MI_NATIVE_POLY for a measured fallback. */
 {
   static const uint8_t chord[9]={60,62,64,65,67,69,71,72,74};
   for(i=0;i<MI_NATIVE_POLY;i++)mi_audio_note_on(&a,chord[i],120);
   for(i=0;i<MI_NATIVE_POLY;i++)
     assert(a.v[i].active&&a.v[i].held&&a.v[i].note==chord[i]);
   mi_audio_note_on(&a,chord[MI_NATIVE_POLY],120);
   assert(a.v[0].note==chord[MI_NATIVE_POLY]);
   for(i=1;i<MI_NATIVE_POLY;i++)assert(a.v[i].note==chord[i]);
   /* A released voice is stolen before an actively held one. */
   mi_audio_note_off(&a,chord[2]);
   mi_audio_note_on(&a,76,120);
   assert(a.v[2].note==76&&a.v[2].active&&a.v[2].held);
 }
 mi_audio_panic(&a);
 for(i=0;i<MI_NATIVE_POLY;i++)assert(!a.v[i].active);
 /* Arpeggiator must start on first sample and advance at 1/16-note BPM. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.arp[0]=1;s.arp[1]=2;s.arp[2]=70;s.arp[3]=1;
 mi_audio_input_on(&a,&s,60,127);
 mi_audio_input_on(&a,&s,60,127); /* same note from USB + physical key */
 mi_audio_input_on(&a,&s,64,90);
 assert(a.held_total==2);
 mi_audio_render(&a,&s,lr,256,4096);
 assert(a.arp_mode==1 && a.arp_sounding && a.arp_note==60);
 /* Exact audio note attacks, not a separately advanced visual timer. */
 assert(a.arp_visual_frame==0u && a.arp_visual_event>=2u);
 for(i=0;i<35;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.arp_note==64 && a.arp_visual_frame==1u);
 mi_audio_input_off(&a,&s,60);
 assert(a.held_total==2); /* still held by second input */
 mi_audio_input_off(&a,&s,60);
 assert(a.held_total==1);
 mi_audio_input_off(&a,&s,64);
 mi_audio_render(&a,&s,lr,256,4096);
 assert(!a.arp_sounding);
 /* Down mode starts with the highest held note; switching modes is safe. */
 s.arp[0]=2;
 mi_audio_input_on(&a,&s,60,127);
 mi_audio_input_on(&a,&s,67,127);
 mi_audio_render(&a,&s,lr,256,4096);
 assert(a.arp_mode==2&&a.arp_note==67);
 assert(a.arp_visual_frame==0u); /* mode change restarts intro */
 {
     uint32_t event=a.arp_visual_event;
     a.arp_visual_frame=161u; /* exercise exact carousel loop bound */
     a.arp_clock=0u;
     mi_audio_render(&a,&s,lr,256,4096);
     assert(a.arp_visual_frame==81u && a.arp_visual_event==event+1u);
 }
 mi_audio_input_off(&a,&s,60);
 mi_audio_input_off(&a,&s,67);
 s.arp[0]=0;
 mi_audio_render(&a,&s,lr,256,4096);
 assert(a.arp_mode==0 && a.arp_visual_frame==0u);
 mi_audio_panic(&a);
 /* ADSR attack is no longer a fixed 0.002 per sample. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.env[0]=127;
 mi_audio_note_on(&a,69,127);
 mi_audio_render(&a,&s,lr,256,4096);
 assert(a.v[0].env>0.f && a.v[0].env<0.02f);
 /* LFO phase advances even with no notes: shared oscillator is live. */
 s.lfo[0]=90;s.lfo[1]=1;s.lfo[2]=80;
 float before=a.lfo_phase;
 mi_audio_render(&a,&s,lr,256,4096);
 assert(a.lfo_phase!=before);
 /* Two distinct FX slots run serially, each with its own wet amount. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.fx=MI_FX_CHORUS;s.wet=120;
 s.fx2=MI_FX_REVERB;s.wet2=100;
 mi_audio_note_on(&a,60,127);
 for(i=0;i<50u;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.last_mode==MI_FX_CHORUS&&a.last_mode2==MI_FX_REVERB);
 assert(a.wet_current>0.f&&a.wet_current2>0.f);
 /* A clip recording command is consumed only on an audio block boundary. */
 mi_audio_rec_request(&a,1u);
 mi_audio_render(&a,&s,lr,256,4096);
 assert(a.recorder.recording && a.recorder.count>=40u &&
        a.recorder.count<=55u);
 mi_audio_rec_request(&a,1u);
 mi_audio_render(&a,&s,lr,256,4096);
 assert(!a.recorder.recording && a.recorder.playing &&
        a.recorder.count>=40u && a.recorder.count<=55u);
 mi_audio_rec_request(&a,2u);
 mi_audio_render(&a,&s,lr,256,4096);
 assert(!a.recorder.playing);
 mi_audio_rec_request(&a,2u);
 mi_audio_render(&a,&s,lr,256,4096);
 assert(a.recorder.playing);
 /* Test captured PCM, silence, and selected MULTI OSC2. */
 {
   unsigned nonzero=0u;
   mi_init(&s);mi_audio_init(&a,44100.f);
   s.wave=0u;s.env[0]=0u;s.env[1]=0u;s.env[2]=127u;
   mi_audio_render(&a,&s,lr,256,4096);
   assert(!a.osc_scope_active&&a.osc_scope_head==256u);
   mi_audio_note_on(&a,69,127);
   for(i=0;i<8u;i++)mi_audio_render(&a,&s,lr,256,4096);
   assert(a.osc_scope_active&&a.osc_scope_head==9u*256u);
   for(i=0;i<MI_OSC_SCOPE_SAMPLES;i++)if(a.osc_scope[i])++nonzero;
   assert(nonzero>150u);
   s.wave=MI_WAVE_MULTI;s.osc1_wave=0u;s.osc2_wave=2u;
   s.osc_page=1u;s.osc2_level=127u;
   mi_audio_render(&a,&s,lr,256,4096);
   assert(a.osc_scope_selected==1u&&a.osc_scope_active);
   mi_audio_panic(&a);
   mi_audio_render(&a,&s,lr,256,4096);
   assert(!a.osc_scope_active);
 }
 /* OSC2 changes actual PCM while preserving the selected polyphony. */
 {
   int32_t first_old,first_new;
   mi_init(&s);mi_audio_init(&a,44100.f);
   s.env[0]=0u;s.env[1]=0u;s.env[2]=127u;
   mi_audio_note_on(&a,69,127);
   for(i=0;i<8u;i++)mi_audio_render(&a,&s,lr,256,4096);
   first_old=lr[128];
   assert(a.v[0].phase2>=0.f&&a.v[0].phase2<1.f);
   mi_audio_panic(&a);
   mi_audio_init(&a,44100.f);
   s.wave=MI_WAVE_MULTI;s.osc1_wave=0u;
   s.osc2_wave=1u;s.osc2_level=127u;
   s.osc2_detune=81u;s.osc2_octave=3u;
   mi_audio_note_on(&a,69,127);
   for(i=0;i<8u;i++)mi_audio_render(&a,&s,lr,256,4096);
   first_new=lr[128];
   assert(first_old!=first_new);
   assert(a.v[0].phase2>=0.f&&a.v[0].phase2<1.f);
   assert(a.v[0].phase2!=a.v[0].phase);
   assert(a.v[0].active);
 }
 /* Eight simultaneous voices through MULTI OSC and two FX: functional
  * smoke only. Host tests cannot establish the FM-1 real-time CPU budget. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.wave=MI_WAVE_MULTI;s.osc1_wave=1u;s.osc2_wave=2u;
 s.osc2_level=64u;
 s.fx=MI_FX_DREAMY;s.wet=90u;
 s.fx2=MI_FX_REVERB;s.wet2=90u;
 for(i=0;i<MI_NATIVE_POLY;i++)mi_audio_note_on(&a,(uint8_t)(60u+i),100u);
 for(i=0;i<16u;i++)mi_audio_render(&a,&s,lr,256,4096);
 for(i=0;i<MI_NATIVE_POLY;i++)assert(a.v[i].active&&a.v[i].held);
 assert(a.last_mode==MI_FX_DREAMY&&a.last_mode2==MI_FX_REVERB);
 /* Six distinct FM synthesized drum voices mapped across consecutive keys.
  * A second group changes timbre via variant, without needing samples. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.wave=MI_WAVE_FMDRUM;
 for(i=0u;i<6u;i++){
     mi_audio_input_on(&a,&s,(uint8_t)(53u+i),127u);
     assert(a.v[i].active && a.v[i].drum_type==(uint8_t)(i+1u));
 }
 for(i=0u;i<4u;i++)mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.osc_scope_active);
 /* OCT DOWN must preserve Kick/Snare/Closed/Open/Crash/Tom order.
  * Physical first key becomes MIDI 41, then 29 and 17 in lower octaves. */
 mi_audio_panic(&a);
 for(i=0u;i<6u;i++){
     mi_audio_input_on(&a,&s,(uint8_t)(41u+i),127u);
     assert(a.v[i].drum_type==(uint8_t)(i+1u));
 }
 mi_audio_panic(&a);
 for(i=0u;i<6u;i++){
     mi_audio_input_on(&a,&s,(uint8_t)(17u+i),127u);
     assert(a.v[i].drum_type==(uint8_t)(i+1u));
 }
 mi_audio_panic(&a);
 /* FILTER reduces high-frequency sample movement but does not silence the
  * signal or change the saved source oscillator waveform. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.filter_cutoff=32u;s.filter_resonance=70u;
 s.wave=MI_WAVE_MULTI;s.osc1_wave=1u;s.osc2_wave=2u;
 s.osc2_level=80u;
 mi_audio_note_on(&a,64u,127u);
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(s.filter_cutoff==32u && a.osc_scope_active);
 /* LFO target must reach DSP rather than mutate saved control values. */
 s.lfo[2]=100u;s.lfo[3]=5u;
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(s.filter_cutoff==32u);
 s.lfo[3]=24u; s.video_params[0]=30u;
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(s.video_params[0]==30u);
 assert(a.video_effective[0]!=30u || a.lfo_phase==0.f);
 /* Dedicated Bitcrusher and Pitchshift are real stage selections. */
 s.lfo[2]=0u;s.fx2=MI_FX_OFF;
 s.fx=MI_FX_BITCRUSH;s.wet=127u;
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.last_mode==MI_FX_BITCRUSH);
 s.fx=MI_FX_PITCHSHIFT;
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.last_mode==MI_FX_PITCHSHIFT);
 /* Confirm the actual recorder input contains FMDrum PCM, not silence.
  * Recording picks up the same post-FX internal bus as regular OSC voices. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.wave=MI_WAVE_FMDRUM;s.fx=MI_FX_OFF;s.fx2=MI_FX_OFF;
 mi_audio_rec_request(&a,1u);
 mi_audio_input_on(&a,&s,53u,127u); /* kick */
 mi_audio_input_on(&a,&s,54u,127u); /* snare */
 for(i=0u;i<16u;i++)mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.recorder.recording && a.recorder.count>200u);
 {
     unsigned audible=0u;
     for(i=0u;i<a.recorder.count;i++){
         int16_t pcm=mi_rec_sample(i);
         if(pcm>64 || pcm< -64)++audible;
     }
     assert(audible>20u);
 }
 mi_audio_input_off(&a,&s,53u);
 mi_audio_input_off(&a,&s,54u);
 mi_audio_rec_request(&a,1u);
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(!a.recorder.recording && a.recorder.playing &&
        a.recorder.count>200u);
 /* The FMDrum sequencer plays on the output only. Live FMDrum is recorded. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.wave=0u; /* live OSC stays SINE; sequencer still uses FMDrum */
 s.step_pattern[0]=s.step_pattern[1]=
     s.step_pattern[2]=s.step_pattern[3]=0x0001u;
 s.step_play=1u;s.global[0]=120;
 mi_audio_rec_request(&a,1u);
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_active && a.step_current==0u && a.recorder.recording);
 assert(a.v[0].drum_type==1u && a.v[1].drum_type==2u &&
        a.v[2].drum_type==3u && a.v[3].drum_type==6u);
 {
     unsigned audible=0u;
     for(i=0u;i<a.recorder.count;i++){
         int16_t pcm=mi_rec_sample(i);
         if(pcm>64 || pcm< -64)++audible;
     }
     assert(audible==0u); /* no sequencer audio in Recorder */
 }
 {
     unsigned audible_output=0u,j;
     for(j=0u;j<512u;j++)if(lr[j]!=0)++audible_output;
     assert(audible_output>5u); /* sequencer still sounds normally */
 }
 for(i=0u;i<22u;i++)mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_current==1u);
 s.step_play=0u;
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(!a.step_active);
 s.step_play=1u;s.step_dir=1u;
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_current==7u);
 s.step_dir=2u;s.step_len=16u;
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_current<16u);
 /* MIDI Start must arm but NOT free-run before external Clock.
  * 24 PPQN: 6 F8 pulses per 1/16 note; Stop/Continue preserve position. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.step_pattern[0]=0xFFFFu;
 s.global[0]=120;
 s.global[3]=1; /* GLO CLOCK=MIDI explicitly enables sequencer transport */
 mi_audio_midi_clock(&a,&s,0xFAu);
 assert(s.step_play && a.step_midi_mode && a.step_clock_phase==0u);
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_active && a.step_next==0u && !a.v[0].active);
 mi_audio_midi_clock(&a,&s,0xF8u);
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_current==0u && a.step_next==1u &&
        a.step_clock_phase==1u && a.v[0].drum_type==1u);
 for(i=0u;i<5u;i++){
     mi_audio_midi_clock(&a,&s,0xF8u);
     mi_audio_render(&a,&s,lr,256u,4096u);
 }
 assert(a.step_current==0u && a.step_clock_phase==0u);
 mi_audio_midi_clock(&a,&s,0xF8u);
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_current==1u && a.step_next==2u);
 mi_audio_midi_clock(&a,&s,0xFCu);
 assert(!s.step_play && a.step_next==2u);
 mi_audio_render(&a,&s,lr,256u,4096u);
 mi_audio_midi_clock(&a,&s,0xF8u); /* Clock while stopped: no step */
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(!s.step_play && a.step_next==2u && a.step_current==1u);
 /* STOP must preserve next step and fractional clock phase even after a
  * long pause; Continue is not equivalent to Start. */
 a.rendered_samples+=(uint32_t)(a.sample_rate*2.f);
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_midi_mode && !s.step_play &&
        a.step_next==2u && a.step_clock_phase==1u);
 mi_audio_midi_clock(&a,&s,0xFBu);
 assert(s.step_play && a.step_next==2u);
 for(i=0u;i<6u;i++){
     mi_audio_midi_clock(&a,&s,0xF8u);
     mi_audio_render(&a,&s,lr,256u,4096u);
 }
 assert(a.step_current==2u && a.step_next==3u);
 /* MIDI Start resets even a previously continued pattern. Reverse
  * transport begins on the last enabled step, not step zero. */
 s.step_dir=1u;s.step_len=16u;
 mi_audio_midi_clock(&a,&s,0xFAu);
 mi_audio_midi_clock(&a,&s,0xF8u);
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_current==15u && a.step_next==14u);
 /* Explicit MIDI mode must not silently free-run when the master pauses. */
 a.step_last_clock_sample=0u;
 a.rendered_samples=(uint32_t)a.sample_rate+2000u;
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.step_midi_mode && s.step_play && a.step_current==15u);
 /* GLO CLOCK=INTERNAL really selects the internal BPM engine. */
 s.global[3]=0;
 mi_audio_render(&a,&s,lr,256u,4096u);
 assert(!a.step_midi_mode && s.step_play && a.step_current==15u);
 /* FMDrum input bypasses the entire synth FILTER/FX chain but is still
  * mixed to output; identical dry audio must result with REVERB on/off. */
 {
    int32_t dry[512];
    unsigned j;
    mi_init(&s);mi_audio_init(&a,44100.f);
    s.wave=MI_WAVE_FMDRUM;
    s.fx=MI_FX_OFF;
    mi_audio_input_on(&a,&s,53u,120u);
    mi_audio_render(&a,&s,lr,256u,4096u);
    for(j=0u;j<512u;j++)dry[j]=lr[j];
    mi_audio_init(&a,44100.f);
    s.fx=MI_FX_REVERB;s.wet=127u;
    s.filter_cutoff=10u;
    mi_audio_input_on(&a,&s,53u,120u);
    mi_audio_render(&a,&s,lr,256u,4096u);
    for(j=0u;j<512u;j++)assert(lr[j]==dry[j]);
 }
 /* SLICER is a selectable FX, but unlike the OSC-only FX chain it sees
  * both full stereo channels AFTER the REC and sequenced FMDrum buses. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.fx=MI_FX_SLICER;s.wet=127u;
 s.fx_slicer[0]=127u;s.fx_slicer[1]=127u;
 mi_audio_note_on(&a,60u,127u);
 for(i=0u;i<65u;i++)mi_audio_render(&a,&s,lr,256u,4096u);
 assert(a.slicer.active&&a.slicer.valid==MI_SLICER_FRAMES);
 assert(a.slicer_mix>0.90f&&a.slicer.span>=32u);
 s.fx=MI_FX_OFF;
 for(i=0u;i<45u;i++)mi_audio_render(&a,&s,lr,256u,4096u);
 assert(!a.slicer.active&&!a.slicer.valid);
 assert(a.slicer_mix<0.001f);
 /* Original Wingie2 SET1 piano must sound on the normal OSC bus and
  * record into the selected 3-track PCM7 recorder without a new bus. */
 {
    unsigned audible=0u;
    mi_init(&s);mi_audio_init(&a,44100.f);
    s.wave=MI_WAVE_PIANO;
    s.fx=MI_FX_OFF;s.fx2=MI_FX_OFF;
    mi_audio_rec_request(&a,1u);
    mi_audio_input_on(&a,&s,51u,127u);
    assert(a.v[0].piano_zone==0u);
    for(i=0u;i<15u;i++){
        mi_audio_render(&a,&s,lr,256u,4096u);
        for(j=0u;j<512u;j++)if(lr[j]!=0)++audible;
    }
    assert(audible>1000u && a.recorder.tracks[0].count>100u);
    mi_audio_rec_request(&a,1u);
    mi_audio_render(&a,&s,lr,256u,4096u);
    assert(a.recorder.tracks[0].playing);
    mi_audio_input_off(&a,&s,51u);
    mi_audio_panic(&a);
    mi_audio_input_on(&a,&s,58u,120u);
    assert(a.v[0].piano_zone==1u);
    mi_audio_panic(&a);
    mi_audio_input_on(&a,&s,65u,120u);
    assert(a.v[0].piano_zone==2u);
 }
 /* ARP K2 timing is calculated by the audio sample clock, not by
  * animation/UI ticks: 120 BPM -> 1/4, 1/8, 1/16, 1/32 in order. */
 for(i=0u;i<4u;i++){
     uint32_t period=22050u>>i;
     mi_init(&s);mi_audio_init(&a,44100.f);
     s.global[0]=120;s.arp[0]=1u;s.arp[1]=(uint8_t)i;
     mi_audio_input_on(&a,&s,60u,127u);
     mi_audio_input_on(&a,&s,64u,127u);
     mi_audio_render(&a,&s,lr,256u,4096u);
     assert(a.arp_note==60u&&a.arp_pos==1u);
     assert(a.arp_clock==period-256u);
     for(j=256u;j<period;j+=256u)
         mi_audio_render(&a,&s,lr,256u,4096u);
     assert(a.arp_pos==2u && a.arp_note==64u);
 }
 puts("PASS: FMDrum live REC vs SEQ; 3x REC, SLICER and Wingie2 SET1 PIANO");
 return 0;
}
