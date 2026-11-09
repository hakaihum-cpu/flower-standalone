/* SPDX-License-Identifier: GPL-3.0-only
 * Experimental MHU1 lossless VQ decoder. NOT production firmware.
 */
#ifndef MI_HU_CODEC_POC_H
#define MI_HU_CODEC_POC_H
#include <stdint.h>
#define MI_HU_FRAMES 628u
#define MI_HU_CLIPS 24u
#define MI_HU_BOOK_BYTES 4096u
#define MI_HU_MAP_BYTES 288u
#define MI_HU_PIXELS_BYTES 4608u
#define MI_HU_RESOURCE_MAX 282592u
typedef struct {
    const uint8_t *data;
    uint32_t size;
    int16_t cached_clip;
    uint8_t *cached_book;
} mi_hu_reader_t;
/* Check header, full CRC32, and frame/dictionary directory bounds once. */
int mi_hu_open(mi_hu_reader_t *reader, const uint8_t *payload, uint32_t size);
/* Decode frame exactly as the approved 96x96 4bpp VQ. No heap allocations.
 * Palette/pixels may be NULL; book and selector buffers are required.
 * The cached dictionary remains valid only with the same book pointer.
 */
int mi_hu_decode(mi_hu_reader_t *reader, unsigned frame, uint16_t palette[16],
                 uint8_t book[MI_HU_BOOK_BYTES],
                 uint8_t selectors[MI_HU_MAP_BYTES],
                 uint8_t pixels[MI_HU_PIXELS_BYTES]);
#endif
