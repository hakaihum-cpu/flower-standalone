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
 for(i=0;i<35;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.arp_note==64);
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
 mi_audio_input_off(&a,&s,60);
 mi_audio_input_off(&a,&s,67);
 s.arp[0]=0;
 mi_audio_render(&a,&s,lr,256,4096);
 assert(a.arp_mode==0);
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
 puts("PASS: eight voices, FMDrum, filter, LFO routing, FX, adaptive REC");
 return 0;
}
