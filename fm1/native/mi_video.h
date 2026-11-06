/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO's original visualizer timing logic, independent of asset storage.
 * VIDEO does not change the audio effect; images must come from user originals.
 */
#ifndef MI_NATIVE_VIDEO_H
#define MI_NATIVE_VIDEO_H
#include <stdint.h>
typedef struct {
    uint32_t effects_rng, eureka_rng;
    uint16_t effect_count;
    int16_t effects_frame, eureka_frame, eureka_clip;
    float eureka_playhead, effects_previous, eureka_previous;
    uint8_t effects_cooldown, effects_hold_ticks;
} mi_video_t;
void mi_video_init(mi_video_t *v, uint16_t effect_count);
void mi_video_tick(mi_video_t *v, float activity, int eureka_enabled);
#endif
