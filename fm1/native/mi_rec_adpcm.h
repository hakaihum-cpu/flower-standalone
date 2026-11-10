/* SPDX-License-Identifier: GPL-3.0-only
 * Independent 64-sample IMA ADPCM blocks for a future three-track REC.
 * A block can be decoded and rewritten without touching neighboring blocks.
 */
#ifndef MI_REC_ADPCM_H
#define MI_REC_ADPCM_H
#include <stdint.h>
#define MI_REC_ADPCM_SAMPLES 64u
#define MI_REC_ADPCM_BLOCK_BYTES 40u
#define MI_REC_ADPCM_BLOCKS 690u /* ceil(44100 / 64): 36-byte ADPCM + 4-byte transient anchor */
#define MI_REC_ADPCM_TRACKS 3u
#define MI_REC_ADPCM_STORE_BYTES (MI_REC_ADPCM_TRACKS*MI_REC_ADPCM_BLOCKS*MI_REC_ADPCM_BLOCK_BYTES)
void mi_adpcm_encode_64(uint8_t out[MI_REC_ADPCM_BLOCK_BYTES],
                        const int16_t samples[MI_REC_ADPCM_SAMPLES]);
void mi_adpcm_decode_64(int16_t samples[MI_REC_ADPCM_SAMPLES],
                        const uint8_t in[MI_REC_ADPCM_BLOCK_BYTES]);
#endif
