/* SPDX-License-Identifier: GPL-3.0-only
 * Bounded 7-bit log-PCM blocks for three independent FM-1 loop tracks.
 * 64 signed PCM16 samples -> 56 bytes; random access, no attack smearing.
 */
#ifndef MI_REC_PCM7_H
#define MI_REC_PCM7_H
#include <stdint.h>
#define MI_REC_PCM7_SAMPLES 64u
#define MI_REC_PCM7_BLOCK_BYTES 56u
#define MI_REC_PCM7_BLOCKS 690u /* ceil(44100 / 64) */
#define MI_REC_PCM7_TRACKS 3u
#define MI_REC_PCM7_STORE_BYTES (MI_REC_PCM7_TRACKS*MI_REC_PCM7_BLOCKS*MI_REC_PCM7_BLOCK_BYTES)
void mi_pcm7_encode_64(uint8_t out[MI_REC_PCM7_BLOCK_BYTES],
                       const int16_t pcm[MI_REC_PCM7_SAMPLES]);
void mi_pcm7_decode_64(int16_t pcm[MI_REC_PCM7_SAMPLES],
                       const uint8_t in[MI_REC_PCM7_BLOCK_BYTES]);
#endif
