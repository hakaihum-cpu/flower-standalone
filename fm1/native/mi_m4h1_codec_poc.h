/* SPDX-License-Identifier: GPL-3.0-only
 * Experimental 96x96, 16-colour, 4x4 tile / 64-codeword / Huffman renderer.
 * Reference source input is user-provided images. No AI-generated art.
 * NOT a source-perfect RGB or 240x240 reproduction.
 */
#ifndef MI_M4H1_CODEC_POC_H
#define MI_M4H1_CODEC_POC_H
#include <stdint.h>
#define MI_M4H1_MAX_BYTES (69u * 4096u - 32u)
#define MI_M4H1_PIXELS_BYTES 4608u
#define MI_M4H1_TILE_BOOK_BYTES 512u
#define MI_M4H1_FRAMES 628u
#define MI_M4H1_CLIPS 24u
#define MI_M4H1_CODES 64u
#define MI_M4H1_MAXLEN 15u
typedef struct {
    uint8_t tilebook[MI_M4H1_TILE_BOOK_BYTES];
    uint16_t palette[16];
    uint16_t count[MI_M4H1_MAXLEN+1u];
    uint16_t first_code[MI_M4H1_MAXLEN+1u];
    uint16_t first_symbol[MI_M4H1_MAXLEN+1u];
    uint8_t symbols[MI_M4H1_CODES];
    int16_t clip;
} mi_m4h1_state_t;
void mi_m4h1_init(mi_m4h1_state_t *state);
/* 4bpp output; left pixel in high nibble. Returns 0 on success, negative on
 * corrupt or incomplete source. Call only from main UI (never audio ISR).
 * Caller must invalidate the clip cache when a resource is replaced.
 */
int mi_m4h1_decode(const uint8_t *payload,uint32_t bytes,uint16_t frame,
                   mi_m4h1_state_t *state,
                   uint8_t pixels[MI_M4H1_PIXELS_BYTES]);
#endif
