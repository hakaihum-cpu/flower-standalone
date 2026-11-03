/* SPDX-License-Identifier: GPL-2.0-or-later
 * EUREKA 576-frame Digitone mk1 / Keys OS 1.44 display-only mod.
 * EUREKA user frames preserve their original composition in black/yellow RG DOT.
 * Stock audio untouched.
 */
#include <stdint.h>
#include "digitone-mk1/core3.h"
#define E576_COUNT 576u
#define E576_FRAME_BYTES 1024u
#define E576_KEY_MIDI 2
#define E576_KEY_YES 13
extern const uint8_t eureka576_frames[];
extern const uint8_t eureka576_frames_end[];
extern const uint8_t eureka576_seek_table[];
extern const uint8_t eureka576_seek_table_end[];
static uint8_t eureka576_framebuf[E576_FRAME_BYTES];
static uint32_t eureka576_readpos, eureka576_index, eureka576_phase;
static uint32_t eureka576_midi_held, eureka576_enabled;
static volatile uint32_t eureka576_dirty;
/* Video-only random sequence state; no changes to audio or sound parameters. */
#define E576_SEEK_ENTRY 1028u
#define E576_SEEK_EVERY 6u
#define E576_SEEK_COUNT 96u
#define E576_STEP_TIME 5400000u
#define E576_QUEUE_SIZE 32u
static uint32_t eureka576_rng=0x534a5531u;
static uint32_t eureka576_effect, eureka576_speed, eureka576_until, eureka576_reverse;
static uint32_t eureka576_stutter_start, eureka576_stutter_span, eureka576_stutter_step;
static uint32_t eureka576_was_playing;
/* Extra AUTO-channel MIDI visual effects. All OFF at boot.
 * None changes the underlying EUREKA frames, stock keys or audio.
 * CC37: horizontal VHS-style tearing (0 off).
 * CC38: source-pixel tile inversion/rearrangement (0 off).
 * CC40: 4/2/1 frame repeat (0 off, note-reactive).
 * MIDI-task writers, UI-task readers: aligned volatile scalars only.
 */
static volatile uint8_t eureka576_cc_tear, eureka576_cc_tile, eureka576_cc_repeat;
static uint32_t eureka576_repeat_anchor, eureka576_repeat_cursor;
static volatile uint32_t eureka576_event_times[E576_QUEUE_SIZE];
static volatile uint32_t eureka576_event_head, eureka576_event_tail;

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
/* Checkpoints are 96 snapshots at frame 0,6,...570.
 * Restore any of 576 frames by decoding no more than five XOR deltas. */
static int eureka576_seek(uint32_t frame)
{
    const uint8_t *s=eureka576_seek_table;
    uint32_t size=(uint32_t)((uintptr_t)eureka576_seek_table_end-(uintptr_t)eureka576_seek_table);
    uint32_t total=(uint32_t)((uintptr_t)eureka576_frames_end-(uintptr_t)eureka576_frames);
    uint32_t slot, offset, i;
    if(frame>=E576_COUNT || size!=E576_SEEK_ENTRY*E576_SEEK_COUNT) return 0;
    slot=(frame/E576_SEEK_EVERY)*E576_SEEK_ENTRY;
    offset=(uint32_t)s[slot] | ((uint32_t)s[slot+1u]<<8) |
           ((uint32_t)s[slot+2u]<<16) | ((uint32_t)s[slot+3u]<<24);
    if(offset>total) return 0;
    for(i=0u;i<E576_FRAME_BYTES;++i) eureka576_framebuf[i]=s[slot+4u+i];
    eureka576_readpos=offset;
    for(i=0u;i<frame%E576_SEEK_EVERY;++i) {
        if(!eureka576_next()) return 0;
    }
    eureka576_index=frame;
    eureka576_dirty=1u;
    return 1;
}
static uint32_t eureka576_random(void)
{
    uint32_t x=eureka576_rng;
    x^=x<<13; x^=x>>17; x^=x<<5;
    eureka576_rng=x;
    return x;
}
/* A real FM voice has started (keyboard, MIDI or sequencer).
 * The callback can run at interrupt level: enqueue time, no frame decoding. */
void eureka576_voice_on(int32_t voice, int32_t track, void *event)
{
    uint32_t tail,next;
    (void)voice; (void)event;
    if(!eureka576_enabled || track<0 || track>3) return;
    tail=eureka576_event_tail;
    next=(tail+1u)&(E576_QUEUE_SIZE-1u);
    if(next==eureka576_event_head) return;
    eureka576_event_times[tail]=fw_timeline;
    eureka576_event_tail=next;
}
/* The EUREKA image CCs are separate from Granular CC33/34/36/60.
 * Only take enabled AUTO-channel CC; pass all other messages to stock.
 */
