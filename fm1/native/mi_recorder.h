/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO FM-1 two/four-bar audio looper: 8kHz G.711 u-law, one channel.
 * One bounded SRAM buffer. Clock mode is exactly 192 MIDI F8 ticks (4/4).
 */
#ifndef MI_NATIVE_RECORDER_H
#define MI_NATIVE_RECORDER_H
#include <stdint.h>
#define MI_REC_RATE 8000u
/* 4 bars at 40 BPM: 24 s * 8000 samples/s = 192000 bytes. */
#define MI_REC_CAPACITY 192000u
#define MI_REC_BAR_TICKS 96u
#define MI_REC_CLOCK_TICKS 192u
typedef struct {
    uint32_t count,write_pos,limit_count;
    float playback_pos,accumulator,accumulated,source_rate;
    uint32_t samples_accumulated;
    uint8_t capture_phase,recording,playing,overdubbing;
    unsigned bpm;
    uint32_t clock_phase,clock_ticks,clock_age;
    uint8_t clock_present,clock_origin,clock_transport;
    uint8_t clock_wait,clock_mode,clock_pad;
} mi_recorder_t;
void mi_rec_init(mi_recorder_t *r);
void mi_rec_config(mi_recorder_t *r,float sample_rate,unsigned bpm);
void mi_rec_midi_realtime(mi_recorder_t *r,uint8_t status);
void mi_rec_toggle_record(mi_recorder_t *r);
void mi_rec_toggle_play(mi_recorder_t *r);
float mi_rec_tick(mi_recorder_t *r,float internal_audio,const uint8_t controls[4]);
void mi_rec_stop(mi_recorder_t *r);
void mi_rec_clear(mi_recorder_t *r);
int16_t mi_rec_sample(uint32_t sample);
void mi_rec_peak(unsigned column,int8_t *lo,int8_t *hi);
#endif
