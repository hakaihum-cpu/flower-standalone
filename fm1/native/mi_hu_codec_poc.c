/* SPDX-License-Identifier: GPL-3.0-only
 * Freestanding, bounded MHU1 canonical Huffman decoder.
 * Experimental proof only; NOT linked into MIYAKO firmware.
 */
#include "mi_hu_codec_poc.h"
#define HU_HEAD 32u
#define HU_PAL 32u
#define HU_DIR 800u
#define HU_DESC 141u
#define HU_FDIR 4184u
#define HU_DATA 6696u
#define HU_STEP 27u
#define HU_MAX_BITS 15u

static uint16_t rd16(const uint8_t *p){return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1]<<8));}
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t crc32(const uint8_t *p,uint32_t n){
    uint32_t c=0xffffffffu,i;unsigned b;
    for(i=0;i<n;i++){
        c^=p[i];
        for(b=0;b<8;b++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);
    }
    return c^0xffffffffu;
}
static unsigned hu_off(const mi_hu_reader_t *r,unsigned f){return (unsigned)rd32(r->data+HU_FDIR+f*4u);}
static const uint8_t *hu_desc(const mi_hu_reader_t *r,unsigned clip){return r->data+HU_DIR+clip*HU_DESC;}
typedef struct {
    uint16_t count[HU_MAX_BITS+1u];
    uint16_t first[HU_MAX_BITS+1u];
    uint16_t rank[HU_MAX_BITS+1u];
    uint8_t sorted[256];
} hu_table_t;
static int hu_prepare(const uint8_t *packed,unsigned alphabet,hu_table_t *t){
    uint16_t next[HU_MAX_BITS+1u];
    unsigned i,bits,code=0,r=0;
    for(i=0;i<=HU_MAX_BITS;i++)t->count[i]=t->first[i]=t->rank[i]=0u;
    for(i=0;i<alphabet;i++){
        bits=(i&1u)?(packed[i/2u]&15u):(packed[i/2u]>>4);
        if(bits>HU_MAX_BITS)return -1;
        if(bits)t->count[bits]++;
    }
    for(bits=1;bits<=HU_MAX_BITS;bits++){
        code=(code+t->count[bits-1u])*2u;
        if(code+(unsigned)t->count[bits]>(1u<<bits))return -1;
        t->first[bits]=(uint16_t)code;
        t->rank[bits]=(uint16_t)r;
        next[bits]=(uint16_t)r;
        r+=(unsigned)t->count[bits];
    }
    if(!r)return -1;
    for(i=0;i<alphabet;i++){
        bits=(i&1u)?(packed[i/2u]&15u):(packed[i/2u]>>4);
        if(bits)t->sorted[next[bits]++]=(uint8_t)i;
    }
    return 0;
}
static int hu_decode(const uint8_t *src,unsigned n,const uint8_t *lens,
                     unsigned alphabet,unsigned symbols,uint8_t *dst,int nibbles){
    hu_table_t t;
    unsigned pos=0,code=0,bits=0,out=0;
    if(hu_prepare(lens,alphabet,&t)!=0)return -1;
    while(out<symbols){
        unsigned first;
        if(pos>=n*8u)return -1;
        code=(code<<1u)|((src[pos>>3u]>>(7u-(pos&7u)))&1u);
        pos++;bits++;
        if(bits>HU_MAX_BITS)return -1;
        first=(unsigned)t.first[bits];
        if(t.count[bits]&&code>=first&&code-first<t.count[bits]){
            unsigned sym=t.sorted[t.rank[bits]+code-first];
            if(nibbles){
                if(!(out&1u))dst[out>>1u]=(uint8_t)(sym<<4u);
                else dst[out>>1u]|=(uint8_t)sym;
            }else dst[out]=(uint8_t)sym;
            out++;code=0;bits=0;
        }
    }
    if((pos+7u)/8u!=n)return -1;
    if(pos&7u){
        unsigned mask=(1u<<(8u-(pos&7u)))-1u;
        if(src[n-1u]&mask)return -1;
    }
    return 0;
}
static void copy(uint8_t *d,const uint8_t *s,unsigned n){while(n--)*d++=*s++;}
int mi_hu_open(mi_hu_reader_t *reader,const uint8_t *payload,uint32_t size){
    unsigned c,f,last=HU_DATA;
    if(!reader||!payload||size<HU_DATA||size>MI_HU_RESOURCE_MAX)return -1;
    if(payload[0]!='M'||payload[1]!='H'||payload[2]!='U'||payload[3]!='1'||
       rd16(payload+4)!=1u||rd16(payload+6)!=MI_HU_FRAMES||
       rd16(payload+8)!=MI_HU_CLIPS||rd16(payload+10)!=HU_STEP||
       rd32(payload+12)!=HU_PAL||rd32(payload+16)!=HU_DIR||
       rd32(payload+20)!=HU_FDIR||rd32(payload+24)!=HU_DATA||
       rd32(payload+28)!=crc32(payload+HU_HEAD,size-HU_HEAD))return -1;
    for(c=0;c<MI_HU_CLIPS;c++){
        const uint8_t *d=payload+HU_DIR+c*HU_DESC;
        unsigned book=rd32(d+137u), first=rd32(payload+HU_FDIR+c*HU_STEP*4u);
        if((d[136u]&~3u)||book<last||book>=first||first>size)return -1;
        if((d[136u]&1u)&&first-book!=MI_HU_BOOK_BYTES)return -1;
        last=first;
    }
    last=0;
    for(f=0;f<MI_HU_FRAMES;f++){
        unsigned o=rd32(payload+HU_FDIR+f*4u);
        if(o<HU_DATA||o>=size||o<=last)return -1;
        last=o;
    }
    reader->data=payload;
    reader->size=size;
    reader->cached_clip=-1;
    reader->cached_book=0;
    return 0;
}
int mi_hu_decode(mi_hu_reader_t *r,unsigned frame,uint16_t palette[16],
                 uint8_t book[MI_HU_BOOK_BYTES],uint8_t selectors[MI_HU_MAP_BYTES],
                 uint8_t pixels[MI_HU_PIXELS_BYTES]){
    unsigned clip,start,end,bookstart,bookend,y,x,row;
    const uint8_t *d;
    if(!r||!r->data||!book||!selectors||frame>=MI_HU_FRAMES)return -1;
    clip=frame/HU_STEP;d=hu_desc(r,clip);
    if(palette)for(x=0;x<16;x++)palette[x]=rd16(r->data+HU_PAL+clip*32u+x*2u);
    if(r->cached_clip!=(int16_t)clip||r->cached_book!=book){
        bookstart=rd32(d+137u);bookend=hu_off(r,clip*HU_STEP);
        if(bookstart>=bookend||bookend>r->size)return -1;
        if(d[136u]&1u){
            if(bookend-bookstart!=MI_HU_BOOK_BYTES)return -1;
            copy(book,r->data+bookstart,MI_HU_BOOK_BYTES);
        }else if(hu_decode(r->data+bookstart,bookend-bookstart,d,16u,
                           MI_HU_BOOK_BYTES*2u,book,1)!=0)return -1;
        r->cached_clip=(int16_t)clip;
        r->cached_book=book;
    }
    start=hu_off(r,frame);
    if(frame+1u==MI_HU_FRAMES)end=r->size;
    else if((frame+1u)%HU_STEP==0u)end=rd32(hu_desc(r,clip+1u)+137u);
    else end=hu_off(r,frame+1u);
    if(end<=start||end>r->size)return -1;
    if(d[136u]&2u){
        if(end-start!=MI_HU_MAP_BYTES)return -1;
        copy(selectors,r->data+start,MI_HU_MAP_BYTES);
    }else if(hu_decode(r->data+start,end-start,d+8u,256u,
                       MI_HU_MAP_BYTES,selectors,0)!=0)return -1;
    if(pixels){
        for(y=0;y<24u;y++)for(x=0;x<12u;x++){
            unsigned addr=selectors[y*12u+x]*16u;
            for(row=0;row<4u;row++){
                unsigned off=(y*4u+row)*48u+x*4u;
                copy(pixels+off,book+addr+row*4u,4u);
            }
        }
    }
    return 0;
}
