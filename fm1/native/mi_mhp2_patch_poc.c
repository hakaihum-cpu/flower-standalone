/* SPDX-License-Identifier: GPL-3.0-only
 * Freestanding, no-heap 8x4 source-derived correction patch decoder.
 */
#include "mi_mhp2_patch_poc.h"
#define FRAME_DIR_BYTES (646u*4u)
static uint32_t little32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|
           ((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
static uint16_t little16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8u));
}
int mi_mhp2_apply_patch(const uint8_t *overlay, uint32_t size,
                        uint16_t frame, uint8_t out_pixels[MI_MHP2_FRAME_BYTES]) {
    uint32_t start,end,limit,offset;
    if(!overlay||!out_pixels||frame>=MI_MHP2_FRAME_COUNT||size<FRAME_DIR_BYTES)return 0;
    limit=size-FRAME_DIR_BYTES;
    start=little32(overlay+(uint32_t)frame*4u);
    end=little32(overlay+((uint32_t)frame+1u)*4u);
    if(start>end||end>limit||(end-start)%18u)return 0;
    for(offset=start;offset<end;offset+=18u){
        const uint8_t *tile=overlay+FRAME_DIR_BYTES+offset;
        uint16_t id=little16(tile);
        unsigned ty,tx,row,col;
        if(id>=MI_MHP2_MAP_BYTES)return 0;
        ty=(unsigned)id/12u;
        tx=(unsigned)id%12u;
        for(row=0u;row<4u;row++)for(col=0u;col<4u;col++)
            out_pixels[(ty*4u+row)*48u+tx*4u+col]=tile[2u+row*4u+col];
    }
    return 1;
}