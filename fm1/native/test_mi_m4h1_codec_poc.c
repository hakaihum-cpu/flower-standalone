/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only M4H1 test, run manually on candidate from user source images.
 * Never builds or flashes firmware.
 */
#include "mi_m4h1_codec_poc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t candidate[MI_M4H1_MAX_BYTES];
static uint8_t decoded[MI_M4H1_PIXELS_BYTES];
static mi_m4h1_state_t state;
static uint32_t crc_add(uint32_t c,const uint8_t *p,uint32_t n)
{
    uint32_t i,j;
    for(i=0u;i<n;i++){
        c^=p[i];
        for(j=0u;j<8u;j++)c=(c>>1u)^((c&1u)?0xEDB88320u:0u);
    }
    return c;
}
static uint32_t add_frame(uint32_t crc)
{
    unsigned i;
    for(i=0u;i<16u;i++){
        uint8_t le[2]={(uint8_t)state.palette[i],
                        (uint8_t)(state.palette[i]>>8u)};
        crc=crc_add(crc,le,2u);
    }
    return crc_add(crc,decoded,MI_M4H1_PIXELS_BYTES);
}
int main(int argc,char **argv)
{
    FILE *f;long bytes;uint32_t frame_crc[628],crc=~0u,index,frame;
    if(argc!=2)return 2;
    f=fopen(argv[1],"rb");if(!f)return 3;
    if(fseek(f,0,SEEK_END))return 4;
    bytes=ftell(f);
    if(bytes!=254985)return 5;
    if(fseek(f,0,SEEK_SET)||
       fread(candidate,1,(size_t)bytes,f)!=(size_t)bytes)return 6;
    fclose(f);
    mi_m4h1_init(&state);
    for(frame=0u;frame<628u;frame++){
        if(mi_m4h1_decode(candidate,(uint32_t)bytes,(uint16_t)frame,
                           &state,decoded))return 7;
        frame_crc[frame]=~add_frame(~0u);
        crc=add_frame(crc);
    }
    if((~crc)!=0x8FF4A721u)return 8;
    mi_m4h1_init(&state);
    for(index=0u;index<628u;index++){
        frame=(index*237u)%628u;
        if(mi_m4h1_decode(candidate,(uint32_t)bytes,(uint16_t)frame,
                           &state,decoded)||~add_frame(~0u)!=frame_crc[frame])return 9;
    }
    mi_m4h1_init(&state);
    for(index=0u;index<628u;index++){
        frame=627u-index;
        if(mi_m4h1_decode(candidate,(uint32_t)bytes,(uint16_t)frame,
                           &state,decoded)||~add_frame(~0u)!=frame_crc[frame])return 10;
    }
    if(!mi_m4h1_decode(candidate,(uint32_t)bytes,628u,&state,decoded))return 11;
    candidate[0]^=255u;
    if(!mi_m4h1_decode(candidate,(uint32_t)bytes,0u,&state,decoded))return 12;
    candidate[0]^=255u;
    for(index=0u;index<4u;index++)candidate[14624u+index]=255u;
    if(!mi_m4h1_decode(candidate,(uint32_t)bytes,0u,&state,decoded))return 13;
    puts("PASS: 628 forward + 628 shuffled + 628 backward; CRC 8FF4A721; corrupt inputs rejected");
    return 0;
}
