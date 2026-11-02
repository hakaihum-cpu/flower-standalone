/* SPDX-License-Identifier: GPL-2.0-or-later
 * EUREKA 576-frame Digitone mk1 / Keys OS 1.44 display-only mod.
 * Supplied EUREKA user video imagery only; stock audio unchanged.
 */
#include <stdint.h>
#include "digitone-mk1/core3.h"
#define E576_COUNT 576u
#define E576_FRAME_BYTES 1024u
#define E576_KEY_MIDI 2
#define E576_KEY_YES 13
extern const uint8_t eureka576_frames[];
extern const uint8_t eureka576_frames_end[];
static uint8_t eureka576_framebuf[E576_FRAME_BYTES];
static uint32_t eureka576_readpos, eureka576_index, eureka576_phase;
static uint32_t eureka576_midi_held, eureka576_enabled, eureka576_dirty;

static void eureka576_clear(void)
{
    uint32_t i;
    for(i=0u;i<E576_FRAME_BYTES;++i) eureka576_framebuf[i]=0u;
}
/* Decode one losslessly delta-compressed frame with byte-bound checks.
 * Record: two-byte little-endian length, mode 0 raw or 1 XOR delta/RLE.
 */
static int eureka576_next(void)
{
    const uint8_t *s=eureka576_frames;
    uint32_t size=(uint32_t)((uintptr_t)eureka576_frames_end-(uintptr_t)eureka576_frames);
    uint32_t p, length, end, out, i;
    if(eureka576_readpos>size || size-eureka576_readpos<3u) return 0;
    p=eureka576_readpos;
    length=(uint32_t)s[p] | ((uint32_t)s[p+1u]<<8);
    p+=2u;
    if(length<2u || length>1025u || length>size-p) return 0;
    end=p+length;
    if(s[p]==0u) {
        if(length!=1025u) return 0;
        ++p;
        for(i=0u;i<E576_FRAME_BYTES;++i) eureka576_framebuf[i]=s[p+i];
        p+=E576_FRAME_BYTES;
    } else if(s[p]==1u) {
        ++p;
        out=0u;
        while(p<end) {
            uint32_t tag=s[p++];
            uint32_t count=(tag&127u)+1u;
            if(out+count>E576_FRAME_BYTES) return 0;
            if(tag&128u) {
                if(count>end-p) return 0;
                for(i=0u;i<count;++i) eureka576_framebuf[out+i]^=s[p+i];
                p+=count;
            }
            out+=count;
        }
        if(out!=E576_FRAME_BYTES) return 0;
    } else return 0;
    if(p!=end) return 0;
    eureka576_readpos=end;
    return 1;
}
void eureka576_tick(void *ctrl)
{
    if(eureka576_enabled) {
        /* Core runs at 30Hz; 9/60 gives exactly 4.5 source frames/sec. */
        eureka576_phase+=9u;
        if(eureka576_phase>=60u) {
            eureka576_phase-=60u;
            if(++eureka576_index==E576_COUNT) {
                eureka576_index=0u;
                eureka576_readpos=0u;
                eureka576_clear();
            }
            if(!eureka576_next()) eureka576_enabled=0u;
            eureka576_dirty=1u;
        }
    }
    if(eureka576_dirty) {
        *((volatile uint8_t *)ctrl+0x20)=1u;
        eureka576_dirty=0u;
    }
}
/* Draw black frame, then horizontal white runs on the portable Bitmap.
 * y=0 is bottom in the firmware; y=0 is top in the source image.
 */
void eureka576_draw(void *bmp, void *ctrl)
{
    uint32_t x,y,start;
    (void)ctrl;
    if(!eureka576_enabled) return;
    fw_fillrect(bmp,0,0,127,63,0);
    for(y=0u;y<64u;++y) {
        uint32_t page=7u-(y>>3);
        uint8_t bit=(uint8_t)(1u<<(y&7u));
        x=0u;
        while(x<128u) {
            while(x<128u && !(eureka576_framebuf[page+8u*x]&bit)) ++x;
            if(x==128u) break;
            start=x;
            while(x<128u && (eureka576_framebuf[page+8u*x]&bit)) ++x;
            fw_fillrect(bmp,(int32_t)start,(int32_t)(63u-y),
                        (int32_t)(x-1u),(int32_t)(63u-y),1);
        }
    }
}
int eureka576_key(void *brain,void *ev)
{
    int key=*(int *)((uint8_t *)ev+12);
    int flags=*(int *)((uint8_t *)ev+16);
    int first=(flags&1) && !(flags&8);
    (void)brain;
    if(key==E576_KEY_MIDI) {
        if(first) eureka576_midi_held=1u;
        if(flags&16) eureka576_midi_held=0u;
    }
    if(key==E576_KEY_YES && first && eureka576_midi_held) {
        eureka576_enabled^=1u;
        eureka576_phase=0u;
        if(eureka576_enabled) {
            eureka576_index=0u;
            eureka576_readpos=0u;
            eureka576_clear();
            if(!eureka576_next()) eureka576_enabled=0u;
        }
        eureka576_dirty=1u;
        return 1;
    }
    return 0;
}
