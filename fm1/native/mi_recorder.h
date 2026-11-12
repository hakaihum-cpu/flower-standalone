/* SPDX-License-Identifier: GPL-3.0-only
 * Three independent tempo-adaptive 7-bit log-PCM loop tracks.
 * Original REC API and first-track legacy fields are preserved.
 */
#ifndef MI_NATIVE_RECORDER_H
#define MI_NATIVE_RECORDER_H
#include <stdint.h>
#include "mi_rec_pcm7.h"
#define MI_REC_RATE 22050u
#define MI_REC_CAPACITY 44100u
#define MI_REC_TRACKS 3u
/* Same layout for the legacy first-track fields and all independent tracks. */
#define MI_REC_FIELDS \
    uint32_t count, write_pos; \
    float playback_pos, accumulator; \
    uint8_t capture_phase, recording, playing, overdubbing; \
    uint8_t armed,clock_active,bars; \
    uint32_t clock_ticks,clock_start_tick,clock_last_sample; \
    float capture_step,play_step,clock_period; \
    float capture_sum; \
    uint16_t capture_samples;
typedef struct {
    MI_REC_FIELDS
    /* Two 64-sample caches avoid per-sample block companding. */
    int16_t read_pcm[MI_REC_PCM7_SAMPLES],write_pcm[MI_REC_PCM7_SAMPLES];
    uint16_t read_block,write_block;
    uint8_t read_valid,write_valid,write_dirty;
    uint8_t reverse,quant,pending_play; /* 0/1, off/beat/bar, none/start/stop */
    uint8_t tempo_ready,one_bar_loop; /* tempo-based first-bar padding only */
} mi_rec_track_t;
typedef struct {
    /* C anonymous struct exposes the historic r.count/r.playback_pos names,
     * but they alias tracks[0] and require no shadow copies in audio IRQ. */
    union {
        mi_rec_track_t tracks[MI_REC_TRACKS];
        struct { MI_REC_FIELDS };
    };
    uint8_t selected, params[MI_REC_TRACKS-1u][4];
    uint8_t mute[MI_REC_TRACKS],pan[MI_REC_TRACKS];
    uint8_t level[MI_REC_TRACKS]; /* include zero-level tracks in bypass logic */
    float beat_phase,beat_samples; /* internal quantization sample clock */
    uint8_t beat_index; /* modulo four, preserved across LOOP transport */
} mi_recorder_t;
#undef MI_REC_FIELDS
void mi_rec_init(mi_recorder_t *r);
void mi_rec_select(mi_recorder_t *r,unsigned track);
void mi_rec_set_transport(mi_recorder_t *r,unsigned track,
                          unsigned reverse,unsigned quant);
/* Align the internal beat/bar grid to a newly started step sequencer. */
void mi_rec_internal_start(mi_recorder_t *r);
void mi_rec_set_track(mi_recorder_t *r,unsigned track,const uint8_t controls[4],
                      unsigned mute,unsigned pan);
int mi_rec_any_playing(const mi_recorder_t *r);
int mi_rec_any_audible(const mi_recorder_t *r);
int mi_rec_any_busy(const mi_recorder_t *r);
void mi_rec_toggle_record(mi_recorder_t *r);
void mi_rec_toggle_play(mi_recorder_t *r);
float mi_rec_tick(mi_recorder_t *r,float input,const uint8_t controls[4]);
void mi_rec_tick_stereo(mi_recorder_t *r,float input,
                        const uint8_t selected_controls[4],float *left,float *right);
void mi_rec_configure(mi_recorder_t *r,unsigned bpm,unsigned bars,
                      float sample_rate,uint32_t audio_sample_clock);
void mi_rec_external_tick(mi_recorder_t *r,uint32_t audio_sample_clock);
void mi_rec_clock_stop(mi_recorder_t *r);
void mi_rec_stop(mi_recorder_t *r);
void mi_rec_clear(mi_recorder_t *r);
int16_t mi_rec_sample(uint32_t sample);
void mi_rec_peak(unsigned column,int8_t *lo,int8_t *hi);
void mi_rec_peak_track(unsigned track,unsigned column,int8_t *lo,int8_t *hi);
#endif
