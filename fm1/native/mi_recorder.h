/* SPDX-License-Identifier: GPL-3.0-only
 * FM-1 MIYAKO internal-bus recorder: 2 seconds 22.05kHz mono 16-bit.
 * Input is MIYAKO synth/FX output, not external audio or USB playback.
 */
#ifndef MI_NATIVE_RECORDER_H
#define MI_NATIVE_RECORDER_H
#include <stdint.h>
#define MI_REC_RATE 22050u
#define MI_REC_CAPACITY 44100u
typedef struct {
    uint32_t count, write_pos;
    float playback_pos, accumulator;
    uint8_t capture_phase, recording, playing;
} mi_recorder_t;
void mi_rec_init(mi_recorder_t *r);
void mi_rec_toggle_record(mi_recorder_t *r);
void mi_rec_toggle_play(mi_recorder_t *r);
float mi_rec_tick(mi_recorder_t *r,float internal_audio,const uint8_t controls[4]);
void mi_rec_stop(mi_recorder_t *r);
int16_t mi_rec_sample(uint32_t sample);
#endif
