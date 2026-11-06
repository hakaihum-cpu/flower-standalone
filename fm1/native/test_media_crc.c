/* SPDX-License-Identifier: GPL-3.0-only
 * Independent C decoder audit against Python's decoded RGB565 palette-index
 * CRC for EVERY original-source MIYAKO image. No image generation.
 * This test is host-only and must pass before any FM-1 FWSC packaging.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mi_effects_vq_frames.h"
#include "mi_eureka_vq_frames.h"

enum { SOURCE_WIDTH = 128, SOURCE_HEIGHT = 78,
       SOURCE_TILE_W = 8, SOURCE_TILE_H = 4, SOURCE_TILE_COLS = 16,
       SOURCE_TILE_ROWS = 20, SOURCE_BYTES = 4992 };
typedef char audit_effects_frames[(MI_EFFECTS_ASSET_COUNT == 144u) ? 1 : -1];
typedef char audit_eureka_frames[(MI_EUREKA_ASSET_COUNT == 576u) ? 1 : -1];
typedef char audit_eureka_geometry[(MI_EUREKA_ASSET_W == SOURCE_WIDTH &&
    MI_EUREKA_ASSET_H == SOURCE_HEIGHT &&
    MI_EUREKA_ASSET_BPP == 4u &&
    MI_EUREKA_ASSET_COLORS == 16u) ? 1 : -1];
typedef char audit_effects_geometry[(MI_EFFECTS_ASSET_W == SOURCE_WIDTH &&
    MI_EFFECTS_ASSET_H == SOURCE_HEIGHT &&
    MI_EFFECTS_ASSET_BPP == 4u &&
    MI_EFFECTS_ASSET_COLORS == 16u) ? 1 : -1];

static uint32_t crc_update(uint32_t crc, const uint8_t *src, unsigned count)
{
    unsigned i, bit;
    for (i=0;i<count;i++) {
        crc ^= src[i];
        for (bit=0;bit<8u;bit++)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return crc;
}

static uint32_t check_bank(unsigned frames, unsigned clip_count,
                           const uint8_t *books, const uint8_t *maps,
                           uint32_t reference, const char *name)
{
    uint8_t result[SOURCE_BYTES];
    uint32_t crc = 0xffffffffu;
    unsigned frame, clip, tx, ty, row;
    for (frame=0;frame<frames;frame++) {
        const uint8_t *map = maps + frame * 320u;
        clip = frame / 36u;
        if (clip >= clip_count) return 0u;
        memset(result, 0, sizeof result);
        for (ty=0;ty<SOURCE_TILE_ROWS;ty++) {
            for (tx=0;tx<SOURCE_TILE_COLS;tx++) {
                const uint8_t *tile =
                    books + ((clip * 256u + map[ty*16u+tx]) * 16u);
                for (row=0;row<SOURCE_TILE_H;row++) {
                    unsigned y=ty*SOURCE_TILE_H+row;
                    unsigned offset;
                    if (y>=SOURCE_HEIGHT) break;
                    offset=y*(SOURCE_WIDTH/2u)+tx*4u;
                    memcpy(&result[offset], &tile[row*4u], 4u);
                }
            }
        }
        crc=crc_update(crc,result,sizeof result);
    }
    crc ^= 0xffffffffu;
    if (crc!=reference) {
        fprintf(stderr,"FAIL %s: C decoded %08lx != Python %08lx\n",
                name,(unsigned long)crc,(unsigned long)reference);
        return 0u;
    }
    printf("PASS %s %u frames, 16 colours, RGB565 palette indices, CRC32 %08lx\n",
           name,frames,(unsigned long)crc);
    return 1u;
}

int main(void)
{
    if (!check_bank(MI_EFFECTS_ASSET_COUNT,4u,
        &mi_effects_vq_tilebook[0][0][0],
        &mi_effects_vq_maps[0][0],
        MI_EFFECTS_DECODED_CRC32,"EFFECTS")) return 1;
    if (!check_bank(MI_EUREKA_ASSET_COUNT,16u,
        &mi_eureka_vq_tilebook[0][0][0],
        &mi_eureka_vq_maps[0][0],
        MI_EUREKA_DECODED_CRC32,"EUREKA")) return 1;
    return 0;
}
