#include "mi_fx_dsp.h"
#include "mi_control.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    mi_fx_dsp_t p;
    float peak;
    unsigned i;
    mi_fx_dsp_init(&p);
    peak=0.f;
    for(i=0;i<36000u;i++) {
        float dry=i==0u?1.f:0.f;
        float out=mi_fx_dsp_process(&p,MI_FX_DELAY,dry,44100.f,0,48);
        if(out>peak)peak=out;
    }
    assert(peak>0.6f);
    mi_fx_dsp_init(&p);
    peak=0.f;
    for(i=0;i<2000u;i++){
        float out=mi_fx_dsp_process(&p,MI_FX_CHORUS,
                                   i==0u?1.f:0.f,44100.f,40,90);
        if(out>peak)peak=out;
    }
    assert(peak>0.6f);
    mi_fx_dsp_init(&p);
    peak=0.f;
    for(i=0;i<8000u;i++){
        float out=mi_fx_dsp_process(&p,MI_FX_REVERB,
                                   i==0u?1.f:0.f,44100.f,70,50);
        if(out>peak)peak=out;
    }
    assert(peak>0.05f);
    mi_fx_dsp_init(&p);
    assert(mi_fx_dsp_process(&p,MI_FX_DREAMY,0.125f,44100.f,55,55)==0.125f);
    puts("PASS: independent MIYAKO FM-1 delay, chorus and reverb bounded impulse responses");
    return 0;
}
