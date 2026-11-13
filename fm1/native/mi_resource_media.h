/* SPDX-License-Identifier: GPL-3.0-only
 * 628 original-source 96x96 / 16 RGB565 colours / MHP2 8x4 VQ resource.
 * All encoding occurs before build; target runs a freestanding bounded decoder.
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
#define MI_TILE_MHP2_CORE_BYTES 203991u
#define MI_TILE_MHP2_OVERLAY_BYTES 77576u
#define MI_TILE_MHP2_PATCH_COUNT 4170u
#define MI_TILE_DATA_BYTES 281567u
#define MI_TILE_RESOURCE_RGB_SOURCE 1u
/* MHP2 payload SHA256 ea3d75e1f8e5865eb488661edbe709bf11cd3b1539734ebf8e56ca5b58625d4c.
 * The resource installer adds its standard CRC-checked 32-byte header.
 * Settings sectors must not move or be overwritten. */
static const uint16_t mi_tile_bank_first[8]={0u,81u,145u,245u,326u,407u,488u,558u};
static const uint8_t mi_tile_bank_count[8]={81u,64u,100u,81u,81u,81u,70u,70u};
#endif
