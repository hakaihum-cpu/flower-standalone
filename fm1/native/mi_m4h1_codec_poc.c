/* SPDX-License-Identifier: GPL-3.0-only
 * Experimental M4H1 offline candidate, not linked into firmware.
 */
#include "mi_m4h1_codec_poc.h"

#define PAL_OFF 32u
#define LEN_OFF (PAL_OFF+24u*32u)
#define BOOK_OFF (LEN_OFF+24u*64u)
#define INDEX_OFF (BOOK_OFF+24u*512u)
#define BITS_OFF (INDEX_OFF+(628u+1u)*4u)

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}
static uint32_t read32(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
void mi_m4h1_init(mi_m4h1_state_t *state)
{
    if (state) state->clip=-1;
}
static int clip_load(const uint8_t *src, uint16_t clip, mi_m4h1_state_t *state)
{
    uint32_t i,l,run,code,next,positions[MI_M4H1_MAXLEN+1u];
    const uint8_t *lengths=src+LEN_OFF+(uint32_t)clip*64u;
    for(i=0u;i<=MI_M4H1_MAXLEN;i++){
        state->count[i]=0u;state->first_code[i]=0u;
        state->first_symbol[i]=0u;
    }
    for(i=0u;i<MI_M4H1_CODES;i++){
        l=lengths[i];
        if(l>MI_M4H1_MAXLEN)return -1;
        if(l)state->count[l]++;
    }
    run=0u;
    for(l=1u;l<=MI_M4H1_MAXLEN;l++)run+=state->count[l];
    if(!run)return -1;
    code=0u;next=0u;
    for(l=1u;l<=MI_M4H1_MAXLEN;l++){
        code=(code+state->count[l-1u])*2u;
        if(code+state->count[l]>(1u<<l))return -1;
        state->first_code[l]=(uint16_t)code;
        state->first_symbol[l]=(uint16_t)next;
        positions[l]=next;next+=state->count[l];
    }
    for(i=0u;i<MI_M4H1_CODES;i++){
        l=lengths[i];
        if(l)state->symbols[positions[l]++]=(uint8_t)i;
    }
    for(i=0u;i<MI_M4H1_TILE_BOOK_BYTES;i++)
        state->tilebook[i]=src[BOOK_OFF+(uint32_t)clip*512u+i];
    for(i=0u;i<16u;i++)
        state->palette[i]=read16(src+PAL_OFF+(uint32_t)clip*32u+i*2u);
    state->clip=(int16_t)clip;
    return 0;
}
static int read_symbol(const uint8_t *src,uint32_t begin,uint32_t end,
                       uint32_t *bitpos,const mi_m4h1_state_t *state)
{
    uint32_t width,code=0u,word,at,delta;
    for(width=1u;width<=MI_M4H1_MAXLEN;width++){
        at=*bitpos;
        if(at/8u>=end-begin)return -1;
        word=src[begin+at/8u];
        code=(code<<1u)|((word>>(7u-(at&7u)))&1u);
        *bitpos=at+1u;
        if(code>=state->first_code[width]){
            delta=code-state->first_code[width];
            if(delta<state->count[width])
                return state->symbols[state->first_symbol[width]+delta];
        }
    }
    return -1;
}
int mi_m4h1_decode(const uint8_t *payload,uint32_t bytes,uint16_t frame,
                   mi_m4h1_state_t *state,
                   uint8_t pixels[MI_M4H1_PIXELS_BYTES])
{
    uint32_t clip,begin,end,position=0u,tile,row,col,symbol,k;
    if(!payload||!pixels||!state||frame>=MI_M4H1_FRAMES||
       bytes<BITS_OFF||bytes>MI_M4H1_MAX_BYTES)return -1;
    if(payload[0]!='M'||payload[1]!='4'||payload[2]!='H'||payload[3]!='1'||
       read16(payload+4u)!=1u||read16(payload+6u)!=96u||
       read16(payload+8u)!=MI_M4H1_FRAMES||read16(payload+10u)!=MI_M4H1_CLIPS||
       read16(payload+12u)!=27u||read16(payload+14u)!=64u||
       read32(payload+16u)!=PAL_OFF||read32(payload+20u)!=LEN_OFF||
       read32(payload+24u)!=BOOK_OFF||read32(payload+28u)!=INDEX_OFF)return -1;
    begin=read32(payload+INDEX_OFF+(uint32_t)frame*4u);
    end=read32(payload+INDEX_OFF+((uint32_t)frame+1u)*4u);
    if(begin<BITS_OFF||begin>=end||end>bytes){state->clip=-1;return -1;}
    clip=(uint32_t)frame/27u;
    if(state->clip!=(int16_t)clip&&clip_load(payload,(uint16_t)clip,state)){
        state->clip=-1;return -1;
    }
    for(tile=0u;tile<576u;tile++){
        int x=read_symbol(payload,begin,end,&position,state);
        if(x<0||x>=64){state->clip=-1;return -1;}
        symbol=(uint32_t)x;
        for(row=0u;row<4u;row++){
            k=((tile/24u)*4u+row)*48u+(tile%24u)*2u;
            for(col=0u;col<2u;col++)
                pixels[k+col]=state->tilebook[symbol*8u+row*2u+col];
        }
    }
    return 0;
}
