/* SPDX-License-Identifier: GPL-3.0-only
 * Original MIYAKO EFFECTS and EUREKA animation state.
 */
#ifndef MI_NATIVE_VIDEO_H
#define MI_NATIVE_VIDEO_H
#include <stdint.h>
typedef struct {
    uint32_t effects_rng, eureka_rng;
    uint16_t eureka_count;
    int16_t eureka_frame;
    uint8_t eureka_cooldown;
    uint16_t effect_count;
    int16_t effects_frame;
    float effects_previous;
    uint8_t effects_cooldown, effects_hold_ticks;
} mi_video_t;
void mi_video_init(mi_video_t *v, uint16_t effect_count);
void mi_video_set_eureka(mi_video_t *v,uint16_t frame_count);
void mi_video_tick(mi_video_t *v, float activity, int eureka_mode);
#endif
