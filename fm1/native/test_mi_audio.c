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
 for(i=0;i<16;i++)mi_audio_render(&a,&s,lr,256,4096);
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
 s.fx=MI_FX_DREAMY; /* untouched until original DSP port is ready */
 for(i=0;i<4;i++)mi_audio_render(&a,&s,lr,256,4096);
 assert(a.last_mode==MI_FX_DREAMY);
 mi_audio_panic(&a);
 for(i=0;i<4;i++)assert(!a.v[i].active);
 puts("PASS: native 4-voice oscillator, Reel/Delay/Reverb/Chorus and honest Dreamy state");
 return 0;
}
