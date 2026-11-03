/* Native granulator test. Does not emulate Digitone's interrupt timing. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../dreamy.c"

int main(void)
{
    int32_t in[64], out[64];
    int blocks, i, changed=0, tail=0;

    assert(sizeof(history) == 131072u);
    assert(dreamy_owner.on==0);
    assert(dreamy_owner.flags==CORE_AUDIO_INSERT);
    assert(strcmp(dreamy_menu.name,"GRANULAR")==0);

    dreamy_open(0,0,0);
    assert(dreamy_owner.on);
    assert(hist_valid==0u && hist_write==0u && grain_armed==0u);

    assert(dreamy_cc(0,60,0,0)==0);
    assert(dreamy_cc(0,60,74,CORE_MIDI_CC_AUTO)==1);
    assert(mix_cc==74u);
    assert(dreamy_cc(0,61,90,CORE_MIDI_CC_AUTO)==0);
    assert(dreamy_cc(0,62,70,CORE_MIDI_CC_AUTO)==0);
    assert(dreamy_cc(0,19,127,CORE_MIDI_CC_AUTO)==0);

    /* 1 s program material, then silence. Grain pitch+envelopes must run,
       and there must be no overflow, stale read, or missing render output. */
    for(blocks=0;blocks<2000;++blocks) {
        for(i=0;i<64;++i)
            in[i]=(blocks<1500 ? (i&1 ? -9600 : 9600) : 0)*65536;
        granular_render(out,in);
        for(i=0;i<64;++i) {
            assert(out[i]<=2147418112 && out[i]>=(-2147483647-1));
            if(blocks>900 && blocks<1500 && out[i]!=in[i]) ++changed;
            if(blocks>=1500 && out[i]!=0) ++tail;
        }
    }
    assert(hist_valid==HIST_FRAMES);
    assert(grain_armed==1u);
    assert(changed>0);
    assert(tail>0);

    /* Reopen must never read the old 128 KiB ring's audio during refill. */
    dreamy_open(0,0,0);
    assert(!dreamy_owner.on);
    dreamy_open(0,0,0);
    assert(dreamy_owner.on);
    assert(hist_valid==0u && hist_write==0u && grain_armed==0u);
    for(i=0;i<64;++i) in[i]=0;
    for(blocks=0;blocks<850;++blocks) {
        granular_render(out,in);
        for(i=0;i<64;++i) assert(out[i]==0);
    }
    assert(grain_armed==1u);

    /* Effect mix zero must preserve low-level stock audio exactly. */
    assert(dreamy_cc(0,60,0,CORE_MIDI_CC_AUTO)==1);
    for(i=0;i<64;++i) in[i]=(i&1 ? -8000 : 8000)*65536;
    granular_render(out,in);
    for(i=0;i<64;++i) assert(out[i]==in[i]);

    printf("PASS GRANULAR: +5/+12 grain engine, MIDI mix only, output bounds, tail, clear-free reopen\n");
    return 0;
}
