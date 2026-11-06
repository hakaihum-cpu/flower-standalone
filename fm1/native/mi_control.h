/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO FM-1 standalone application control state.
 * No external synth/firmware implementation dependency.
 * This module does NOT generate audio or write flash.
 */
#ifndef MIYAKO_NATIVE_CONTROL_H
#define MIYAKO_NATIVE_CONTROL_H
#include <stdint.h>

typedef enum {
    MI_HOME, MI_OSC, MI_FX, MI_VIDEO, MI_ENV, MI_LFO,
    MI_ARP, MI_SEQ, MI_CONFIG, MI_SAVE, MI_VIEW_COUNT
} mi_view_t;
typedef enum {
    MI_FX_OFF, MI_FX_REEL, MI_FX_DREAMY, MI_FX_DELAY,
    MI_FX_REVERB, MI_FX_CHORUS, MI_FX_EUREKA, MI_FX_COUNT
} mi_fx_t;
typedef enum {
    MI_B_HOME, MI_B_EDIT, MI_B_FX, MI_B_SEL, MI_B_ENV,
    MI_B_LFO, MI_B_ARP, MI_B_SEQ, MI_B_GLO, MI_B_SAVE,
    MI_B_PLAY, MI_B_REC, MI_B_OCT_DOWN, MI_B_OCT_UP
} mi_button_t;
typedef enum {
    MI_EN_ALGORITHM, MI_EN_PRESETS, MI_EN_SELECT,
    MI_EN_K1, MI_EN_K2, MI_EN_K3, MI_EN_K4
} mi_encoder_t;
typedef enum {
    MI_ACTION_NONE, MI_ACTION_SAVE, MI_ACTION_PLAY, MI_ACTION_RECORD
} mi_action_t;
typedef struct {
    mi_view_t view;
    uint8_t wave, width, sub, crush, fx, wet, level, preset;
    uint8_t fx_options[MI_FX_COUNT][2];
    uint8_t env[4], lfo[4], arp[4], seq[4];
    int16_t global[4]; /* BPM 40..240, SWING 0..100, TUNE -50..50, CLOCK 0..2 */
    uint8_t project_slot; /* 1..4; persistent storage provided by the FM-1 HAL */
    int8_t octave;       /* -3..+3 */
    uint8_t playing, recording, save_armed;
    uint32_t save_armed_ms;
} mi_state_t;
typedef struct {
    mi_action_t action;
    uint8_t slot; /* meaningful only for MI_ACTION_SAVE */
} mi_result_t;

void mi_init(mi_state_t *s);
mi_result_t mi_button(mi_state_t *s, mi_button_t b, uint32_t now_ms);
mi_result_t mi_rotate(mi_state_t *s, mi_encoder_t encoder, int32_t steps);
const char *mi_wave_name(unsigned wave);
const char *mi_arp_mode_name(unsigned mode);
const char *mi_lfo_wave_name(unsigned wave);
const char *mi_view_name(mi_view_t view);
const char *mi_fx_name(mi_fx_t effect);
const char *mi_param_name(const mi_state_t *s, unsigned knob);
int32_t mi_param_value(const mi_state_t *s, unsigned knob);
#endif
