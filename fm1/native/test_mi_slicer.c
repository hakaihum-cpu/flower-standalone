/* Host-only: stereo separation, silence/bypass, random retriggers,
 * bounded history, and a tempo-derived 32..2048-sample slice. */
#include "mi_slicer.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    mi_slicer_t x;
    float left=0.f,right=0.f;
    unsigned i,changed=0u;
    mi_slicer_init(&x);
    assert(x.valid==0u&&!x.active);
    mi_slicer_tick(&x,0.3f,-0.4f,2048.f,127u,127u,0u,&left,&right);
    assert(left==0.3f&&right==-0.4f&&x.valid==0u);
    for(i=0u;i<25000u;i++){
        mi_slicer_tick(&x,0.3f,-0.4f,2048.f,127u,127u,1u,&left,&right);
        assert(left>=-1.f&&left<=1.f&&right>=-1.f&&right<=1.f);
        assert(x.valid<=MI_SLICER_FRAMES&&x.span>=32u&&
               x.span<=MI_SLICER_FRAMES/2u);
        if(x.holding)++changed;
    }
    assert(changed>100u);
    /* Small MIDI period jitter must not reset the active slice; large
     * tempo changes are intentionally applied on the next sample. */
    {
        uint32_t before=x.phase;
        mi_slicer_tick(&x,0.3f,-0.4f,2054.f,127u,127u,1u,&left,&right);
        assert(x.phase==(before+1u)%x.span);
        mi_slicer_tick(&x,0.3f,-0.4f,3200.f,127u,127u,1u,&left,&right);
        assert(x.phase==1u && x.span==800u);
    }
    mi_slicer_tick(&x,0.f,0.f,2048.f,127u,127u,0u,&left,&right);
    assert(left==0.f&&right==0.f&&x.valid==0u);
    mi_slicer_init(&x);
    for(i=0u;i<15000u;i++){
        mi_slicer_tick(&x,0.2f,-0.2f,2400.f,0u,0u,1u,&left,&right);
        assert(left==0.2f&&right==-0.2f);
    }
    puts("PASS: stereo micro-slicer bypass, bounded history, tempo and random hold");
    return 0;
}
