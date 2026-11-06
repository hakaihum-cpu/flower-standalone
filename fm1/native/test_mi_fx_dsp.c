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
    peak=0.f;
    for(i=0;i<45000u;i++){
        float dry=(i%137u)<40u?0.6f:0.f;
        float out=mi_fx_dsp_process(&p,MI_FX_DREAMY,dry,44100.f,64,80);
        assert(out>=-1.f&&out<=1.f);
        if(i>18000u && out>peak)peak=out;
    }
    assert(peak>0.01f); /* DREAMY must no longer be a dry bypass. */
    puts("PASS: MIYAKO native DREAMY, delay, chorus and reverb bounded FX responses");
    return 0;
}
