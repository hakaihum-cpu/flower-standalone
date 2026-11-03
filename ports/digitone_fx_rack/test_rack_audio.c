/* SPDX-License-Identifier: GPL-2.0-or-later
 * Host DSP regression (instrument validation still required).
 * Compile rack_audio.c together with this file, -DRACK_HOST_TEST and UBSan.
 */
#include "rack.h"
#include <stdlib.h>
#include <stdint.h>
struct dnfx_state rack_state[DNFX_COUNT];
volatile int32_t rack_active=-1;
static void check(int c) {if(!c) abort();}
static void fill(int32_t *p,int32_t x)
{unsigned i;for(i=0;i<64u;++i)p[i]=x;}
int main(void)
{
    int32_t in[64],out[64];
    unsigned i,k;
    rack_state[DNFX_CRUSH].value[0][0]=16u;
    rack_state[DNFX_CRUSH].value[0][1]=1u;
    rack_state[DNFX_CRUSH].value[0][2]=127u;
    rack_state[DNFX_CRUSH].value[0][3]=100u;
    rack_audio_reset(DNFX_CRUSH);
    fill(in,12345*65536);
    rack_audio_render(out,in,DNFX_CRUSH);
    for(i=0u;i<64u;++i) check(out[i]==in[i]);
    /* With MIX=0, the complete Q31 input must remain untouched.
     * With MIX=127/BITS=16, 24-bit input is intentionally reduced. */
    rack_state[DNFX_CRUSH].value[0][2]=0u;
    fill(in,12345*65536+321);
    rack_audio_render(out,in,DNFX_CRUSH);
    for(i=0u;i<64u;++i) check(out[i]==in[i]);
    rack_state[DNFX_CRUSH].value[0][2]=127u;
    fill(in,12345*65536);
    rack_state[DNFX_CRUSH].value[0][0]=4u;
    rack_audio_reset(DNFX_CRUSH);
    rack_audio_render(out,in,DNFX_CRUSH);
    check(out[0]!=in[0]);
    rack_state[DNFX_CRUSH].value[0][2]=0u;
    rack_audio_render(out,in,DNFX_CRUSH);
    for(i=0u;i<64u;++i) check(out[i]==in[i]);
    rack_state[DNFX_REEL].value[0][0]=80u;
    rack_state[DNFX_REEL].value[0][1]=25u;
    rack_state[DNFX_REEL].value[0][2]=13u;
    rack_state[DNFX_REEL].value[0][3]=0u;
    rack_state[DNFX_REEL].value[0][4]=0u;
    rack_audio_reset(DNFX_REEL);
    for(k=0u;k<150u;++k) {
        fill(in,10000*65536+511);
        rack_audio_render(out,in,DNFX_REEL);
        for(i=0u;i<64u;++i) check(out[i]==in[i]);
    }
    rack_state[DNFX_REEL].value[0][4]=127u;
    rack_audio_render(out,in,DNFX_REEL);
    check(out[0]!=in[0]);
    rack_state[DNFX_EUREKA].value[0][0]=100u;
    rack_state[DNFX_EUREKA].value[0][1]=64u;
    rack_state[DNFX_EUREKA].value[0][2]=64u;
    rack_state[DNFX_EUREKA].value[0][3]=64u;
    rack_state[DNFX_EUREKA].value[0][4]=10u;
    rack_state[DNFX_EUREKA].value[0][7]=1u;
    rack_state[DNFX_EUREKA].value[1][0]=1u;
    rack_state[DNFX_EUREKA].value[1][7]=1u;
    rack_audio_reset(DNFX_EUREKA);
    for(k=0u;k<190u;++k) {
        fill(in,13000*65536);
        rack_audio_render(out,in,DNFX_EUREKA);
    }
    check(out[0]!=0);
    return 0;
}
