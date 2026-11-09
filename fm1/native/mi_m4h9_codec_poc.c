/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO offline M4H9 proof, not linked into firmware.
 */
#include "mi_m4h9_codec_poc.h"
#define PAL 32u
#define LEN 800u
#define BOOK 3104u
#define INDEX 21536u
#define BITSTREAM 24052u
static uint16_t u16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
void mi_m4h9_reset(mi_m4h9_state_t *state){if(state)state->cached_clip=-1;}
static int load_clip(const uint8_t *src,uint32_t clip,mi_m4h9_state_t *state){
    uint32_t i,l,code=0u,next=0u,positions[16];
    const uint8_t *lengths=src+LEN+clip*MI_M4H9_CODES;
    for(i=0;i<16u;i++)state->count[i]=state->first_code[i]=state->first_symbol[i]=0u;
    for(i=0;i<MI_M4H9_CODES;i++){
        l=lengths[i];if(l>15u)return -1;
        if(l)state->count[l]++;
    }
    for(l=1u;l<=15u;l++){
        code=(code+state->count[l-1u])*2u;
        if(code+state->count[l]>(1u<<l))return -1;
        state->first_code[l]=(uint16_t)code;
        state->first_symbol[l]=(uint16_t)next;
        positions[l]=next;next+=state->count[l];
    }
    if(next==0u||next>MI_M4H9_CODES)return -1;
    for(i=0;i<MI_M4H9_CODES;i++){
        l=lengths[i];if(l)state->alphabet[positions[l]++]=(uint8_t)i;
    }
    for(i=0;i<MI_M4H9_BOOK;i++)state->tiles[i]=src[BOOK+clip*MI_M4H9_BOOK+i];
    for(i=0;i<16u;i++)state->palette[i]=u16(src+PAL+clip*32u+i*2u);
    state->cached_clip=(int16_t)clip;
    return 0;
}
static int symbol(const uint8_t *p,uint32_t start,uint32_t size,uint32_t *bit,
                  const mi_m4h9_state_t *state){
    uint32_t width,code=0u,delta,at;
    for(width=1u;width<=15u;width++){
        at=*bit;
        if(at/8u>=size)return -1;
        code=(code<<1u)|((p[start+at/8u]>>(7u-(at&7u)))&1u);
        *bit=at+1u;
        if(code>=state->first_code[width]){
            delta=code-state->first_code[width];
            if(delta<state->count[width])
                return state->alphabet[state->first_symbol[width]+delta];
        }
    }
    return -1;
}
int mi_m4h9_decode(const uint8_t *source,uint32_t length,uint16_t frame,
                   mi_m4h9_state_t *state,uint8_t pixels[MI_M4H9_PIXELS]){
    uint32_t clip,begin,end,bit=0u,tile,row,k;
    if(!source||!state||!pixels||frame>=MI_M4H9_FRAMES||
       length<BITSTREAM||length>MI_M4H9_MAX_BYTES)return -1;
    if(source[0]!='M'||source[1]!='4'||source[2]!='H'||source[3]!='9'||
       u16(source+4)!=1u||u16(source+6)!=96u||
       u16(source+8)!=MI_M4H9_FRAMES||u16(source+10)!=24u||
       u16(source+12)!=27u||u16(source+14)!=MI_M4H9_CODES||
       u32(source+16)!=PAL||u32(source+20)!=LEN||
       u32(source+24)!=BOOK||u32(source+28)!=INDEX)return -1;
    begin=u32(source+INDEX+frame*4u);
    end=u32(source+INDEX+((uint32_t)frame+1u)*4u);
    if(begin<BITSTREAM||begin>=end||end>length){state->cached_clip=-1;return -1;}
    clip=frame/27u;
    if(state->cached_clip!=(int16_t)clip && load_clip(source,clip,state)){
        state->cached_clip=-1;return -1;
    }
    for(tile=0;tile<576u;tile++){
        int s=symbol(source,begin,end-begin,&bit,state);
        if(s<0||(unsigned)s>=MI_M4H9_CODES){state->cached_clip=-1;return -1;}
        for(row=0;row<4u;row++){
            k=((tile/24u)*4u+row)*48u+(tile%24u)*2u;
            pixels[k]=state->tiles[(unsigned)s*8u+row*2u];
            pixels[k+1u]=state->tiles[(unsigned)s*8u+row*2u+1u];
        }
    }
    return 0;
}
