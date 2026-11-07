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
 assert(v.eureka_clip==-1);
 mi_video_tick(&v,0.f,1); /* source chooses the first original 36-frame clip */
 assert(v.eureka_frame>=0 && v.eureka_frame<576);
 assert((v.eureka_frame%36)==0);
 int initial=v.eureka_frame;
 for(int k=0;k<50;k++)mi_video_tick(&v,0.f,1);
 assert(v.eureka_frame==initial); /* no autoplay while idle */
 mi_video_tick(&v,0.8f,1);
 assert(v.eureka_frame>=0&&v.eureka_frame<576);
 int eureka_at=v.eureka_frame;
 for(int k=0;k<40;k++)mi_video_tick(&v,0.8f,1);
 assert(v.eureka_frame==eureka_at); /* held level does NOT animate */
 for(int k=0;k<50;k++)mi_video_tick(&v,0.f,1);
 assert(v.eureka_frame==eureka_at); /* silence freezes the current image */
 mi_video_tick(&v,0.35f,1); /* real activity change moves the playhead */
 assert(v.eureka_frame>=0 && v.eureka_frame<576);
 assert(v.eureka_previous==0.35f);
 /* EFFECTS must not share EUREKA's previous activity or 36-frame playhead. */
 int eureka_after=v.eureka_frame;
 mi_video_tick(&v,0.5f,0);
 assert(v.eureka_frame==eureka_after && v.eureka_previous==0.f);
 assert(v.effects_frame>=0 && v.effects_frame<144);
 mi_video_init(&v,0);
 mi_video_tick(&v,1.f,0);
 assert(v.effects_frame==-1);
 puts("PASS: 144 EFFECTS random frames, 576 EUREKA original 36-frame dynamics, both freeze on silence");
 return 0;
}
