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
    uint8_t eureka_decimate;
    float eureka_sum,eureka_phase,eureka_filter;
    uint32_t dreamy_write, dreamy_valid;
    float dreamy_phase;
    uint32_t chorus_write;
    uint32_t reverb_write[4];
    float chorus_phase;
    float reverb_damp[4];
} mi_fx_dsp_t;
void mi_fx_dsp_init(mi_fx_dsp_t *p);
/* DELAY and EUREKA share one 64-KiB history buffer. */
void mi_fx_dsp_reset_shared(mi_fx_dsp_t *p);
/* EUREKA has a wet-only output and must prefill its TIME+HAZE lookback.
 * Until the history is valid, the FX mixer must retain the dry signal. */
int mi_fx_dsp_eureka_ready(const mi_fx_dsp_t *p, unsigned time, unsigned haze);
float mi_fx_dsp_process(mi_fx_dsp_t *p, unsigned mode, float dry,
                        float sample_rate, unsigned p1, unsigned p2);
#endif
