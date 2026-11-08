/* SPDX-License-Identifier: GPL-3.0-only
 * Candidate MTL1 tile animation decoder. Not wired into firmware.
 * Decodes the existing VQ frames exactly; does not recover original RGB.
 */
#ifndef MI_SHARED_TILE_CODEC_POC_H
#define MI_SHARED_TILE_CODEC_POC_H
#include <stdint.h>
#define MI_TP_FRAMES 628u
#define MI_TP_PALETTE 16u
#define MI_TP_BOOK_BYTES 4096u
#define MI_TP_MAP_BYTES 288u
#define MI_TP_PIXELS_BYTES 4608u
#define MI_TP_MAX_PAYLOAD (69u*4096u-32u)
/* Caller provides storage outside the interrupt. Do not call from audio IRQ. */
/* Optional cached clip book: caller initializes *cached_clip = -1. A hit skips
 * 4096-byte dictionary inflate; selectors still decode per frame. The caller
 * must invalidate on resource replacement / missing resources. */
int mi_tp_decode_frame_cached(const uint8_t *payload, uint32_t len, uint16_t frame,
                              uint16_t palette[MI_TP_PALETTE],
                              uint8_t pixels[MI_TP_PIXELS_BYTES],
                              uint8_t book[MI_TP_BOOK_BYTES],
                              uint8_t selectors[MI_TP_MAP_BYTES],
                              int16_t *cached_clip);

int mi_tp_decode_frame(const uint8_t *payload, uint32_t len, uint16_t frame,
                       uint16_t palette[MI_TP_PALETTE],
                       uint8_t pixels[MI_TP_PIXELS_BYTES],
                       uint8_t book[MI_TP_BOOK_BYTES],
                       uint8_t selectors[MI_TP_MAP_BYTES]);
#endif
