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
        /* CI #264: an impulse MUST appear at the exact 40ms boundary.
         * delay_valid>n skipped that one and only source sample. */
        if(i<1764u)assert(out==0.f);
        if(i==1764u)assert(out>0.99f);
        if(out>peak)peak=out;
    }
    assert(peak>0.6f);
    /* Maximum DELAY must now make a substantially stronger second repeat:
     * after the first 40ms echo, the next repeat returns ~96% amplitude. */
    mi_fx_dsp_init(&p);
    for(i=0u;i<=3528u;i++){
        float out=mi_fx_dsp_process(&p,MI_FX_DELAY,
                                    i==0u?1.f:0.f,44100.f,0u,127u);
        if(i==3528u)assert(out>0.94f && out<0.98f);
    }
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
    /* EUREKA: two seconds at 14.7kHz from existing DELAY memory. */
    mi_fx_dsp_reset_shared(&p);
    assert(p.eureka_valid==0u && p.delay_valid==0u);
    peak=0.f;
    for(i=0;i<110000u;i++){
        float dry=(i%400u)<200u?0.65f:-0.65f;
        float out=mi_fx_dsp_process(&p,MI_FX_EUREKA,dry,44100.f,30,75);
        assert(out>=-1.f&&out<=1.f);
        if(i>32000u && out>peak)peak=out;
    }
    assert(p.eureka_valid==29400u);
    assert(peak>0.02f);
    mi_fx_dsp_reset_shared(&p);
    assert(p.eureka_valid==0u && p.delay_valid==0u);
    /* Real BITCRUSH: hold multiple output samples and reduce bit depth. */
    mi_fx_dsp_init(&p);
    {
        float a=mi_fx_dsp_process(&p,MI_FX_BITCRUSH,0.6f,44100.f,0u,127u);
        float b=mi_fx_dsp_process(&p,MI_FX_BITCRUSH,-0.6f,44100.f,0u,127u);
        assert(a==b && a>0.f && a<0.6f);
    }
    /* Real +/-12-semitone grain shifter, reusing existing DREAMY RAM. */
    mi_fx_dsp_reset_grain(&p);
    peak=0.f;
    for(i=0u;i<40000u;i++){
        float dry=(i%149u)<50u?0.7f:-0.7f;
        float out=mi_fx_dsp_process(&p,MI_FX_PITCHSHIFT,dry,
                                    44100.f,96u,72u);
        assert(out>=-1.f&&out<=1.f);
        if(i>22000u && out>peak)peak=out;
    }
    assert(peak>0.01f);
    puts("PASS: EUREKA shared ring, DREAMY, BITCRUSH, PITCHSHIFT, DELAY/CHORUS/REVERB");
    return 0;
}
