/* SPDX-License-Identifier: GPL-3.0-only
 * Wingie2 SET1 sample voice. Three original 12kHz mono PCM roots live in
 * XIP flash; only two floats and a zone index are needed per voice.
 * No flash writes, heap allocations, audio pool buffers, or res.bin edits.
 */
#include "mi_piano_set1.h"
float mi_piano_set1_tick(unsigned zone,float *position,float step)
{
    const float end=(float)MI_PIANO_SET1_SAMPLES, fade=192.f;
    const uint8_t *pcm;
    float p,value;
    unsigned i,begin;
    if(!position || zone>=MI_PIANO_SET1_COUNT)return 0.f;
    pcm=mi_piano_set1_data(zone);
    begin=mi_piano_set1_loop_start[zone];
    p=*position;
    if(p<0.f)p=0.f;
    if(p>=end)p=(float)(begin+192u);
    i=(unsigned)p;
    if(i>=MI_PIANO_SET1_SAMPLES-1u)i=MI_PIANO_SET1_SAMPLES-2u;
    value=(float)pcm[i]+((float)pcm[i+1u]-(float)pcm[i])*(p-(float)i);
    if(p>=end-fade){
        float headpos=(float)begin+p-(end-fade);
        unsigned j=(unsigned)headpos;
        float head=(float)pcm[j]+((float)pcm[j+1u]-(float)pcm[j])*
                    (headpos-(float)j);
        float blend=(p-(end-fade))/fade;
        value=value*(1.f-blend)+head*blend;
    }
    if(step<0.f)step=0.f;
    if(step>16.f)step=16.f;
    p+=step;
    if(p>=end)p=(float)(begin+192u)+(p-end);
    if(p>=end)p=(float)(begin+192u);
    *position=p;
    return (value-128.f)*(1.f/128.f);
}
