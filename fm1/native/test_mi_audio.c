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
 for(i=0;i<4;i++)assert(!a.v[i].active);
 /* All four slots full: fifth note must steal the OLDEST, not newest. */
 mi_audio_note_on(&a,60,120);
 mi_audio_note_on(&a,62,120);
 mi_audio_note_on(&a,64,120);
 mi_audio_note_on(&a,65,120);
 mi_audio_note_on(&a,67,120);
 assert(a.v[0].note==67 && a.v[1].note==62 &&
        a.v[2].note==64 && a.v[3].note==65);
 /* A released voice is stolen before an actively held one. */
 mi_audio_note_off(&a,64);
 mi_audio_note_on(&a,69,120);
 assert(a.v[2].note==69);
 mi_audio_panic(&a);
 for(i=0;i<4;i++)assert(!a.v[i].active);
 /* Arpeggiator must start on first sample and advance at 1/16-note BPM. */
 mi_init(&s);mi_audio_init(&a,44100.f);
 s.arp[0]=1;s.arp[1]=2;s.arp[2]=70;s.arp[3]=1;
 mi_audio_input_on(&a,&s,60,127);
 mi_audio_input_on(&a,&s,64,90);
 mi_audio_render(&a,&s,lr,256,4096);
 assert(a.arp_mode==1 && a.arp_sounding && a.arp_note==60);
 for(i=0;i<35;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.arp_note==64);
 mi_audio_input_off(&a,&s,60);
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
 puts("PASS: 4-voice synth ADSR LFO MIDI/keys arpeggiator and native FX");
 return 0;
}
