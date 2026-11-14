/* SPDX-License-Identifier: GPL-3.0-only
 * Freestanding MHP2 reader, intended ONLY for source/host experiment until
 * target compile, timings and source-image quality gates have been approved.
 */
#include "mi_mhp2_codec_poc.h"
#define MI_MHP2_HEADER 64u
#define MI_MHP2_PALS 768u
#define MI_MHP2_TABLE_BYTES 160u
#define MI_MHP2_LENGTHS (24u*2u*160u)
#define MI_MHP2_MODES 24u
#define MI_MHP2_BOOK_DIR (25u*4u)
#define MI_MHP2_FRAME_DIR (646u*4u)
#define MI_MHP2_MIN_PAYLOAD (MI_MHP2_HEADER+MI_MHP2_PALS+MI_MHP2_LENGTHS+MI_MHP2_MODES+MI_MHP2_BOOK_DIR+MI_MHP2_FRAME_DIR)

typedef struct {
    const uint8_t *data;
    uint32_t length;
    uint32_t bit;
} mi_mhp2_bits_t;
typedef struct {
    uint16_t count[32];
    uint16_t firstsym[32];
    uint32_t firstcode[32];
    uint8_t symbols[256];
    uint8_t maxlen;
} mi_mhp2_huff_t;

static uint16_t mi_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t mi_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint32_t mi_crc32(const uint8_t *p, uint32_t n) {
    uint32_t crc=0xffffffffu;
    uint32_t i;
    for(i=0u;i<n;i++){
        unsigned j;
        crc^=p[i];
        for(j=0u;j<8u;j++)crc=(crc >> 1)^((crc&1u)?0xedb88320u:0u);
    }
    return ~crc;
}
static int mi_read_bit(mi_mhp2_bits_t *rd, unsigned *out) {
    uint32_t x;
    if(rd->bit>=rd->length*8u)return 0;
    x=rd->bit++;
    *out=(unsigned)((rd->data[x>>3u]>>(7u-(x&7u)))&1u);
    return 1;
}
/* Compact 5bit code lengths. Canonical codebook is regenerated once per clip. */
static int mi_read_huff_table(const uint8_t *packed, mi_mhp2_huff_t *t) {
    uint8_t lengths[256];
    uint16_t next[32];
    unsigned i,len;
    uint32_t code=0u;
    uint16_t accum=0u;
    for(i=0u;i<32u;i++){
        t->count[i]=0u;t->firstsym[i]=0u;t->firstcode[i]=0u;next[i]=0u;
    }
    t->maxlen=0u;
    for(i=0u;i<256u;i++){
        unsigned at=i*5u,shift=at&7u,byte=at>>3u;
        uint16_t pair=(uint16_t)((uint16_t)packed[byte]<<8u);
        if(byte+1u<MI_MHP2_TABLE_BYTES)pair|=packed[byte+1u];
        len=(unsigned)((pair>>(11u-shift))&31u);
        lengths[i]=(uint8_t)len;
        if(len){t->count[len]++;if(len>t->maxlen)t->maxlen=(uint8_t)len;}
    }
    if(t->maxlen==0u)return 0;
    for(len=1u;len<=31u;len++){
        uint32_t limit=(uint32_t)1u<<len;
        code=(code+(uint32_t)t->count[len-1u])<<1u;
        if(code>limit||t->count[len]>limit-code)return 0;
        t->firstcode[len]=code;
        t->firstsym[len]=accum;
        accum=(uint16_t)(accum+t->count[len]);
        next[len]=t->firstsym[len];
    }
    if(accum>256u)return 0;
    for(i=0u;i<256u;i++)if(lengths[i]){
        unsigned at=(unsigned)next[lengths[i]]++;
        if(at>=256u)return 0;
        t->symbols[at]=(uint8_t)i;
    }
    return 1;
}
static int mi_read_symbol(mi_mhp2_bits_t *rd, const mi_mhp2_huff_t *t, uint8_t *out) {
    uint32_t code=0u;
    unsigned len;
    for(len=1u;len<=t->maxlen;len++){
        unsigned bit;
        if(!mi_read_bit(rd,&bit))return 0;
        code=(code<<1u)|bit;
        if(t->count[len]&&code>=t->firstcode[len]&&
           code-t->firstcode[len]<t->count[len]){
            unsigned index=(unsigned)t->firstsym[len]+
                           (unsigned)(code-t->firstcode[len]);
            if(index>=256u)return 0;
            *out=t->symbols[index];
            return 1;
        }
    }
    return 0;
}
static int mi_dir_bounds(const uint8_t *p, uint32_t start, uint32_t count,
                          uint32_t i, uint32_t limit,
                          uint32_t *lo, uint32_t *hi) {
    uint32_t a,b;
    if(i>=count)return 0;
    a=mi_u32(p+start+i*4u);b=mi_u32(p+start+(i+1u)*4u);
    if(a>b||b>limit)return 0;
    *lo=a;*hi=b;return 1;
}
int mi_mhp2_init(mi_mhp2_t *st, const uint8_t *p, uint32_t n) {
    uint32_t bookstart,framestart;
    if(!st||!p||n<MI_MHP2_MIN_PAYLOAD||n>MI_MHP2_RESOURCE_MAX)return 0;
    if(p[0]!='M'||p[1]!='H'||p[2]!='P'||p[3]!='2'||
       mi_u16(p+4u)!=1u||mi_u16(p+6u)!=0u||mi_u32(p+8u)!=n||
       mi_u16(p+16u)!=24u||mi_u16(p+18u)!=645u||
       mi_u16(p+20u)!=96u||mi_u16(p+22u)!=96u)return 0;
    if(mi_u32(p+24u)!=64u||mi_u32(p+28u)!=832u||
       mi_u32(p+32u)!=8512u||mi_u32(p+36u)!=8536u||
       mi_u32(p+40u)!=8636u||mi_u32(p+44u)!=11220u)return 0;
    bookstart=mi_u32(p+44u);framestart=mi_u32(p+48u);
    if(framestart<bookstart||framestart>n)return 0;
    if(mi_u32(p+8536u)!=0u||mi_u32(p+8636u)!=0u||
       mi_u32(p+8536u+24u*4u)!=framestart-bookstart||
       mi_u32(p+8636u+645u*4u)!=n-framestart)return 0;
    if(mi_crc32(p+64u,n-64u)!=mi_u32(p+12u))return 0;
    st->blob=p;st->length=n;st->cached_clip=0xffffu;
    return 1;
}
int mi_mhp2_decode(mi_mhp2_t *st, uint16_t frame, uint8_t out_pixels[MI_MHP2_FRAME_BYTES]) {
    const uint8_t *p;
    mi_mhp2_huff_t h;
    mi_mhp2_bits_t rd;
    uint32_t bookstart,framestart,a,b,idx,limit;
    unsigned clip,i,ty,tx,row,col;
    if(!st||!st->blob||!out_pixels||frame>=MI_MHP2_FRAME_COUNT)return 0;
    p=st->blob;clip=(unsigned)frame/27u;
    bookstart=mi_u32(p+44u);framestart=mi_u32(p+48u);
    if(st->cached_clip!=clip){
        if(!mi_dir_bounds(p,8536u,24u,clip,framestart-bookstart,&a,&b))return 0;
        if(!mi_read_huff_table(p+832u+clip*320u,&h))return 0;
        rd.data=p+bookstart+a;rd.length=b-a;rd.bit=0u;
        for(i=0u;i<MI_MHP2_DICT_BYTES;i++)
            if(!mi_read_symbol(&rd,&h,&st->book[i]))return 0;
        if((rd.bit+7u)/8u!=rd.length)return 0;
        for(i=0u;i<16u;i++)
            st->palette[i]=mi_u16(p+64u+clip*32u+i*2u);
        st->cached_clip=(uint16_t)clip;
    }
    if(!mi_dir_bounds(p,8636u,645u,(unsigned)frame,st->length-framestart,&a,&b))return 0;
    if(!mi_read_huff_table(p+832u+clip*320u+160u,&h))return 0;
    rd.data=p+framestart+a;rd.length=b-a;rd.bit=0u;
    limit=p[8512u+clip];
    if(limit>1u)return 0;
    for(i=0u;i<MI_MHP2_MAP_BYTES;i++){
        if(limit&&i>=12u){
            unsigned copy;
            if(!mi_read_bit(&rd,&copy))return 0;
            if(copy){st->map[i]=st->map[i-12u];continue;}
        }
        if(!mi_read_symbol(&rd,&h,&st->map[i]))return 0;
    }
    if((rd.bit+7u)/8u!=rd.length)return 0;
    /* Each VQ word is an opaque 8x4 tile of 4-bit palette indices,
     * packed 2 pixels/byte; frame map is 12 x 24 words. */
    for(ty=0u;ty<24u;ty++)for(tx=0u;tx<12u;tx++){
        idx=(uint32_t)st->map[ty*12u+tx]*16u;
        for(row=0u;row<4u;row++)for(col=0u;col<4u;col++){
            uint32_t dest=(uint32_t)(ty*4u+row)*48u+tx*4u+col;
            out_pixels[dest]=st->book[idx+row*4u+col];
        }
    }
    return 1;
}