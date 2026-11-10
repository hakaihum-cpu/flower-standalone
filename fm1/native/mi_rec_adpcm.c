/* SPDX-License-Identifier: GPL-3.0-only
 * Block-isolated IMA ADPCM. No allocation, libc, or audio IRQ buffer clears.
 * 64 PCM16 samples -> 36 bytes: signed predictor, index, pad, 32 nibbles.
 * Every block resets its predictor, enabling trim, pitch, and overdub edits.
 */
#include "mi_rec_adpcm.h"
static const int16_t steps[89]={
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,
    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,
    157,173,190,209,230,253,279,307,337,371,408,449,494,544,
    598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,
    1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,
    5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,
    13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};
static const int8_t shifts[16]={
    -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8
};
static int bound(int n,int low,int high){return n<low?low:n>high?high:n;}
static int step_predict(int predictor,int *index,unsigned nibble)
{
    int step=steps[*index];
    int delta=step>>3;
    if(nibble&1u)delta+=step>>2;
    if(nibble&2u)delta+=step>>1;
    if(nibble&4u)delta+=step;
    if(nibble&8u)delta=-delta;
    predictor=bound(predictor+delta,-32768,32767);
    *index=bound(*index+shifts[nibble&15u],0,88);
    return predictor;
}
void mi_adpcm_encode_64(uint8_t out[MI_REC_ADPCM_BLOCK_BYTES],
                        const int16_t samples[MI_REC_ADPCM_SAMPLES])
{
    unsigned i;int predictor,index=0,first_gap;
    if(!out||!samples)return;
    predictor=samples[0];
    first_gap=(int)samples[1]-(int)samples[0];
    if(first_gap<0)first_gap=-first_gap;
    /* Match the initial step to the local transient; avoid a block-edge
     * slide when the signal enters at full level or high frequency. */
    while(index<88 && (int)steps[index] < first_gap/4)++index;
    out[0]=(uint8_t)predictor;out[1]=(uint8_t)(predictor>>8);
    out[2]=(uint8_t)index;out[3]=0u;
    for(i=0;i<32u;i++)out[4u+i]=0u;
    for(i=0;i<MI_REC_ADPCM_SAMPLES;i++){
        int diff=(int)samples[i]-predictor,mag,step,remaining,code=0;
        unsigned nibble;
        if(diff<0){code=8;mag=-diff;}else mag=diff;
        step=steps[index];remaining=step;
        if(mag>=remaining){code|=4;mag-=remaining;}
        remaining>>=1;
        if(mag>=remaining){code|=2;mag-=remaining;}
        remaining>>=1;
        if(mag>=remaining)code|=1;
        nibble=(unsigned)code;
        if(i&1u)out[4u+(i>>1u)]|=(uint8_t)(nibble<<4u);
        else out[4u+(i>>1u)]=(uint8_t)nibble;
        predictor=step_predict(predictor,&index,nibble);
    }
}
void mi_adpcm_decode_64(int16_t samples[MI_REC_ADPCM_SAMPLES],
                        const uint8_t in[MI_REC_ADPCM_BLOCK_BYTES])
{
    int predictor,index;unsigned i;
    if(!samples||!in)return;
    predictor=(int)(int16_t)((uint16_t)in[0]|((uint16_t)in[1]<<8u));
    index=bound((int)in[2],0,88);
    for(i=0;i<MI_REC_ADPCM_SAMPLES;i++){
        unsigned b=in[4u+(i>>1u)];
        unsigned nibble=(i&1u)?b>>4u:b&15u;
        predictor=step_predict(predictor,&index,nibble);
        samples[i]=(int16_t)predictor;
    }
}