int eureka576_cc(int32_t track, int32_t cc, int32_t value, int32_t flags)
{
    (void)track;
    if(!eureka576_enabled ||
       (((uint32_t)flags & CORE_MIDI_CC_AUTO) == 0u) ||
       value < 0 || value > 127)
        return 0;
    if(cc == 37) eureka576_cc_tear=(uint8_t)value;
    else if(cc == 38) eureka576_cc_tile=(uint8_t)value;
    else if(cc == 40) {
        uint8_t previous=eureka576_cc_repeat;
        eureka576_cc_repeat=(uint8_t)value;
        if(!previous || !value) {
            eureka576_repeat_anchor=eureka576_index;
            eureka576_repeat_cursor=0u;
        }
    } else return 0;
    eureka576_dirty=1u;
    return 1;
}

/* Every actual note makes a visible change. Randomize the action, not
 * whether the note has any effect: jump / reverse / speed / stutter. */
static void eureka576_trigger(uint32_t time)
{
    uint32_t action, clip, inside, duration, frame;
    action=eureka576_random()%100u;
    eureka576_effect=0u;
    eureka576_speed=0u;
    eureka576_phase=0u; /* keep the audible note's first visual jump visible */
    if(action<35u) {
        clip=((eureka576_index/36u)+1u+eureka576_random()%15u)%16u;
        inside=eureka576_random()%36u;
        if(!eureka576_seek(clip*36u+inside)) eureka576_enabled=0u;
    } else if(action<60u) {
        /* Reverse stays enabled until the next reverse event. */
        eureka576_reverse^=1u;
        frame=(eureka576_index+(eureka576_reverse?E576_COUNT-6u:6u))
              %E576_COUNT;
        if(!eureka576_seek(frame)) eureka576_enabled=0u;
    } else if(action<80u) {
        eureka576_effect=1u;
        eureka576_speed=1u+eureka576_random()%3u;
        duration=4u+eureka576_random()%5u;
        eureka576_until=time+duration*E576_STEP_TIME;
        frame=(eureka576_index+(eureka576_reverse?E576_COUNT-7u:7u))
              %E576_COUNT;
        if(!eureka576_seek(frame)) eureka576_enabled=0u;
    } else {
        eureka576_effect=2u;
        inside=eureka576_index%36u;
        eureka576_stutter_start=eureka576_index-inside+
                                (inside>32u?32u:inside);
        eureka576_stutter_span=2u+eureka576_random()%2u;
        eureka576_stutter_step=0u;
        duration=2u+eureka576_random()%3u;
        eureka576_until=time+duration*E576_STEP_TIME;
        frame=eureka576_stutter_start+
              (eureka576_index==eureka576_stutter_start?1u:0u);
        if(!eureka576_seek(frame)) eureka576_enabled=0u;
    }
    /* Keep live note-triggered frame jumps visible even in repeated video. */
    eureka576_repeat_anchor=eureka576_index;
    eureka576_repeat_cursor=0u;
    eureka576_dirty=1u;
}
static int eureka576_step(void)
{
    uint32_t frame;
    /* Short source-frame loop; no extra image bank or audio callback load. */
    if(eureka576_cc_repeat) {
        uint32_t v=(uint32_t)eureka576_cc_repeat;
        uint32_t span = v < 64u ? 4u : (v < 96u ? 2u : 1u);
        if(span==1u) return 1; /* frozen frame; note jumps still work */
        frame=(eureka576_repeat_anchor+
               (eureka576_repeat_cursor++ % span))%E576_COUNT;
        return eureka576_seek(frame);
    }
    if(eureka576_effect==2u) {
        frame=eureka576_stutter_start+
              (eureka576_stutter_step++%eureka576_stutter_span);
        return eureka576_seek(frame);
    }
    if(eureka576_reverse)
        return eureka576_seek((eureka576_index+E576_COUNT-1u)%E576_COUNT);
    if(++eureka576_index==E576_COUNT) return eureka576_seek(0u);
    return eureka576_next();
}
void eureka576_tick(void *ctrl)
{
    uint32_t playing=(fw_transport==1u), now=fw_timeline;
    if(!playing && eureka576_was_playing) {
        eureka576_event_head=eureka576_event_tail;
        eureka576_effect=0u;
        eureka576_speed=0u;
    }
    eureka576_was_playing=playing;
    /* Live notes change the displayed image even with transport STOPPED. */
    if(eureka576_enabled) {
        uint32_t note_time=0u, have_note=0u;
        while(eureka576_event_head!=eureka576_event_tail) {
            uint32_t at=eureka576_event_times[eureka576_event_head];
            if((int32_t)(now-at)<0) break;
            eureka576_event_head=(eureka576_event_head+1u)&(E576_QUEUE_SIZE-1u);
            note_time=at;
            have_note=1u;
        }
        /* One visible action per UI frame, even when a chord starts voices. */
        if(have_note) eureka576_trigger(note_time);
    }
    if(eureka576_enabled && playing) {
        uint32_t tempo=(fw_tempo>=3600 && fw_tempo<=36000)?(uint32_t)fw_tempo:14400u;
        if(eureka576_effect && (int32_t)(now-eureka576_until)>=0) {
            eureka576_effect=0u;
            eureka576_speed=0u;
        }
        if(eureka576_cc_repeat || eureka576_effect!=1u || eureka576_speed!=3u) {
            /* 4.5fps at 120BPM, scaled by actual sequencer tempo. */
            uint32_t delta=9u*tempo;
            if(eureka576_effect==1u && eureka576_speed==1u) delta/=2u;
            if(eureka576_effect==1u && eureka576_speed==2u) delta*=2u;
            eureka576_phase+=delta;
            if(eureka576_phase>=864000u) {
                eureka576_phase-=864000u;
                if(!eureka576_step()) eureka576_enabled=0u;
                eureka576_dirty=1u;
            }
        }
    }
    if(eureka576_dirty) {
        *((volatile uint8_t *)ctrl+0x20)=1u;
        eureka576_dirty=0u;
    }
}
/* Render only from the SAME 1024-byte 128x64 packed image.
 * No source content is generated, no extra frames are decoded.
 * Pixel remapping happens on the UI thread, never the audio callback.
 * With CC37=0 and CC38=0 the following renderer is pixel-identical to HG76.
 */
