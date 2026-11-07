/* SPDX-License-Identifier: GPL-2.0-or-later
 * CPU optimization regression for Reel soft clip and Bitcrusher.
 * Host-only: no SYX generation. Compile with -DRACK_HOST_TEST, -Werror,
 * and -fsanitize=address,undefined, then compare with legacy math.
 */
#define RACK_HOST_TEST 1
#include "../rack_audio.c"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
struct dnfx_state rack_state[DNFX_COUNT];
volatile int32_t rack_active=-1;
static void expect(int test)
{
    if(!test) abort();
}
static int32_t legacy_tape(int32_t x,uint32_t drive)
{
    int32_t absx=x<0?-x:x;
    int32_t denom=256+((((int32_t)drive-256)*absx)>>15);
    return clamp16((x*(int32_t)drive)/denom);
}
int main(void)
{
    unsigned drive,bits;
    int32_t max_error=0;
    uint64_t checked=0u;
    for(drive=0u;drive<=127u;++drive) {
        const int16_t *table;
        rack_state[DNFX_REEL].value[0][0]=(uint8_t)drive;
        rack_state[DNFX_REEL].value[0][1]=12u;
        rack_state[DNFX_REEL].value[0][2]=34u;
        rack_audio_prepare_reel((uint8_t)drive);
        table=reel_soft_table[reel_soft_index];
        for(int32_t x=-32768;x<=32767;++x) {
            uint32_t d=256u+(1536u*drive)/127u;
            int32_t got=tape_lookup(x,table);
            int32_t ref=legacy_tape(x,d);
            int32_t diff=got-ref;
            if(diff<0) diff=-diff;
            if(diff>max_error) max_error=diff;
            ++checked;
            expect(diff<=120); /* <=0.4% of full scale, bounded interpolation */
        }
    }
    /* Changing WOW or FLUTTER without DRIVE must update the modulation
     * depths, without mutating the current Reel transfer table. */
    {
        uint8_t old_curve=reel_soft_index;
        rack_state[DNFX_REEL].value[0][1]=127u;
        rack_state[DNFX_REEL].value[0][2]=127u;
        rack_audio_prepare_reel(rack_state[DNFX_REEL].value[0][0]);
        expect(reel_soft_index==old_curve);
        expect(reel_wow_depth_q8==49152u);
        expect(reel_flutter_depth_q8==12288u);
    }
    /* Rounded 4..16bit quantization must reproduce the original C
     * trunc-toward-zero semantics over all signed 16-bit samples. */
    for(bits=4u;bits<=16u;++bits) {
        uint32_t step=1u<<(16u-bits);
        rack_state[DNFX_CRUSH].value[0][0]=(uint8_t)bits;
        rack_state[DNFX_CRUSH].value[0][1]=1u;
        rack_state[DNFX_CRUSH].value[0][2]=127u;
        rack_state[DNFX_CRUSH].value[0][3]=100u;
        rack_audio_reset(DNFX_CRUSH);
        for(int32_t x=-32768;x<=32767;++x) {
            int16_t dry[2]={(int16_t)x,(int16_t)x};
            int32_t wet[2]={0,0};
            int32_t round=x>=0?x+(int32_t)(step/2u):x-(int32_t)(step/2u);
            int32_t legacy=clamp16((round/(int32_t)step)*(int32_t)step);
            crush_frame(wet,dry,&rack_state[DNFX_CRUSH]);
            expect(wet[0]==legacy && wet[1]==legacy);
            ++checked;
        }
    }
    printf("PASS CPU equivalence: %llu samples, max 16bit tape LUT error %d\n",
           (unsigned long long)checked,max_error);
    return 0;
}
