/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO native FM-1 audio prototype, no third-party application runtime.
 * NOT a fidelity-complete port of original MIYAKO DSP.
 */
#ifndef MI_NATIVE_AUDIO_H
#define MI_NATIVE_AUDIO_H
#include <stdint.h>
#include "mi_control.h"
#include "mi_recorder.h"
#define MI_NATIVE_POLY 8
#define MI_NATIVE_RING 4096
#define MI_OSC_SCOPE_SAMPLES 1024u /* 1KiB oscilloscope audio ring */
typedef struct {
    uint8_t note, active, held, vel, env_stage;
    float phase, phase2, subphase, increment, env, release_step;
    uint32_t age;
    uint8_t drum_type,drum_variant;
    float drum_decay_rate,drum_pitch_env;
} mi_voice_t;
typedef struct {
    mi_voice_t v[MI_NATIVE_POLY];
    int16_t ring[MI_NATIVE_RING];
    uint32_t wr, valid, phase, segment, rand_state;
    uint32_t age_counter;
    uint8_t last_mode,last_mode2;
    /* Held keys are shared between physical keys and incoming USB/TRS MIDI.
     * Arp runs on the audio sample clock, not the UI frame clock. */
    uint32_t held_keys[4];
    uint8_t held_velocity[128];
    uint8_t held_refs[128]; /* USB and local key overlaps cannot cancel early */
    uint8_t arp_mode, arp_dir, arp_note, arp_sounding, held_total;
    uint16_t arp_pos;
    uint32_t arp_clock, arp_gate_clock, arp_random;
    float lfo_phase;
    float filter_a,filter_b;
    volatile uint8_t video_effective[4];
    uint32_t rendered_samples; /* MIDI-clock timestamp in output samples */
    mi_recorder_t recorder;
    volatile uint8_t rec_command;
    /* Audio-sample-accurate FMDrum step sequencer (independent of OSC page). */
    uint32_t step_wait, step_random;
    uint8_t step_active, step_next, step_last_dir, step_last_len;
    volatile uint8_t step_current; /* UI playhead; 0..15 */
    float sample_rate, wow, flutter, wet_current,wet_current2;
    float rec_monitor_mix; /* existing wet-smoothing approach for REC monitor */
    /* Audio writer/UI reader: actual pre-FX voice output, never UI sine. */
    int8_t osc_scope[MI_OSC_SCOPE_SAMPLES];
    volatile uint32_t osc_scope_head;
    volatile uint8_t osc_scope_active;
    uint8_t osc_scope_selected, osc_scope_stride;
} mi_audio_t;
void mi_audio_init(mi_audio_t *a, float sample_rate);
void mi_audio_note_on(mi_audio_t *a, uint8_t note, uint8_t velocity);
void mi_audio_note_off(mi_audio_t *a, uint8_t note);
/* These input wrappers route live keys to either polyphonic keyboard or ARP. */
void mi_audio_input_on(mi_audio_t *a, const mi_state_t *s,
                       uint8_t note, uint8_t velocity);
void mi_audio_input_off(mi_audio_t *a, const mi_state_t *s, uint8_t note);
void mi_audio_panic(mi_audio_t *a);
/* Command is consumed at the next audio block, no audio memory writes on UI. */
void mi_audio_rec_request(mi_audio_t *a, unsigned command);
/* USB/TRS real-time MIDI 0xF8/0xFA/0xFB/0xFC on the audio thread. */
void mi_audio_midi_clock(mi_audio_t *a,unsigned status);
void mi_audio_render(mi_audio_t *a, const mi_state_t *s,
                     int32_t *interleaved_lr, uint32_t frames, uint32_t master_q12);
#endif
