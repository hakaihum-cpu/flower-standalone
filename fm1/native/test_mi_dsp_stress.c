/* Host-only deterministic sanitizer stress: PCM7 + stereo SLICER.
 * Bounded input variation exercises block headers, transients, disabled FX,
 * tempo changes, muted histories, and both stereo signals.
 * No FM-1 firmware build is triggered by this test source.
 */
#include "mi_rec_pcm7.h"
#include "mi_slicer.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t rng=0x821273abu;
static uint32_t next_random(void)
{
    rng^=rng<<13; rng^=rng>>17; rng^=rng<<5;
    return rng;
}
int main(void)
{
    int16_t pcm[MI_REC_PCM7_SAMPLES],decoded[MI_REC_PCM7_SAMPLES];
    uint8_t block[MI_REC_PCM7_BLOCK_BYTES];
    mi_slicer_t slicer;
    unsigned trial,i;
    float l,r;
    mi_slicer_init(&slicer);
    assert(MI_REC_PCM7_STORE_BYTES==115920u);
    for(trial=0u;trial<80000u;trial++){
        for(i=0u;i<MI_REC_PCM7_SAMPLES;i++)
            pcm[i]=(int16_t)next_random();
        mi_pcm7_encode_64(block,pcm);
        /* Decoding any block must be deterministic and bounded. */
        mi_pcm7_decode_64(decoded,block);
        /* Separate identical decode verifies all block headers are
         * deterministic without cross-track mutable codec state. */
        {
            int16_t second[MI_REC_PCM7_SAMPLES];
            mi_pcm7_decode_64(second,block);
            for(i=0u;i<MI_REC_PCM7_SAMPLES;i++)
                assert(second[i]==decoded[i]);
        }
        {
            float in_l=(float)(int16_t)next_random()/32768.f;
            float in_r=(float)(int16_t)next_random()/32768.f;
            float step=(float)(next_random()%100000u);
            unsigned rate=next_random()%140u;
            unsigned random=next_random()%140u;
            unsigned active=(next_random()%100u)!=0u;
            mi_slicer_tick(&slicer,in_l,in_r,step,rate,random,
                           active,&l,&r);
            assert(l>=-1.f&&l<=1.f&&r>=-1.f&&r<=1.f);
            assert(slicer.valid<=MI_SLICER_FRAMES);
            assert(slicer.span<=MI_SLICER_FRAMES/2u);
            if(!active)assert(slicer.valid==0u&&!slicer.active);
        }
    }
    puts("PASS: 80000 deterministic PCM7/stereo SLICER sanitizer cases");
    return 0;
}
