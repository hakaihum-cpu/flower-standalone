/* Host-only M4H9 96-codeword regression. Does NOT build firmware. */
#include "mi_m4h9_codec_poc.h"
#include <stdio.h>
#include <stdint.h>
static uint8_t source[MI_M4H9_MAX_BYTES];
static uint8_t pixels[MI_M4H9_PIXELS];
static uint32_t crcs[MI_M4H9_FRAMES];
static mi_m4h9_state_t state;
static uint32_t crc_add(uint32_t c,const uint8_t *p,unsigned n){
    unsigned i,j;
    for(i=0;i<n;i++){
        c^=p[i];
        for(j=0;j<8u;j++)c=(c>>1u)^((c&1u)?0xEDB88320u:0u);
    }
    return c;
}
static uint32_t digest(void){
    uint32_t c=~0u;unsigned j;
    for(j=0;j<16u;j++){
        uint8_t le[2]={(uint8_t)state.palette[j],(uint8_t)(state.palette[j]>>8u)};
        c=crc_add(c,le,2u);
    }
    return ~crc_add(c,pixels,MI_M4H9_PIXELS);
}
int main(int argc,char **argv){
    FILE *f;long len;unsigned i,fr;uint32_t all=~0u;
    if(argc!=2)return 2;
    f=fopen(argv[1],"rb");if(!f)return 3;
    if(fseek(f,0,SEEK_END)){fclose(f);return 4;}
    len=ftell(f);if(len!=275443){fclose(f);return 5;}
    rewind(f);
    if(fread(source,1,(size_t)len,f)!=(size_t)len){fclose(f);return 6;}
    fclose(f);mi_m4h9_reset(&state);
    for(fr=0;fr<MI_M4H9_FRAMES;fr++){
        unsigned j;
        if(mi_m4h9_decode(source,(uint32_t)len,(uint16_t)fr,&state,pixels))return 7;
        crcs[fr]=digest();
        for(j=0;j<16u;j++){
            uint8_t p[2]={(uint8_t)state.palette[j],(uint8_t)(state.palette[j]>>8u)};
            all=crc_add(all,p,2u);
        }
        all=crc_add(all,pixels,MI_M4H9_PIXELS);
    }
    if((~all)!=0xCB90E9DAu){printf("bad decoded CRC %08x\n",~all);return 8;}
    mi_m4h9_reset(&state);
    for(i=0;i<MI_M4H9_FRAMES;i++){
        fr=(i*237u)%MI_M4H9_FRAMES;
        if(mi_m4h9_decode(source,(uint32_t)len,(uint16_t)fr,&state,pixels)
           ||digest()!=crcs[fr])return 9;
    }
    mi_m4h9_reset(&state);
    for(i=0;i<MI_M4H9_FRAMES;i++){
        fr=MI_M4H9_FRAMES-1u-i;
        if(mi_m4h9_decode(source,(uint32_t)len,(uint16_t)fr,&state,pixels)
           ||digest()!=crcs[fr])return 10;
    }
    if(!mi_m4h9_decode(source,(uint32_t)len,MI_M4H9_FRAMES,&state,pixels))return 11;
    source[0]^=255u;
    if(!mi_m4h9_decode(source,(uint32_t)len,0u,&state,pixels))return 12;
    source[0]^=255u;
    for(i=0;i<4u;i++)source[21536u+i]=255u;
    if(!mi_m4h9_decode(source,(uint32_t)len,0u,&state,pixels))return 13;
    puts("PASS: 628 forward/random/reverse; CRC CB90E9DA; malformed input rejected");
    return 0;
}
