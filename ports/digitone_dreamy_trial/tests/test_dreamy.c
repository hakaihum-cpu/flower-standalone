/* Source-level host audio test. No stock firmware or emulator involved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../dreamy.c"

int main(void)
{
    int32_t in[64], out[64];
    int blocks, i, changed=0, tail=0, saturation=0;
    assert(dreamy_owner.on==0);
    assert(dreamy_owner.flags==CORE_AUDIO_INSERT);
    assert(strcmp(dreamy_menu.name,"DREAMY")==0);

    dreamy_open(0,0,0);
    assert(!dreamy_owner.on);
    for(i=0;i<32 && !dreamy_owner.on;i++) dreamy_tick(0);
    assert(dreamy_owner.on && clear_pos==MEM_COUNT);

    assert(dreamy_cc(0,60,0,0)==0);
    assert(dreamy_cc(0,60,74,CORE_MIDI_CC_AUTO)==1);
    assert(dreamy_cc(0,61,90,CORE_MIDI_CC_AUTO)==1);
    assert(dreamy_cc(0,62,70,CORE_MIDI_CC_AUTO)==1);
    assert(dreamy_cc(0,19,127,CORE_MIDI_CC_AUTO)==0);
    assert(mix_cc==74 && time_cc==90 && room_cc==70);

    /* 1 s of program material plus 0.3 s of silence. All samples bounded. */
    for(blocks=0; blocks<2000; ++blocks) {
        for(i=0;i<64;i++) in[i]=blocks<1500 ? (i&1 ? -9600 : 9600)*65536 : 0;
        dreamy_render(out,in);
        for(i=0;i<64;i++) {
            assert(out[i]<=2147418112 && out[i]>=(-2147483647-1));
            if(blocks>900 && blocks<1500 && out[i]!=in[i]) ++changed;
            if(blocks>=1500 && out[i]!=0) ++tail;
            if(out[i]==2147418112 || out[i]==(-2147483647-1)) ++saturation;
        }
    }
    assert(changed>0);
    assert(tail>0);
    assert(saturation==0);

    dreamy_open(0,0,0);
    assert(!dreamy_owner.on && !requested_on);
    dreamy_open(0,0,0);
    for(i=0;i<32 && !dreamy_owner.on;i++) dreamy_tick(0);
    assert(dreamy_owner.on && hist_valid==0 && hist_write==0);
    puts("PASS DREAMY insert: activation, CC, audio, tail, bounds, fresh state");
    return 0;
}
