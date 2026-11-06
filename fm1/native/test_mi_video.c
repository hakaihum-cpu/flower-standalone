#include "mi_video.h"
#include <stdio.h>
#include <assert.h>
int main(void)
{
 mi_video_t v;
 mi_video_init(&v,120);
 assert(v.effects_frame==0&&v.eureka_frame==-1);
 mi_video_tick(&v,0.f,1);
 int initial=v.eureka_frame;
 assert(initial>=0&&initial<576);
 for(int i=0;i<100;i++){mi_video_tick(&v,0.f,1);assert(v.eureka_frame==initial);}
 mi_video_tick(&v,0.5f,1);
 assert(v.eureka_frame>=0&&v.eureka_frame<576);
 assert(v.effects_frame>=0&&v.effects_frame<120);
 int effects=v.effects_frame;
 mi_video_tick(&v,0.f,0);
 assert(v.effects_frame==effects);
 mi_video_tick(&v,0.6f,0);
 assert(v.effects_frame>=0&&v.effects_frame<120);
 mi_video_init(&v,0);
 mi_video_tick(&v,1.f,0);
 assert(v.effects_frame==-1);
 puts("PASS: independent original-EFFECTS and EUREKA audio reactive frame selectors");
 return 0;
}
