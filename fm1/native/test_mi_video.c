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
 mi_video_init(&v,144);
 mi_video_set_eureka(&v,576);
 assert(v.eureka_frame==0&&v.eureka_count==576);
 for(int k=0;k<50;k++)mi_video_tick(&v,0.f,1);
 assert(v.eureka_frame==0); /* silence must also freeze EUREKA */
 mi_video_tick(&v,0.8f,1);
 assert(v.eureka_frame>=0&&v.eureka_frame<576);
 int eureka_at=v.eureka_frame;
 for(int k=0;k<50;k++)mi_video_tick(&v,0.f,1);
 assert(v.eureka_frame==eureka_at);
 mi_video_init(&v,0);
 mi_video_tick(&v,1.f,0);
 assert(v.effects_frame==-1);
 puts("PASS: 144 EFFECTS and 576 EUREKA source video frames freeze on silence");
 return 0;
}
