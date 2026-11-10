/* SPDX-License-Identifier: GPL-3.0-only
 * Derived from user-provided approved Wingie2 SampleSeq v0.2.7 SET1.
 * 3 root notes, each 0.8 s @ 12 kHz, unsigned 8-bit mono in flash/XIP.
 */
#ifndef MI_PIANO_SET1_H
#define MI_PIANO_SET1_H
#include <stdint.h>
#define MI_PIANO_SET1_COUNT 3u
#define MI_PIANO_SET1_SAMPLES 9600u
#define MI_PIANO_SET1_RATE 12000u
static const uint8_t mi_piano_set1_roots[3]={51u,58u,65u};
static const uint16_t mi_piano_set1_loop_start[3]={5991u,5867u,6070u};
extern const uint8_t mi_piano_set1_51[MI_PIANO_SET1_SAMPLES];
extern const uint8_t mi_piano_set1_58[MI_PIANO_SET1_SAMPLES];
extern const uint8_t mi_piano_set1_65[MI_PIANO_SET1_SAMPLES];
float mi_piano_set1_tick(unsigned zone,float *position,float step);
static const uint8_t *mi_piano_set1_data(unsigned index)
{
    return index==0u?mi_piano_set1_51:
           index==1u?mi_piano_set1_58:mi_piano_set1_65;
}
#endif
