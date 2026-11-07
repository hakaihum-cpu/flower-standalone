/* SPDX-License-Identifier: GPL-3.0-only
 * FM-1 standard resources via fm1-base plat_res() / FM1B_RES.
 * The first 92,928 bytes preserve the SAME RGB565 16-colour 8x4 VQ data
 * previously stored in mi_tile_atlas.h; no new compression format.
 * The upstream 32-byte resource-region header and CRC are supplied by
 * fm1-base tools/build.py and device/res.c, not by the application.
 */
#ifndef MI_RESOURCE_MEDIA_H
#define MI_RESOURCE_MEDIA_H
#include <stdint.h>
#define MI_TILE_BANKS 6u
#define MI_TILE_COUNT 488u
#define MI_TILE_SIDE 48u
#define MI_TILE_VQ_CLIPS 14u
#define MI_TILE_VQ_FRAMES_PER_CLIP 36u
#define MI_TILE_VQ_COLS 6u
#define MI_TILE_VQ_ROWS 12u
#define MI_TILE_VQ_MAP_STRIDE 72u
#define MI_TILE_VQ_PALETTE_OFFSET 0u
#define MI_TILE_VQ_BOOK_OFFSET 448u
#define MI_TILE_VQ_MAP_OFFSET 57792u
#define MI_TILE_DATA_BYTES 92928u
static const uint16_t mi_tile_bank_first[6]={0u,81u,145u,245u,326u,407u};
static const uint8_t mi_tile_bank_count[6]={81u,64u,100u,81u,81u,81u};
#endif
