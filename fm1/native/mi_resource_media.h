/* SPDX-License-Identifier: GPL-3.0-only
 * 645 border-aligned Runway-colored 96x96 / 16 RGB565 colours / MHP2 8x4 VQ resource.
 * All encoding occurs before build; target runs a freestanding bounded decoder.
 */
#ifndef MI_RESOURCE_MEDIA_H
#define MI_RESOURCE_MEDIA_H
#include <stdint.h>
#define MI_TILE_BANKS 8u
#define MI_TILE_COUNT 645u
#define MI_TILE_SIDE 96u
#define MI_TILE_VQ_CLIPS 24u
#define MI_TILE_VQ_FRAMES_PER_CLIP 27u
#define MI_TILE_VQ_COLS 12u
#define MI_TILE_VQ_ROWS 24u
#define MI_TILE_VQ_MAP_STRIDE 288u
#define MI_TILE_MHP2_CORE_BYTES 203248u
#define MI_TILE_MHP2_OVERLAY_BYTES 79336u
#define MI_TILE_MHP2_PATCH_COUNT 4264u
#define MI_TILE_DATA_BYTES 282584u
#define MI_TILE_RESOURCE_RGB_SOURCE 1u
/* MHP2 payload SHA256 a83500a38d95fa341b998f09d1ef1d8cf05ae04bea9879e3476cb42ac631d74e.
 * The resource installer adds its standard CRC-checked 32-byte header.
 * Settings sectors must not move or be overwritten. */
static const uint16_t mi_tile_bank_first[8]={0u,90u,162u,243u,343u,424u,505u,575u};
static const uint8_t mi_tile_bank_count[8]={90u,72u,81u,100u,81u,81u,70u,70u};
#endif
