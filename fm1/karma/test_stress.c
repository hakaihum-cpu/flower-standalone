/* SPDX-License-Identifier: MIT
 * Reproducible host stress: exercise all 5 waveforms and FX mode
 * changes with 640,000 samples under AddressSanitizer and UBSan.
 * Includes the smoke harness stubs, not the embedded firmware binary.
 */
#define main original_smoke
#include "test_engine.c"
#undef main

int main(void)
{
    track_t *t = &trk[0];
    vmod_t m = {.inc=42852281u,.amp0=20000,.amp1=20000};
    uint32_t state=0xA0B05DE1u, block, i, j;
    int32_t b[32];
    if (original_smoke())
        return 1;
    for (block=0; block<20000; block++) {
        for (i=0; i<8; i++) {
            state=state*1664525u+1013904223u;
            t->p[i]=(int16_t)((state>>17)%162u-16);
        }
        state=state*1664525u+1013904223u;
        t->p[P_E0]=(int16_t)(state%4u);
        state=state*1664525u+1013904223u;
        t->p[P_E4]=(int16_t)(state%3u);
        state=state*1664525u+1013904223u;
        m.inc=1000u+state/17u;
        memset(b,0,sizeof b);
        miyako_render(t,&t->v[0],b,32,&m);
        miyako_post(t,b,32);
        for (j=0; j<32; j++) {
            if(b[j]>200000 || b[j]<-200000) {
                fprintf(stderr,"FAIL: block=%u sample=%u amplitude=%d\n",block,j,b[j]);
                return 1;
            }
        }
    }
    puts("PASS: 20000 randomized blocks / 640000 output samples under ASan+UBSan");
    return 0;
}
