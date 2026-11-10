/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO stereo micro-slicer: a post-mix, bounded random stutter.
 * A single 16-KiB history is shared by both stereo channels.
 */
#ifndef MI_NATIVE_SLICER_H
#define MI_NATIVE_SLICER_H
#include <stdint.h>
#define MI_SLICER_FRAMES 4096u
typedef struct {
    uint32_t write,valid,phase,span,source,rng;
    uint8_t active,holding,repeats;
} mi_slicer_t;
void mi_slicer_init(mi_slicer_t *s);
/* tempo_step: samples per 1/16 note (internal BPM or measured MIDI clock).
 * speed 0..127 maps to successively faster micro-slices; rand 0..127
 * controls the likelihood of re-triggering a recorded stereo slice.
 * The function always writes the unprocessed input into history.
 */
void mi_slicer_tick(mi_slicer_t *s,float in_l,float in_r,
                    float tempo_step,unsigned speed,unsigned randomness,
                    unsigned active,float *wet_l,float *wet_r);
#endif
