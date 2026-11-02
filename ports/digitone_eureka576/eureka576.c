/* SPDX-License-Identifier: GPL-2.0-or-later
 * EUREKA 576-frame overlay for Digitone mk1 OS 1.44.
 * All image data is derived solely from user-provided footage.
 * Neither Digitone audio nor effects code is changed.
 */
#include <stdint.h>
#include "digitone-mk1/core3.h"

#define WIDTH 128u
#define HEIGHT 64u
#define FRAME_COUNT 576u
#define FRAME_BYTES 1024u
#define KEY_MIDI 2
#define KEY_YES 13

extern const uint8_t eureka576_frames[];

static volatile unsigned int eureka_frame;
static volatile unsigned int eureka_enabled;
static unsigned int eureka_ticks;
static unsigned int midi_held;

void eureka576_tick(void *ctrl)
{
    if (eureka_enabled) {
        if (++eureka_ticks >= 7u) {
            eureka_ticks = 0u;
            eureka_frame = (eureka_frame + 1u == FRAME_COUNT) ? 0u : eureka_frame + 1u;
            *((volatile uint8_t *)ctrl+0x20) = 1;
        }
    }
}

/* After the stock screen draw, use core's exported Bitmap::fillRect.
 * This avoids raw 1.43-specific display pointers on OS 1.44.
 */
void eureka576_draw(void *bmp, void *ctrl)
{
    unsigned int y, x, run_start;
    const uint8_t *src;
    (void)ctrl;
    if (!eureka_enabled) return;
    src = eureka576_frames + eureka_frame * FRAME_BYTES;
    fw_fillrect(bmp,0,0,127,63,0);
    for (y=0;y<HEIGHT;++y) {
        unsigned int page=7u-(y>>3);
        uint8_t bit=(uint8_t)(1u<<(y&7u));
        x=0u;
        while (x<WIDTH) {
            while (x<WIDTH && !(src[page+8u*x]&bit)) ++x;
            if (x>=WIDTH) break;
            run_start=x;
            while (x<WIDTH && (src[page+8u*x]&bit)) ++x;
            fw_fillrect(bmp,(int32_t)run_start,(int32_t)(63u-y),
                        (int32_t)(x-1u),(int32_t)(63u-y),1);
        }
    }
}

int eureka576_key(void *brain, void *ev)
{
    int key=*(int *)((char *)ev+12);
    int flags=*(int *)((char *)ev+16);
    int first=(flags&1) && !(flags&8);
    (void)brain;
    if (key==KEY_MIDI) {
        if (first) midi_held=1u;
        if (flags&16) midi_held=0u;
    }
    if (key==KEY_YES && first && midi_held) {
        eureka_enabled ^= 1u;
        eureka_ticks=0u;
        if (eureka_enabled) eureka_frame=0u;
        return 1;
    }
    return 0;
}
