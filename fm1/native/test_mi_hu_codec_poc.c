/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only byte/pixel regression for experimental MHU1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mi_hu_codec_poc.h"
static uint8_t source[279936],resource[MI_HU_RESOURCE_MAX];
static uint8_t book[MI_HU_BOOK_BYTES],selectors[MI_HU_MAP_BYTES];
static uint8_t pixels[MI_HU_PIXELS_BYTES];
static unsigned seed=0x4d49u;
static unsigned rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void must(int pass,const char *msg,unsigned frame){
    if(!pass){fprintf(stderr,"FAIL: %s frame=%u\n",msg,frame);exit(1);}
}
static unsigned readfile(const char *name,uint8_t *dst,unsigned cap){
    FILE *f=fopen(name,"rb");unsigned n;
    if(!f){perror(name);exit(1);}
    n=(unsigned)fread(dst,1,cap,f);
    must(!ferror(f)&&fgetc(f)==EOF,"file too long or read error",0);
    fclose(f);return n;
}
static void verify(mi_hu_reader_t *r,unsigned frame){
    const unsigned clip=frame/27u;
    const uint8_t *pal=source+clip*32u;
    const uint8_t *oldbook=source+768u+clip*4096u;
    const uint8_t *oldmap=source+99072u+frame*288u;
    uint16_t colors[16];unsigned i,ty,tx,row;
    must(mi_hu_decode(r,frame,colors,book,selectors,pixels)==0,"frame decode",frame);
    for(i=0;i<16u;i++)
        must(colors[i]==(unsigned)(pal[i*2u]|(pal[i*2u+1u]<<8)),"palette",frame);
    must(memcmp(book,oldbook,4096u)==0,"book changed",frame);
    must(memcmp(selectors,oldmap,288u)==0,"selectors changed",frame);
    for(ty=0;ty<24u;ty++)for(tx=0;tx<12u;tx++){
        unsigned tid=oldmap[ty*12u+tx];
        for(row=0;row<4u;row++){
            unsigned y=(ty*4u+row)*48u+tx*4u,x=tid*16u+row*4u;
            must(memcmp(pixels+y,oldbook+x,4u)==0,"pixel changed",frame);
        }
    }
}
int main(int argc,char **argv){
    mi_hu_reader_t r={0};unsigned i,n,order[628];
    if(argc!=3){fprintf(stderr,"usage: test old-vq.bin compressed.mhu1\n");return 2;}
    must(readfile(argv[1],source,sizeof(source))==sizeof(source),"source size",0);
    n=readfile(argv[2],resource,sizeof(resource));
    must(mi_hu_open(&r,resource,n)==0,"open",0);
    for(i=0;i<628u;i++)verify(&r,i);
    for(i=0;i<628u;i++)order[i]=i;
    for(i=628u;i>1u;i--){unsigned j=rnd()%i,t=order[i-1u];order[i-1u]=order[j];order[j]=t;}
    for(i=0;i<628u;i++)verify(&r,order[i]);
    for(i=628u;i>0u;i--)verify(&r,i-1u);
    resource[0]^=1u;
    must(mi_hu_open(&r,resource,n)!=0,"reject header corruption",0);
    resource[0]^=1u;
    resource[4184]^=1u;
    must(mi_hu_open(&r,resource,n)!=0,"reject corrupt index/CRC",0);
    resource[4184]^=1u;
    must(mi_hu_open(&r,resource,n-1u)!=0,"reject truncation",0);
    printf("PASS C: 628 forward/shuffled/reverse, pixel-perfect VQ, malformed input rejected, %u B\n",n);
    return 0;
}
