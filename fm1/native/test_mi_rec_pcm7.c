/* Host-only regression: the new three-track 7-bit log-PCM codec. */
#include "mi_rec_pcm7.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    int16_t src[MI_REC_PCM7_SAMPLES],dec[MI_REC_PCM7_SAMPLES];
    uint8_t block[MI_REC_PCM7_BLOCK_BYTES];
    unsigned i;
    assert(MI_REC_PCM7_STORE_BYTES==115920u);
    for(i=0u;i<MI_REC_PCM7_SAMPLES;i++)src[i]=i<32u?-24000:24000;
    mi_pcm7_encode_64(block,src);
    mi_pcm7_decode_64(dec,block);
    assert(dec[31]<-23000&&dec[32]>23000); /* no transient delay */
    for(i=0u;i<MI_REC_PCM7_SAMPLES;i++)
        src[i]=(int16_t)((int)i*1024-32768);
    mi_pcm7_encode_64(block,src);
    mi_pcm7_decode_64(dec,block);
    for(i=0u;i<MI_REC_PCM7_SAMPLES;i++)
        assert(dec[i]>=-32767&&dec[i]<=32767);
    for(i=0u;i<MI_REC_PCM7_SAMPLES;i++)src[i]=0;
    mi_pcm7_encode_64(block,src);
    mi_pcm7_decode_64(dec,block);
    for(i=0u;i<MI_REC_PCM7_SAMPLES;i++)assert(dec[i]==0);
    puts("PASS: 3-track PCM7 115,920-byte codec, step edge and silence");
    return 0;
}
