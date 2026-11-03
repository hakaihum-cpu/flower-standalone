/* SPDX-License-Identifier: GPL-2.0-or-later
 * Audio FX rack for Digitone mk1 OS1.44/core-dn1 4.1.
 * Exactly one insert owner. A visual EUREKA animation is independent.
 */
#ifndef DIGITONE_RACK_H
#define DIGITONE_RACK_H
#include <stdint.h>
#include "digitone_fx_pages.h"
extern struct dnfx_state rack_state[DNFX_COUNT];
extern volatile int32_t rack_active; /* -1 OFF, else 0..3 */
void rack_choose(int mode);
void rack_render(int32_t *out,const int32_t *in);
void rack_granular_render(int32_t *out,const int32_t *in);
void rack_granular_reset(void);
void rack_granular_set(unsigned knob,uint8_t value);
int rack_granular_cc(int track,int cc,int value,int flags);
void rack_audio_reset(int mode);
void rack_audio_render(int32_t *out,const int32_t *in,int mode);
void rack_show_page(void);
void rack_hide_page(void);
int rack_key(void *brain,void *ev);
int rack_enc(void *brain,void *ev);
int rack_cc(int track,int cc,int value,int flags);
void rack_tick(void *ctrl);
void rack_draw(void *bmp,void *ctrl);
#endif
