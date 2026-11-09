/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO M4H9 experimental 96-codeword Huffman tile renderer.
 * 96x96, 16-colour, 4x4 VQ; not a lossless original RGB format.
 */
#ifndef MI_M4H9_CODEC_POC_H
#define MI_M4H9_CODEC_POC_H
#include <stdint.h>
#define MI_M4H9_FRAMES 628u
#define MI_M4H9_CODES 96u
#define MI_M4H9_PIXELS 4608u
#define MI_M4H9_BOOK 768u
#define MI_M4H9_MAX_BYTES (69u*4096u-32u)
typedef struct {
    uint8_t tiles[MI_M4H9_BOOK];
    uint8_t alphabet[MI_M4H9_CODES];
    uint16_t palette[16];
    uint16_t count[16], first_code[16], first_symbol[16];
    int16_t cached_clip;
} mi_m4h9_state_t;
void mi_m4h9_reset(mi_m4h9_state_t *state);
/* 0 = fully decoded; -1 = corrupt/incomplete. Main UI only, never audio IRQ. */
int mi_m4h9_decode(const uint8_t *source,uint32_t length,uint16_t frame,
                   mi_m4h9_state_t *state,uint8_t pixels[MI_M4H9_PIXELS]);
#endif
