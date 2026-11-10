/* Host-only test of real Wingie2 SET1 flash-backed ROM samples.
 * Assets are 3x0.8 s, 12 kHz, unsigned 8-bit mono, no extra RAM pool.
 */
#include "mi_piano_set1.h"
#include <stdio.h>
#include <assert.h>
int main(void)
{
    unsigned zone,i;
    assert(MI_PIANO_SET1_COUNT==3u);
    assert(MI_PIANO_SET1_RATE==12000u);
    assert(MI_PIANO_SET1_SAMPLES==9600u);
    assert(mi_piano_set1_roots[0]==51u&&
           mi_piano_set1_roots[1]==58u&&mi_piano_set1_roots[2]==65u);
    for(zone=0u;zone<3u;zone++){
        /* Actual flash ROM is exercised by mi_piano_set1_tick below. */
        float pos=0.f,out,sum=0.f;
        unsigned transitions=0u;
        for(i=0u;i<12000u;i++){
            out=mi_piano_set1_tick(zone,&pos,1.f);
            assert(out>=-1.f&&out<=1.f);
            assert(pos>=0.f&&pos<9600.f);
            if(out>0.03f||out< -0.03f)++transitions;
            sum+=out*out;
        }
        assert(sum>5.f && transitions>500u);
        assert(mi_piano_set1_loop_start[zone]>=5000u);
        assert(mi_piano_set1_loop_start[zone]+192u<9600u);
        /* Arbitrary out-of-range positions cannot index outside flash ROM. */
        pos=100000.f;
        mi_piano_set1_tick(zone,&pos,1.f);
        assert(pos>=5000.f&&pos<9600.f);
    }
    puts("PASS: Wingie2 SET1 3x flash PCM, pitch and sustain loop bounds");
    return 0;
}
