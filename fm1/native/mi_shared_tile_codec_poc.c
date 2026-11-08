/* SPDX-License-Identifier: GPL-3.0-only */
#include "mi_shared_tile_codec_poc.h"

#define MI_TP_CLIPS 24u
#define MI_TP_STEP 27u
#define MI_TP_HDR 28u
#define MI_TP_PAL 28u
#define MI_TP_BOOKDIR (MI_TP_PAL+MI_TP_CLIPS*32u)
#define MI_TP_MAPDIR (MI_TP_BOOKDIR+MI_TP_CLIPS*8u)
#define MI_TP_DATA (MI_TP_MAPDIR+MI_TP_FRAMES*8u)
static uint16_t rd16(const uint8_t *p){return (uint16_t)((unsigned)p[0]|(unsigned)p[1]<<8u);}
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0] | (uint32_t)p[1]<<8u | (uint32_t)p[2]<<16u | (uint32_t)p[3]<<24u;}

/* Raw LZ4 block: no malloc, bounded input/output, handles overlapping copies. */
static int lz4_block(const uint8_t *src, uint32_t n, uint8_t *dst, uint32_t cap)
{
    uint32_t i=0u,o=0u;
    while(i<n){
        uint32_t tok=src[i++],lit=tok>>4u,match=tok&15u,k,offset,extra;
        if(lit==15u){
            do{
                if(i>=n)return -1;
                extra=src[i++];
                if(lit>cap-extra)return -1;
                lit+=extra;
            }while(extra==255u);
        }
        if(lit>cap-o || lit>n-i)return -1;
        for(k=0;k<lit;k++)dst[o+k]=src[i+k];
        o+=lit;i+=lit;
        if(i==n)break;
        if(n-i<2u)return -1;
        offset=rd16(src+i);i+=2u;
        if(offset==0u || offset>o)return -1;
        match+=4u;
        if((tok&15u)==15u){
            do{
                if(i>=n)return -1;
                extra=src[i++];
                if(match>cap-extra)return -1;
                match+=extra;
            }while(extra==255u);
        }
        if(match>cap-o)return -1;
        for(k=0;k<match;k++)dst[o+k]=dst[o+k-offset];
        o+=match;
    }
    return o==cap?0:-1;
}

static int load_block(const uint8_t *payload, uint32_t len, uint32_t dir,
                      uint8_t *out, uint32_t decoded_bytes)
{
    uint32_t at,bytes,flag,i;
    if(dir>len || 8u>len-dir)return -1;
    at=rd32(payload+dir);
    bytes=rd16(payload+dir+4u);
    flag=rd16(payload+dir+6u);
    if(flag>1u || bytes==0u || at<MI_TP_DATA || at>len || bytes>len-at)return -1;
    if(flag==0u)return lz4_block(payload+at,bytes,out,decoded_bytes);
    if(bytes!=decoded_bytes)return -1;
    for(i=0u;i<bytes;i++)out[i]=payload[at+i];
    return 0;
}

int mi_tp_decode_frame(const uint8_t *payload,uint32_t len,uint16_t frame,
                       uint16_t palette[MI_TP_PALETTE],
                       uint8_t pixels[MI_TP_PIXELS_BYTES],
                       uint8_t book[MI_TP_BOOK_BYTES],
                       uint8_t selectors[MI_TP_MAP_BYTES])
{
    uint32_t clip,ty,tx,row,col,b,at,i;
    if(!payload||!palette||!pixels||!book||!selectors ||
       frame>=MI_TP_FRAMES ||len<MI_TP_DATA||len>MI_TP_MAX_PAYLOAD)return -1;
    if(payload[0]!='M'||payload[1]!='T'||payload[2]!='L'||payload[3]!='1'||
       rd16(payload+4)!=1u||rd16(payload+6)!=MI_TP_FRAMES||
       rd16(payload+8)!=MI_TP_CLIPS||rd16(payload+10)!=MI_TP_STEP||
       rd32(payload+12)!=MI_TP_PAL||rd32(payload+16)!=MI_TP_BOOKDIR||
       rd32(payload+20)!=MI_TP_MAPDIR||rd32(payload+24)!=MI_TP_DATA)return -1;
    clip=frame/MI_TP_STEP;
    if(load_block(payload,len,MI_TP_BOOKDIR+clip*8u,book,MI_TP_BOOK_BYTES)!=0||
       load_block(payload,len,MI_TP_MAPDIR+(uint32_t)frame*8u,
                  selectors,MI_TP_MAP_BYTES)!=0)return -1;
    for(i=0;i<MI_TP_PALETTE;i++)
        palette[i]=rd16(payload+MI_TP_PAL+clip*32u+i*2u);
    for(ty=0u;ty<24u;ty++){
        for(tx=0u;tx<12u;tx++){
            b=(uint32_t)selectors[ty*12u+tx]*16u;
            for(row=0u;row<4u;row++){
                at=(ty*4u+row)*48u+tx*4u;
                for(col=0u;col<4u;col++)pixels[at+col]=book[b+row*4u+col];
            }
        }
    }
    return 0;
}