static uint32_t eureka576_display_pixel(uint32_t x,uint32_t y,
                                        uint32_t tear,uint32_t tile,
                                        uint32_t seed)
{
    uint32_t sx=x, sy=y;
    if(tile) {
        uint32_t xbits,ybits;
        uint32_t tx,ty,which;
        /* Each tile width/height is a power of two, so no divisions. */
        if(tile<32u) { xbits=6u; ybits=5u; }       /* 2 x 2 */
        else if(tile<64u) { xbits=5u; ybits=4u; }  /* 4 x 4 */
        else if(tile<96u) { xbits=4u; ybits=4u; }  /* 8 x 4 */
        else { xbits=4u; ybits=3u; }              /* 8 x 8 */
        tx=x>>xbits; ty=y>>ybits;
        which=tx*13u+ty*29u+(seed>>6);
        /* Localized blocks of reverse-source scanning, not new art. */
        if((which&3u)==0u || (which&7u)==3u)
            sx = (x & ~((1u<<xbits)-1u)) +
                 (((1u<<xbits)-1u) - (x & ((1u<<xbits)-1u)));
        if((which&7u)==5u)
            sy = (y & ~((1u<<ybits)-1u)) +
                 (((1u<<ybits)-1u) - (y & ((1u<<ybits)-1u)));
    }
    if(tear) {
        uint32_t band=y>>3u;
        uint32_t v=band*17u + (seed>>11);
        /* Higher CC value shifts more and affects more row bands. */
        uint32_t eligible=(v&3u) < 1u+(tear>>6);
        if(eligible) {
            uint32_t dist=1u+(tear>>3); /* 1-16 source pixels */
            if(v&4u) sx=(sx+dist)&127u;
            else sx=(sx+128u-dist)&127u;
        }
    }
    return ((uint32_t)(eureka576_framebuf[7u-(sy>>3)+8u*sx]>>(sy&7u)))&1u;
}

/* Display scanline, y=0 at top in the frame and bottom in the LCD. */
void eureka576_draw(void *bmp, void *ctrl)
{
    uint32_t x,y,start;
    uint32_t tear=(uint32_t)eureka576_cc_tear;
    uint32_t tile=(uint32_t)eureka576_cc_tile;
    uint32_t seed=eureka576_rng ^ (eureka576_index*2654435761u);
    (void)ctrl;
    if(!eureka576_enabled) return;
    fw_fillrect(bmp,0,0,127,63,0);
    for(y=0u;y<64u;++y) {
        x=0u;
        while(x<128u) {
            while(x<128u &&
                  !eureka576_display_pixel(x,y,tear,tile,seed)) ++x;
            if(x==128u) break;
            start=x;
            while(x<128u &&
                  eureka576_display_pixel(x,y,tear,tile,seed)) ++x;
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
        eureka576_effect=0u;
        eureka576_speed=0u;
        eureka576_reverse=0u;
        eureka576_event_head=eureka576_event_tail;
        if(eureka576_enabled) {
            eureka576_readpos=0u;
            eureka576_clear();
            if(!eureka576_seek(0u)) eureka576_enabled=0u;
        }
        eureka576_dirty=1u;
        return 1;
    }
    return 0;
}
