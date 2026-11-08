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
/* Original fixed palette from the user's archival FM-1 96x96 VQ pack.
 * Match colour indices to the actual codebooks; never replace the 24
 * archive palettes with unrelated cool blue/purple colours. The legacy
 * recoloured res.bin uses the SAME index maps, so it renders correctly. */
static const uint16_t mi_photo_source_palette[16] = {
    0x0000u,0x1841u,0x28a2u,0x5101u,
    0x4184u,0x49c5u,0x6206u,0x7a05u,
    0x89e4u,0x8b0au,0xa2c6u,0xd2c6u,
    0xabacu,0xec49u,0xdd50u,0xfdd1u
};
static const uint16_t mi_tile_bank_first[8]={0u,81u,145u,245u,326u,407u,488u,558u};
static const uint8_t mi_tile_bank_count[8]={81u,64u,100u,81u,81u,81u,70u,70u};
#endif
