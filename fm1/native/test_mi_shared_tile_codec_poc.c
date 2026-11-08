/* FM-1 MTL1 decoded-pixel parity test. Host-only; no firmware build. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mi_shared_tile_codec_poc.h"
static uint8_t current[279936];
static uint8_t coded[MI_TP_MAX_PAYLOAD];
static uint8_t out[MI_TP_PIXELS_BYTES], book[MI_TP_BOOK_BYTES], ids[MI_TP_MAP_BYTES];
static uint8_t ref[MI_TP_PIXELS_BYTES];
static uint16_t colors[16];
static int16_t cached_clip=-1;
static int verify(unsigned frame,unsigned size){
 unsigned clip=frame/27u,ty,tx,row,col,k,i;
 if(mi_tp_decode_frame_cached(coded,size,(uint16_t)frame,colors,out,book,ids,
                             &cached_clip))return -1;
 for(i=0;i<16;i++){
  k=current[clip*32u+i*2u]|((unsigned)current[clip*32u+i*2u+1u]<<8u);
  if(colors[i]!=(uint16_t)k)return -2;
 }
 for(ty=0;ty<24;ty++)for(tx=0;tx<12;tx++){
  unsigned id=current[99072u+frame*288u+ty*12u+tx];
  const uint8_t *tile=current+768u+clip*4096u+id*16u;
  for(row=0;row<4;row++)for(col=0;col<4;col++)
   ref[(ty*4u+row)*48u+tx*4u+col]=tile[row*4u+col];
 }
 return memcmp(ref,out,MI_TP_PIXELS_BYTES)?-3:0;
}
int main(int argc,char **argv){
 FILE *fp;long n;unsigned f,i;
 if(argc!=3)return 5;
 fp=fopen(argv[1],"rb");if(!fp)return 6;
 if(fread(current,1,sizeof(current),fp)!=sizeof(current)){fclose(fp);return 7;}fclose(fp);
 fp=fopen(argv[2],"rb");if(!fp)return 8;
 fseek(fp,0,SEEK_END);n=ftell(fp);fseek(fp,0,SEEK_SET);
 if(n<=0||n>MI_TP_MAX_PAYLOAD){fclose(fp);return 9;}
 if(fread(coded,1,(size_t)n,fp)!=(size_t)n){fclose(fp);return 10;}fclose(fp);
 for(f=0;f<MI_TP_FRAMES;f++)if(verify(f,(unsigned)n)){printf("bad sequential %u\n",f);return 11;}
 for(i=0;i<MI_TP_FRAMES;i++){
  f=(i*237u)%MI_TP_FRAMES; /* coprime multiplier: all 628 frames exactly once */
  if(verify(f,(unsigned)n)){printf("bad random %u\n",f);return 12;}
 }
 cached_clip=-1;
 for(i=0;i<MI_TP_FRAMES;i++){
  f=MI_TP_FRAMES-1u-i;
  if(verify(f,(unsigned)n)){printf("bad reverse %u\n",f);return 15;}
 }
 /* Uncached API must remain bit-identical for independent legacy tests. */
 if(mi_tp_decode_frame(coded,(unsigned)n,0,colors,out,book,ids))return 16;
 coded[0]^=0xffu;
 if(mi_tp_decode_frame(coded,(unsigned)n,0,colors,out,book,ids)==0)return 13;
 coded[0]^=0xffu;
 coded[28u+24u*32u+24u*8u]=0xffu;
 coded[28u+24u*32u+24u*8u+1u]=0xffu;
 coded[28u+24u*32u+24u*8u+2u]=0xffu;
 coded[28u+24u*32u+24u*8u+3u]=0xffu;
 if(mi_tp_decode_frame(coded,(unsigned)n,0,colors,out,book,ids)==0)return 14;
 printf("PASS: cached 628 sequential + 628 shuffled + 628 reverse, legacy API and corrupt-input checks; bytes %ld\n",n);
 return 0;
}
