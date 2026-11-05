/* SPDX-License-Identifier: GPL-3.0-only
 * FM-1 original 576-frame / 16-colour VQ candidate decoder.
 * NOT selected by approved firmware until full source visual/size audits pass.
 * Each frame has 320 8x4 tile indices, each clip 256 4bpp tile prototypes.
 * No heap, no floating point and no audio-thread processing.
 */
#include <stdint.h>
#include "mi_eureka_vq_frames.h"
typedef char mi_vq_frame_count[(MI_EUREKA_ASSET_COUNT == 576u) ? 1 : -1];
typedef char mi_vq_dims[(MI_EUREKA_ASSET_W == 128u &&
                         MI_EUREKA_ASSET_H == 78u &&
                         MI_EUREKA_ASSET_BYTES == 4992u &&
                         MI_EUREKA_ASSET_BPP == 4u &&
                         MI_EUREKA_ASSET_COLORS == 16u &&
                         MI_EUREKA_VQ_TILE_COUNT == 320u) ? 1 : -1];
static uint8_t mi_vq_pixels[MI_EUREKA_ASSET_BYTES];
static const uint16_t *mi_vq_palette;
static int32_t mi_vq_last_frame = -1;
static int mi_vq_seek(uint32_t frame)
{
    uint32_t ty, tx, row, byte_index;
    const uint8_t (*book)[16];
    const uint8_t *map;
    if (frame >= MI_EUREKA_ASSET_COUNT)
        return 0;
    if ((int32_t)frame == mi_vq_last_frame)
        return 1;
    book = mi_eureka_vq_tilebook[frame / 36u];
    map = mi_eureka_vq_maps[frame];
    mi_vq_palette = mi_eureka_vq_palette[frame / 36u];
    for (ty = 0u; ty < 20u; ++ty) {
        for (tx = 0u; tx < 16u; ++tx) {
            const uint8_t *tile = book[map[ty * 16u + tx]];
            for (row = 0u; row < 4u; ++row) {
                uint32_t y = ty * 4u + row;
                if (y >= 78u) break; /* two padded rows do not enter 4992B buffer */
                byte_index = y * 64u + tx * 4u;
                mi_vq_pixels[byte_index] = tile[row * 4u];
                mi_vq_pixels[byte_index + 1u] = tile[row * 4u + 1u];
                mi_vq_pixels[byte_index + 2u] = tile[row * 4u + 2u];
                mi_vq_pixels[byte_index + 3u] = tile[row * 4u + 3u];
            }
        }
    }
    mi_vq_last_frame = (int32_t)frame;
    return 1;
}
