/* SPDX-License-Identifier: GPL-2.0-or-later
 * Host sanitizer stress: all three new effects, extremes, changing controls.
 * Requires -DRACK_HOST_TEST and -fsanitize=address,undefined.
 */
#include "rack.h"
#include <stdint.h>
#include <stdio.h>
struct dnfx_state rack_state[DNFX_COUNT];
volatile int32_t rack_active=-1;
static uint32_t rnd=0x53445031u;
static uint32_t next(void) {rnd^=rnd<<13;rnd^=rnd>>17;rnd^=rnd<<5;return rnd;}
int main(void)
{
    int32_t in[64],out[64];
    uint64_t checks=0;
    for(int mode=DNFX_EUREKA;mode<=DNFX_CRUSH;++mode) {
        for(int pass=0;pass<5;pass++) {
            rack_audio_reset(mode);
            for(int blk=0;blk<12000;blk++) {
                struct dnfx_state *s=&rack_state[mode];
                if(blk%47==0) {
                    for(unsigned pg=0;pg<2u;pg++)
                        for(unsigned k=0;k<8u;k++) {
                            uint32_t r=next();
                            s->value[pg][k]=(uint8_t)(pass==0?0:pass==1?127:(r&127u));
                        }
                    if(mode==DNFX_CRUSH) {
                        s->value[0][0]=(uint8_t)(4u+next()%13u);
                        s->value[0][1]=(uint8_t)(1u+next()%32u);
                    }
                    if(mode==DNFX_EUREKA) {
                        s->value[0][7]=(uint8_t)(next()%3u);
                        s->value[1][0]=(uint8_t)(next()%3u);
                        s->value[1][1]=(uint8_t)(next()%3u);
                    }
                }
                for(int i=0;i<64;i++)
                    in[i]=((int32_t)(next()>>16)-32768)*65536;
                rack_audio_render(out,in,mode);
                for(int i=0;i<64;i++) {
                    if((out[i]&65535)!=0) {
                        if(!((mode==DNFX_REEL && s->value[0][4]==0u) ||
                             (mode==DNFX_CRUSH && s->value[0][2]==0u &&
                              s->value[0][3]==100u))) {
                            fprintf(stderr,"Bad Q31 packing at %d/%d/%d/%d\n",mode,pass,blk,i);
                            return 2;
                        }
                    }
                    ++checks;
                }
            }
        }
    }
    printf("PASS rack DSP stress: %llu samples, 3 FX modes x 5 ranges\n",
           (unsigned long long)checks);
    return 0;
}
