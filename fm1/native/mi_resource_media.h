/* SPDX-License-Identifier: GPL-3.0-only
 * 628 original source images, 96x96, 16 RGB565 colours, 8x4 VQ,
 * 256 codewords every 27 frames; validated stock FM-1 res.bin.
 */
#ifndef MI_RESOURCE_MEDIA_H
#define MI_RESOURCE_MEDIA_H
#include <stdint.h>
#define MI_TILE_BANKS 8u
#define MI_TILE_COUNT 628u
#define MI_TILE_SIDE 96u
#define MI_TILE_VQ_CLIPS 24u
#define MI_TILE_VQ_FRAMES_PER_CLIP 27u
#define MI_TILE_VQ_COLS 12u
#define MI_TILE_VQ_ROWS 24u
#define MI_TILE_VQ_MAP_STRIDE 288u
#define MI_TILE_VQ_PALETTE_OFFSET 0u
#define MI_TILE_VQ_BOOK_OFFSET 768u
#define MI_TILE_VQ_MAP_OFFSET 99072u
#define MI_TILE_DATA_BYTES 279936u
static const uint16_t mi_tile_bank_first[8]={0u,81u,145u,245u,326u,407u,488u,558u};
static const uint8_t mi_tile_bank_count[8]={81u,64u,100u,81u,81u,81u,70u,70u};
#endif
