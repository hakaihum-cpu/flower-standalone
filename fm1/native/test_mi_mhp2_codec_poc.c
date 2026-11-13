/* Host-only roundtrip test of FM-1 SEL MHP2 against original VQ data.
 * Not linked in target. Run with reference VQ and MHP2 payload paths.
 */
#include "mi_mhp2_codec_poc.h"
#include "mi_mhp2_patch_poc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t reference[279936u];
static uint8_t encoded[MI_MHP2_RESOURCE_MAX];
static uint8_t pixels[MI_MHP2_FRAME_BYTES];
static uint8_t expected[MI_MHP2_FRAME_BYTES];
static uint8_t overlay[100000u];
static uint8_t expected_patched[MI_MHP2_FRAME_COUNT*MI_MHP2_FRAME_BYTES];
static uint32_t overlay_size;
static mi_mhp2_t decoder;
static int check(uint16_t f) {
    unsigned clip=(unsigned)f/27u,ty,tx,row,col,i;
    const uint8_t *books=reference+768u+clip*4096u;
    const uint8_t *maps=reference+99072u+(uint32_t)f*288u;
    if(!mi_mhp2_decode(&decoder,f,pixels))return 0;
    for(i=0;i<16u;i++){
        uint16_t original=(uint16_t)((uint16_t)reference[clip*32u+i*2u]|
                    ((uint16_t)reference[clip*32u+i*2u+1u]<<8u));
        if(original!=decoder.palette[i])return 0;
    }
    for(ty=0u;ty<24u;ty++)for(tx=0u;tx<12u;tx++){
        uint32_t at=(uint32_t)maps[ty*12u+tx]*16u;
        for(row=0u;row<4u;row++)for(col=0u;col<4u;col++)
            expected[(ty*4u+row)*48u+tx*4u+col]=books[at+row*4u+col];
    }
    if(memcmp(pixels,expected,sizeof pixels)!=0)return 0;
    if(overlay_size){
        if(!mi_mhp2_apply_patch(overlay,overlay_size,f,pixels))return 0;
        if(memcmp(pixels,expected_patched+(uint32_t)f*MI_MHP2_FRAME_BYTES,
                  MI_MHP2_FRAME_BYTES)!=0)return 0;
    }
    return 1;
}
static size_t readfile(const char *path, uint8_t *out,size_t cap) {
    size_t n;
    FILE *fp=fopen(path,"rb");
    if(!fp){perror(path);exit(1);}
    n=fread(out,1,cap,fp);
    if(n==cap && fgetc(fp)!=EOF){fprintf(stderr,"Input too large: %s\n",path);exit(1);}
    if(ferror(fp)){perror("fread");exit(1);}
    fclose(fp);
    return n;
}
int main(int argc,char **argv){
    size_t refsize,encsize;
    uint32_t rnd=0x0a4e9b15u;
    unsigned i;
    if(argc!=3&&argc!=5){fprintf(stderr,"USAGE: test_mhp2 reference.vq encoded.mhp2 [overlay expected_patched]\n");return 2;}
    refsize=readfile(argv[1],reference,sizeof reference);
    encsize=readfile(argv[2],encoded,sizeof encoded);
    if(argc==5){
        size_t checksize;
        overlay_size=(uint32_t)readfile(argv[3],overlay,sizeof overlay);
        checksize=readfile(argv[4],expected_patched,sizeof expected_patched);
        if(!overlay_size||checksize!=sizeof expected_patched){
            fprintf(stderr,"MHP2 source overlay test fixtures invalid\n");return 1;
        }
    }
    if(refsize!=sizeof reference||!mi_mhp2_init(&decoder,encoded,(uint32_t)encsize)){
        fprintf(stderr,"source format or MHP2 header invalid\n");return 1;
    }
    for(i=0;i<MI_MHP2_FRAME_COUNT;i++)if(!check((uint16_t)i)){
        fprintf(stderr,"forward mismatch %u\n",i);return 1;
    }
    for(i=MI_MHP2_FRAME_COUNT;i-->0u;)if(!check((uint16_t)i)){
        fprintf(stderr,"reverse mismatch %u\n",i);return 1;
    }
    for(i=0;i<MI_MHP2_FRAME_COUNT;i++){
        rnd^=rnd<<13;rnd^=rnd>>17;rnd^=rnd<<5;
        if(!check((uint16_t)(rnd%MI_MHP2_FRAME_COUNT))){
            fprintf(stderr,"random mismatch %u\n",i);return 1;
        }
    }
    encoded[encsize-1u]^=0x80u;
    if(mi_mhp2_init(&decoder,encoded,(uint32_t)encsize)){
        fprintf(stderr,"accepted corrupted resource CRC\n");return 1;
    }
    printf("PASS C MHP2: 628 forward + 628 reverse + 628 random; exact 96x96 packed pixels + 16 RGB565 palette; rejects corrupt CRC\n");
    return 0;
}