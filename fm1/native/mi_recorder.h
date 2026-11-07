/* SPDX-License-Identifier: GPL-3.0-only
 * FM-1 MIYAKO internal-bus recorder: 2 seconds 22.05kHz mono 16-bit.
 * Input is MIYAKO synth/FX output, not external audio or USB playback.
 */
#ifndef MI_NATIVE_RECORDER_H
#define MI_NATIVE_RECORDER_H
#include <stdint.h>
#define MI_REC_RATE 22050u /* historical host-test default; live rate follows BPM */
#define MI_REC_CAPACITY 44100u
typedef struct {
    uint32_t count, write_pos;
    float playback_pos, accumulator;
    uint8_t capture_phase, recording, playing, overdubbing;
    uint8_t armed,clock_active,bars;
    uint32_t clock_ticks,clock_start_tick,clock_last_sample;
    float capture_step,play_step,clock_period;
    float capture_sum;
    uint16_t capture_samples;
} mi_recorder_t;
void mi_rec_init(mi_recorder_t *r);
/* REC on a nonempty clip means OVERDUB: add new audio, never erase. */
void mi_rec_toggle_record(mi_recorder_t *r);
void mi_rec_toggle_play(mi_recorder_t *r);
float mi_rec_tick(mi_recorder_t *r,float internal_audio,const uint8_t controls[4]);
/* Called on audio thread: no extra PCM buffer or heap allocation. */
void mi_rec_configure(mi_recorder_t *r,unsigned bpm,unsigned bars,
                      float sample_rate,uint32_t audio_sample_clock);
void mi_rec_external_tick(mi_recorder_t *r,uint32_t audio_sample_clock);
void mi_rec_clock_stop(mi_recorder_t *r);
void mi_rec_stop(mi_recorder_t *r);
/* Discard the clip without scanning 88 KiB of PCM in the audio callback. */
void mi_rec_clear(mi_recorder_t *r);
int16_t mi_rec_sample(uint32_t sample);
/* Precomputed 224-bin waveform envelope; UI does no full-clip scans. */
void mi_rec_peak(unsigned column,int8_t *lo,int8_t *hi);
#endif
