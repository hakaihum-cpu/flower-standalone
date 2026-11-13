/* SPDX-License-Identifier: GPL-3.0-only
 * Experimental MHP2 96x96 SEL resource reader, NOT linked in approved FM-1.
 * The host-only encoder resides in fm1/native/tools/pack_photo_mhp2_poc.py.
 */
#ifndef MI_MHP2_CODEC_POC_H
#define MI_MHP2_CODEC_POC_H
#include <stdint.h>
#define MI_MHP2_FRAME_COUNT 628u
#define MI_MHP2_CLIP_COUNT 24u
#define MI_MHP2_FRAME_BYTES 4608u
#define MI_MHP2_DICT_BYTES 4096u
#define MI_MHP2_MAP_BYTES 288u
#define MI_MHP2_RESOURCE_MAX 282592u
typedef struct {
    const uint8_t *blob;
    uint32_t length;
    uint16_t cached_clip;
    uint16_t palette[16];
    uint8_t book[MI_MHP2_DICT_BYTES];
    uint8_t map[MI_MHP2_MAP_BYTES];
} mi_mhp2_t;
/* Returns 1 on valid CRC-verified MHP2 payload; no heap, no libc. */
int mi_mhp2_init(mi_mhp2_t *st, const uint8_t *blob, uint32_t length);
/* Returns 1 on frame decoded. Order is arbitrary; codebooks cached by clip.
 * out_pixels contains 4-bit RGB565 palette indices, 2 pixels per byte.
 * Use st->palette to convert indices to RGB565.
 */
int mi_mhp2_decode(mi_mhp2_t *st, uint16_t frame, uint8_t out_pixels[MI_MHP2_FRAME_BYTES]);
#endif