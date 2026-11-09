/* Host-only ADPCM smoke test; no FM-1 build triggered by this file. */
#include "mi_rec_adpcm.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    int16_t source[64], decoded[64], silence[64]={0};
    uint8_t block[MI_REC_ADPCM_BLOCK_BYTES];
    unsigned i;
    unsigned long abs_error=0;
    assert(MI_REC_ADPCM_STORE_BYTES==74520u);
    mi_adpcm_encode_64(block,silence);
    mi_adpcm_decode_64(decoded,block);
    for(i=0u;i<64u;i++)assert(decoded[i]>=-4&&decoded[i]<=4);
    for(i=0u;i<64u;i++){
        /* 16-sample triangular cycle, including positive and negative levels. */
        int tri=(int)(i%16u);
        int val=tri<8?tri:15-tri;
        source[i]=(int16_t)((i/16u)%2u?-(val*3500):(val*3500));
    }
    mi_adpcm_encode_64(block,source);
    mi_adpcm_decode_64(decoded,block);
    for(i=0u;i<64u;i++){
        int err=(int)decoded[i]-(int)source[i];
        abs_error+=(unsigned long)(err<0?-err:err);
    }
    assert(abs_error/64u<3500u); /* basic signal fidelity gate */
    puts("PASS: 3-track 74,520-byte IMA ADPCM block codec");
    return 0;
}
