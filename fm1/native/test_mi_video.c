#include "mi_video.h"
#include <stdio.h>
#include <assert.h>
int main(void)
{
 mi_video_t v;
 mi_video_init(&v,144);
 assert(v.effects_frame==0);
 for(int i=0;i<50;i++)mi_video_tick(&v,0.f,0);
 assert(v.effects_frame==0); /* silence must freeze video */
 mi_video_tick(&v,0.5f,0);
 assert(v.effects_frame>=0&&v.effects_frame<144);
 int changed=v.effects_frame;
 mi_video_tick(&v,0.f,0);
 assert(v.effects_frame==changed);
 mi_video_tick(&v,0.6f,0);
 assert(v.effects_frame>=0&&v.effects_frame<144);
 mi_video_init(&v,0);
 mi_video_tick(&v,1.f,0);
 assert(v.effects_frame==-1);
 puts("PASS: MIYAKO EFFECTS source video only; audio-reactive 144 frame scheduler");
 return 0;
}
