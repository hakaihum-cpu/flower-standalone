/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO FM-1 native FX: no KARMA application, audio or UI code.
 * Early native processors must not be mistaken for source-faithful JUCE FX.
 */
#ifndef MI_NATIVE_FX_DSP_H
#define MI_NATIVE_FX_DSP_H
#include <stdint.h>
typedef struct {
    uint32_t delay_write,delay_valid;
    uint32_t eureka_write,eureka_valid;
    uint8_t eureka_decimate,eureka_heard;
    uint32_t eureka_content_valid,eureka_quiet_samples;
    float eureka_sum,eureka_phase,eureka_filter;
    float eureka_pitch_cache;
    uint8_t eureka_last_pitch;
    uint32_t dreamy_write, dreamy_valid;
    float dreamy_phase;
    float dreamy_pitch_cache;
    uint8_t dreamy_last_pitch;
    uint32_t chorus_write;
    uint32_t reverb_write[4];
    float chorus_phase;
    float reverb_damp[4];
} mi_fx_dsp_t;
void mi_fx_dsp_init(mi_fx_dsp_t *p);
/* DELAY and EUREKA share one 64-KiB history buffer. */
void mi_fx_dsp_reset_shared(mi_fx_dsp_t *p);
/* EUREKA has a wet-only output. The mixer must keep DRY audible until
 * history containing an actual note (not pre-roll silence) fills TIME+HAZE. */
int mi_fx_dsp_eureka_ready(const mi_fx_dsp_t *p, unsigned time, unsigned haze);
/* Extended DSP controls are real: pitch, reverse, density and feedback.
 * Original two FX parameters and legacy call signature are retained. */
float mi_fx_dsp_process_ex(mi_fx_dsp_t *p,unsigned mode,float dry,
                           float sample_rate,unsigned p1,unsigned p2,
                           const uint8_t extra[4]);
float mi_fx_dsp_process(mi_fx_dsp_t *p, unsigned mode, float dry,
                        float sample_rate, unsigned p1, unsigned p2);
#endif
