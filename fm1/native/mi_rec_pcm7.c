/* SPDX-License-Identifier: GPL-3.0-only
 * 7-bit logarithmic PCM, not ADPCM: signed 6-bit magnitude companding.
 * No predictor state between adjacent samples, preserving FMDrum attacks,
 * square-wave transitions and exact loop playback position across overdubs.
 * 64 PCM16 samples -> 56 bytes, with no headers or allocator.
 */
#include "mi_rec_pcm7.h"
/* 64 magnitudes from mu=15 logarithmic companding.
 * Precomputed once for the target's freestanding C toolchain, no libm. */
static const int16_t magnitude[64] = {
    0,98,201,308,420,538,660,788,
    922,1062,1208,1360,1520,1686,1861,2043,
    2233,2432,2639,2856,3083,3320,3568,3827,
    4097,4380,4675,4984,5306,5643,5995,6363,
    6748,7150,7570,8009,8467,8946,9447,9970,
    10517,11089,11686,12310,12962,13644,14356,15100,
    15878,16690,17540,18427,19354,20323,21336,22394,
    23500,24656,25863,27125,28444,29822,31262,32767
};
static unsigned encode_magnitude(unsigned v)
{
    unsigned lo=0u,hi=63u;
    if(v>=32767u)return 63u;
    while(lo<hi){
        unsigned mid=(lo+hi)>>1u;
        if((unsigned)magnitude[mid]<v)lo=mid+1u;
        else hi=mid;
    }
    if(lo>0u && v-(unsigned)magnitude[lo-1u] <=
                  (unsigned)magnitude[lo]-v)--lo;
    return lo;
}
void mi_pcm7_encode_64(uint8_t out[MI_REC_PCM7_BLOCK_BYTES],
                       const int16_t pcm[MI_REC_PCM7_SAMPLES])
{
    unsigned i;
    if(!out||!pcm)return;
    for(i=0u;i<MI_REC_PCM7_BLOCK_BYTES;i++)out[i]=0u;
    for(i=0u;i<MI_REC_PCM7_SAMPLES;i++){
        int v=(int)pcm[i];
        unsigned sign=v<0?64u:0u,code,bit,byte,shift;
        if(v<0)v=-v;
        if(v>32767)v=32767;
        code=sign|encode_magnitude((unsigned)v);
        bit=7u*i;byte=bit>>3u;shift=bit&7u;
        out[byte]|=(uint8_t)(code<<shift);
        if(shift>1u)out[byte+1u]|=(uint8_t)(code>>(8u-shift));
    }
}
void mi_pcm7_decode_64(int16_t pcm[MI_REC_PCM7_SAMPLES],
                       const uint8_t in[MI_REC_PCM7_BLOCK_BYTES])
{
    unsigned i;
    if(!pcm||!in)return;
    for(i=0u;i<MI_REC_PCM7_SAMPLES;i++){
        unsigned bit=7u*i,byte=bit>>3u,shift=bit&7u;
        unsigned code=(unsigned)in[byte]>>shift;
        if(shift>1u)code|=(unsigned)in[byte+1u]<<(8u-shift);
        code&=127u;
        pcm[i]=(int16_t)((code&64u)?
            -magnitude[code&63u]:magnitude[code&63u]);
    }
}
