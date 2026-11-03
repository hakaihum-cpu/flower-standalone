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

    assert(sizeof(history) == 524288u);
    assert(dreamy_owner.on==0);
    assert(dreamy_owner.flags==CORE_AUDIO_INSERT);
    assert(strcmp(dreamy_menu.name,"GRANULAR")==0);
    assert(GRAIN_MIN_FRAMES == 5760u);
    assert(GRAIN_MAX_FRAMES == 17280u);
    assert(GRAIN_DEFAULT == 11520u);
    assert(pitch_choice[0] == 4u && pitch_choice[1] == 6u);
    assert(grain_frames_target == 11520u);
    assert(grain_position_target == 0u);
    assert(grain_ramp == 0u);
    assert(grain_hann_q15[0] == 0u);
    assert(grain_hann_q15[128] == 32767u);
    for(i=0;i<256;++i) {
        assert((unsigned)grain_hann_q15[i] +
               (unsigned)grain_hann_q15[(i+128)&255] == 32767u);
    }
    assert(pitch_steps[0] == 2048u && pitch_steps[7] == 16384u);
    assert((uint64_t)GRAIN_MAX_FRAMES * pitch_steps[7] < 0xffffffffu);
    assert(GRAIN_LAG_A > 3u * GRAIN_MAX_FRAMES);

    assert(GRAIN_ARM_FRAMES == 90000u);
    assert(GRAIN_LAG_B + GRAIN_POSITION_MAX + GRAIN_MAX_FRAMES / 2u <
           GRAIN_ARM_FRAMES);

    dreamy_open(0,0,0);
    assert(dreamy_owner.on);
    assert(hist_valid==0u && hist_write==0u && grain_armed==0u);

    assert(dreamy_cc(0,60,0,0)==0);
    assert(dreamy_cc(0,60,74,CORE_MIDI_CC_AUTO)==1);
    assert(mix_cc==74u);
    assert(dreamy_cc(0,61,90,CORE_MIDI_CC_AUTO)==0);
    assert(dreamy_cc(0,62,70,CORE_MIDI_CC_AUTO)==0);
    assert(dreamy_cc(0,19,127,CORE_MIDI_CC_AUTO)==0);
    /* Full 0-127 sweep: every pitch band contains exactly 16 CC values. */
    for(i=0;i<128;++i) {
        assert(dreamy_cc(0,33,i,CORE_MIDI_CC_AUTO)==1);
        assert(pitch_choice[0]==(unsigned)i/16u);
        assert(dreamy_cc(0,34,i,CORE_MIDI_CC_AUTO)==1);
        assert(pitch_choice[1]==(unsigned)i/16u);
    }
    assert(dreamy_cc(0,36,0,CORE_MIDI_CC_AUTO)==1);
    assert(grain_frames_target==5760u);
    assert(dreamy_cc(0,36,127,CORE_MIDI_CC_AUTO)==1);
    assert(grain_frames_target==17280u);
    assert(dreamy_cc(0,36,63,CORE_MIDI_CC_AUTO)==1);
    assert(grain_frames_target==11520u); /* 240 ms default */
    assert(dreamy_cc(0,35,127,CORE_MIDI_CC_AUTO)==0);
    assert(dreamy_cc(0,37,127,CORE_MIDI_CC_AUTO)==1);
    assert(grain_position_target==3000u);
    assert(dreamy_cc(0,37,0,CORE_MIDI_CC_AUTO)==1);
    assert(grain_position_target==0u);
    assert(dreamy_cc(0,33,64,CORE_MIDI_CC_AUTO)==1); /* default +5 */
    assert(dreamy_cc(0,34,96,CORE_MIDI_CC_AUTO)==1); /* default +12 */
    assert(dreamy_cc(0,33,127,0)==0); /* require AUTO */
    assert(dreamy_cc(0,36,-1,CORE_MIDI_CC_AUTO)==0);
    assert(dreamy_cc(0,36,128,CORE_MIDI_CC_AUTO)==0);
    assert(dreamy_cc(0,37,-1,CORE_MIDI_CC_AUTO)==0);
    assert(dreamy_cc(0,37,128,CORE_MIDI_CC_AUTO)==0);
    /* Exhaustive safety: 8 pitches x 49 grain sizes x 101 positions. */
    for(int pitch=0;pitch<8;++pitch)
        for(unsigned size=5760u;size<=17280u;size+=240u)
            for(unsigned pos=0u;pos<=3000u;pos+=30u) {
                unsigned lag_min=GRAIN_LAG_A+pos;
                unsigned lag_max=GRAIN_LAG_B+pos;
                unsigned step=pitch_steps[pitch];
                unsigned advanced=(unsigned)(((uint64_t)size*step)>>12);
                /* never outrun the write pointer; negative pitch never
                   reads history older than the fresh arm threshold. */
                assert(lag_min > (advanced>=size ? advanced-size : 0u));
                assert(lag_max+size/2u < GRAIN_ARM_FRAMES);
            }

    /* 1 s program material, then silence. Grain pitch+envelopes must run,
       and there must be no overflow, stale read, or missing render output. */
    for(blocks=0;blocks<5500;++blocks) {
        for(i=0;i<64;++i)
            in[i]=(blocks<4000 ? (i&1 ? -9600 : 9600) : 0)*65536;
        granular_render(out,in);
        for(i=0;i<64;++i) {
            assert(out[i]<=2147418112 && out[i]>=(-2147483647-1));
            if(blocks>3000 && blocks<4000 && out[i]!=in[i]) ++changed;
            if(blocks>=4000 && out[i]!=0) ++tail;
        }
    }
    assert(hist_valid==HIST_FRAMES);
    assert(grain_armed==1u);
    assert(grain_ramp==GRAIN_RAMP_FRAMES);
    assert(changed>0);
    assert(tail>0);
    /* Live MIDI during running audio, including both extreme pitches and
       maximum 140 ms grain size. Existing grains finish at boundaries. */
    for(blocks=0;blocks<1200;++blocks) {
        if((blocks % 71)==0) {
            int cc = (blocks/71)%8*16;
            assert(dreamy_cc(0,33,cc,CORE_MIDI_CC_AUTO)==1);
            assert(dreamy_cc(0,34,127-cc,CORE_MIDI_CC_AUTO)==1);
            assert(dreamy_cc(0,36,(blocks/71)%2 ? 127 : 0,
                            CORE_MIDI_CC_AUTO)==1);
            assert(dreamy_cc(0,37,(blocks/71)%2 ? 127 : 0,
                            CORE_MIDI_CC_AUTO)==1);
        }
        for(i=0;i<64;++i)
            in[i]=(i&1 ? -11000 : 11000)*65536;
        granular_render(out,in);
        for(i=0;i<64;++i) {
            assert(out[i]<=2147418112 && out[i]>=(-2147483647-1));
        }
        for(i=0;i<4;++i) {
            assert(grain_frames[i]>=5760u && grain_frames[i]<=17280u);
            assert(grain_phase[i]<grain_frames[i]);
        }
    }
    assert(dreamy_cc(0,33,127,CORE_MIDI_CC_AUTO)==1);
    assert(dreamy_cc(0,34,0,CORE_MIDI_CC_AUTO)==1);
    assert(dreamy_cc(0,36,127,CORE_MIDI_CC_AUTO)==1);
    for(blocks=0;blocks<400;++blocks) {
        granular_render(out,in);
        for(i=0;i<4;++i) {
            assert(grain_frames[i]<=17280u);
            assert(grain_phase[i]<grain_frames[i]);
        }
    }
    assert(dreamy_cc(0,33,64,CORE_MIDI_CC_AUTO)==1);
    assert(dreamy_cc(0,34,96,CORE_MIDI_CC_AUTO)==1);
    assert(dreamy_cc(0,36,63,CORE_MIDI_CC_AUTO)==1);
    assert(dreamy_cc(0,37,0,CORE_MIDI_CC_AUTO)==1);

    /* Reopen must never read the old 128 KiB ring's audio during refill. */
    dreamy_open(0,0,0);
    assert(!dreamy_owner.on);
    dreamy_open(0,0,0);
    assert(dreamy_owner.on);
    assert(hist_valid==0u && hist_write==0u && grain_armed==0u);
    assert(grain_ramp==0u);
    for(i=0;i<64;++i) in[i]=0;
    for(blocks=0;blocks<3000;++blocks) {
        granular_render(out,in);
        for(i=0;i<64;++i) assert(out[i]==0);
    }
    assert(grain_armed==1u);

    /* Effect mix zero must preserve low-level stock audio exactly. */
    assert(dreamy_cc(0,60,0,CORE_MIDI_CC_AUTO)==1);
    for(i=0;i<64;++i) in[i]=(i&1 ? -8000 : 8000)*65536;
    granular_render(out,in);
    for(i=0;i<64;++i) assert(out[i]==in[i]);

    printf("PASS GRANULAR: 2.73s 512KiB history, Hann overlapping grains 120-360ms default240, CC33/34 pitch, CC37 position, CC60 mix, bound safety, 80ms fade\n");
    return 0;
}
