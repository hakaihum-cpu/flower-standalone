/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only exact VQ tilebook -> 576 decoded 4bpp frames test.
 * Requires the *experimental* generated mi_eureka_vq_frames.h in the include
 * path. This does NOT assert that lossy VQ reproduces its original JPEG pixels.
 * Run with: cc -std=c99 -O1 -g -Wall -Wextra -Werror
 * -fsanitize=address,undefined -fno-sanitize-recover=all
 * -o /tmp/vq_decoder fm1/karma/test_eureka_vq_decoder.c
 * (from the repository root, after generating the candidate header).
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "mi_eureka_vq_decoder.c"
int main(void)
{
    uint32_t frame, ty, tx, row, b;
    assert(sizeof(mi_eureka_vq_tilebook) == 16u * 256u * 16u);
    assert(sizeof(mi_eureka_vq_maps) == 576u * 320u);
    assert(sizeof(mi_eureka_vq_palette) == 16u * 16u * 2u);
    assert(!mi_vq_seek(576u));
    for (frame = 0u; frame < 576u; ++frame) {
        assert(mi_vq_seek(frame));
        assert(mi_vq_palette == mi_eureka_vq_palette[frame / 36u]);
        for (ty = 0u; ty < 20u; ++ty) {
            for (tx = 0u; tx < 16u; ++tx) {
                const uint8_t *tile =
                    mi_eureka_vq_tilebook[frame / 36u]
                       [mi_eureka_vq_maps[frame][ty * 16u + tx]];
                for (row = 0u; row < 4u; ++row) {
                    uint32_t y = ty * 4u + row;
                    if (y >= 78u) continue;
                    for (b = 0u; b < 4u; ++b)
                        assert(mi_vq_pixels[y * 64u + tx * 4u + b] ==
                               tile[row * 4u + b]);
                }
            }
        }
    }
    assert(mi_vq_seek(575u));
    printf("PASS RGB565 VQ: all 576 frame slots reconstruct from their "
           "256-tile clip dictionaries without decoder errors\n");
    return 0;
}
