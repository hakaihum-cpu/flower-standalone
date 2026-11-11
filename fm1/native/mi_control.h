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
    MI_FX_REVERB, MI_FX_CHORUS, MI_FX_EUREKA,
    MI_FX_BITCRUSH, MI_FX_PITCHSHIFT, MI_FX_SLICER, MI_FX_COUNT
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
    MI_ACTION_NONE, MI_ACTION_SAVE, MI_ACTION_PLAY, MI_ACTION_RECORD,
    MI_ACTION_LOAD, MI_ACTION_DEFAULT
} mi_action_t;
/* Preserve all approved MI17/MI18 FX parameter offsets when appending SLICER.
 * SLICER's two controls live at the tail of mi_state_t. */
#define MI_FX_STORAGE_COUNT 9u
typedef struct {
    mi_view_t view;
    uint8_t wave, width, sub, crush, fx, wet, level, preset;
    /* wave 0..3 = single, 4 = MULTI. Oscillator1's actual waveform is
     * retained separately while the HOME WAVE selector stays on MULTI. */
    uint8_t fx_options[MI_FX_STORAGE_COUNT][2];
    uint8_t env[4], lfo[4], arp[4], seq[4];
    int16_t global[4]; /* BPM 40..240, SWING 0..100, TUNE -50..50, CLOCK 0..2 */
    uint8_t fx2, wet2, fx_slot; /* independent FX B, toggle slot via FX button */
    uint8_t project_slot; /* 1..4; persistent storage provided by the FM-1 HAL */
    int8_t octave;       /* -3..+3 */
    uint8_t playing, recording, save_armed;
    uint32_t save_armed_ms;
    /* Added at the END to preserve the complete original MI13 state prefix.
     * OSC1 parameters/sub/crush are unchanged. OSC2 has independent waveform,
     * octave, fine detune and level. Page only affects editing, never audio. */
    uint8_t osc2_wave, osc2_level, osc2_detune, osc2_octave, osc_page;
    /* Four additional genuinely routed DSP controls for DREAMY/EUREKA:
     * pitch, reverse, density, feedback. Original 2 controls remain. */
    uint8_t fx_extra[MI_FX_STORAGE_COUNT][4];
    uint8_t fx_page;
    uint8_t osc1_wave; /* appended, previous saved-state prefix unchanged */
    /* MI15 additions are migrated field-wise from historic layouts. */
    uint8_t filter_cutoff,filter_resonance;
    uint8_t drum_tone,drum_decay,drum_color,drum_level;
    uint8_t rec_bars; /* 2(default) or 4; external MIDI clock forces 2 */
    uint8_t video_params[4]; /* glitch, tile, xshift, yshift */
    /* MI16: FMDrum 4-track x 16-step pattern. Append only: all MI15
     * persisted fields retain their exact byte offsets. */
    uint16_t step_pattern[4]; /* bit 0 = first white key, per track */
    uint8_t step_track;       /* selected track 0..3 */
    uint8_t step_play;        /* K2: clockwise starts, CCW stops */
    uint8_t step_dir;         /* K3: 0=FWD, 1=REV, 2=RND */
    uint8_t step_len;         /* K4: 8..16; default 8 */
    uint8_t step_editor;      /* SEQ long hold UI, transient */
    /* MI17: appended configuration and explicit save/load controls.
     * Historic MI16 flash data is migrated by its exact aligned prefix. */
    uint8_t midi_channel;     /* 1..16: incoming note channel */
    uint8_t save_action;      /* SAVE screen K2: 0=SAVE, 1=LOAD, 2=DEFAULT */
    uint8_t config_load_armed;/* GLO K4 double-detent confirmation */
    /* MI18 REC: track 1 retains seq[] for historic preset/CC21 migration. */
    uint8_t rec_track;   /* 0..2, PRESET turns while on REC */
    uint8_t rec_page;    /* 0=IN/OUT/PITCH/LEVEL, 1=MUTE/PAN/TRACK/BARS */
    uint8_t rec_extra[2][4]; /* REC 2/3 independent controls */
    uint8_t rec_mute[3],rec_pan[3];
    /* MI19: independent SLICER rate and randomness, added at the tail. */
    uint8_t fx_slicer[2];
    /* MI20: per-track reverse and quantized PLAY/STOP. Append-only:
     * older MI19 flash slots migrate their exact historical prefix. */
    uint8_t rec_reverse[3]; /* 0 forward, 1 reverse */
    uint8_t rec_quant[3];   /* 0 immediate, 1 beat, 2 bar */
} mi_state_t;
typedef struct {
    mi_action_t action;
    uint8_t slot; /* meaningful only for MI_ACTION_SAVE */
} mi_result_t;

void mi_init(mi_state_t *s);
mi_result_t mi_button(mi_state_t *s, mi_button_t b, uint32_t now_ms);
mi_result_t mi_rotate(mi_state_t *s, mi_encoder_t encoder, int32_t steps);
const char *mi_wave_name(unsigned wave);
#define MI_WAVE_MULTI 4u
#define MI_WAVE_FMDRUM 5u
#define MI_WAVE_PIANO 6u /* Wingie2 SET1 original source, three flash zones */
/* MIDI CC#21 on any channel controls REC PITCH's existing seven values. */
#define MI_REC_PITCH_CC 21u
/* New, non-overlapping REC control bank: CC22..24 level, 25..27 pan,
 * 28..30 mute. Existing CC21 stays the selected-track pitch. */
#define MI_REC_LEVEL_CC_BASE 22u
#define MI_REC_PAN_CC_BASE 25u
#define MI_REC_MUTE_CC_BASE 28u
int mi_rec_cc(mi_state_t *s,unsigned cc,unsigned value);
#define MI_LFO_DEST_COUNT 58u
const char *mi_arp_mode_name(unsigned mode);
const char *mi_lfo_wave_name(unsigned wave);
const char *mi_lfo_dest_name(unsigned dest);
const char *mi_view_name(mi_view_t view);
const char *mi_fx_name(mi_fx_t effect);
/* Two serial FX, but no duplicate processor or shared DELAY/EUREKA ring. */
int mi_fx_pair_supported(unsigned first,unsigned second);
/* Complete saved-settings bounds validation, shared with host C regression. */
int mi_state_valid(const mi_state_t *s);
const char *mi_param_name(const mi_state_t *s, unsigned knob);
int32_t mi_param_value(const mi_state_t *s, unsigned knob);
#endif
