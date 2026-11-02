/* SPDX-License-Identifier: GPL-2.0-or-later
 * MOOD MkII 343-frame Digitone mk1 / Keys OS 1.44 display-only mod.
 * User-supplied MOOD MkII photographs converted to yellow outlines; stock audio unchanged.
 */
#include <stdint.h>
#include "digitone-mk1/core3.h"
#define E576_COUNT 343u
#define E576_FRAME_BYTES 1024u
#define E576_KEY_MIDI 2
#define E576_KEY_YES 13
extern const uint8_t eureka576_frames[];
extern const uint8_t eureka576_frames_end[];
extern const uint8_t eureka576_seek_table[];
extern const uint8_t eureka576_seek_table_end[];
static uint8_t eureka576_framebuf[E576_FRAME_BYTES];
static uint32_t eureka576_readpos, eureka576_index, eureka576_phase;
static uint32_t eureka576_midi_held, eureka576_enabled, eureka576_dirty;
/* Video-only random sequence state; no changes to audio or sound parameters. */
#define E576_SEEK_ENTRY 1028u
#define E576_SEEK_EVERY 6u
#define E576_SEEK_COUNT 58u
#define E576_STEP_TIME 5400000u
#define E576_QUEUE_SIZE 32u
static uint32_t eureka576_rng=0x534a5531u;
static uint32_t eureka576_effect, eureka576_speed, eureka576_until;
static uint32_t eureka576_stutter_start, eureka576_stutter_span, eureka576_stutter_step;
static uint32_t eureka576_last_change, eureka576_have_change, eureka576_was_playing;
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
/* Checkpoints are 58 snapshots at frame 0,6,...342.
 * Restore any of 343 frames by decoding no more than five XOR deltas. */
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
/* Core 4.1.1 calls this before a note plays. It may run with IRQs masked.
 * Queue only its scheduled time: no decoding, drawing or firmware calls. */
void eureka576_note_queue(void *event, uint32_t time)
{
    uint32_t tail, next, kind, step;
    int32_t track;
    if(!eureka576_enabled || !event) return;
    kind=FW_EVENT_KIND(event);
    if(kind!=FW_EVENT_FIRE && kind!=FW_EVENT_FIRST) return;
    track=FW_EVENT_TRACK(event);
    if(track<0 || track>3) return;
    step=FW_EVENT_STEP(event);
    if(!(step&FW_EVENT_NOTE_TRIG)) return;
    tail=eureka576_event_tail;
    next=(tail+1u)&(E576_QUEUE_SIZE-1u);
    if(next==eureka576_event_head) return;
    eureka576_event_times[tail]=time;
    eureka576_event_tail=next;
}
static void eureka576_trigger(uint32_t time)
{
    uint32_t action, clip, inside, duration, first, last;
    if(eureka576_have_change &&
       (uint32_t)(time-eureka576_last_change)<2u*E576_STEP_TIME) return;
    if(eureka576_random()%100u>=30u) return;
    eureka576_have_change=1u;
    eureka576_last_change=time;
    action=eureka576_random()%100u;
    eureka576_effect=0u;
    eureka576_speed=0u;
    if(action<45u) {
        /* 45%: jump to a different one of 16 consecutive MOOD frame banks.
         * The 343 originals are partitioned without repeating/dropping any. */
        clip=((eureka576_index*16u/E576_COUNT)+1u+eureka576_random()%15u)%16u;
        first=clip*E576_COUNT/16u;
        last=(clip+1u)*E576_COUNT/16u;
        inside=eureka576_random()%(last-first);
        if(!eureka576_seek(first+inside)) eureka576_enabled=0u;
    } else if(action<80u) {
        /* 35%: half-speed, double-speed, or hold for 4-8 steps. */
        eureka576_effect=1u;
        eureka576_speed=1u+eureka576_random()%3u;
        duration=4u+eureka576_random()%5u;
        eureka576_until=time+duration*E576_STEP_TIME;
    } else {
        /* 20%: 2-3 adjacent frames, bounded inside their MOOD bank. */
        eureka576_effect=2u;
        clip=eureka576_index*16u/E576_COUNT;
        first=clip*E576_COUNT/16u;
        last=(clip+1u)*E576_COUNT/16u;
        eureka576_stutter_span=2u+eureka576_random()%2u;
        eureka576_stutter_start=eureka576_index;
        if(eureka576_stutter_start+eureka576_stutter_span>last)
            eureka576_stutter_start=last-eureka576_stutter_span;
        eureka576_stutter_step=0u;
        duration=2u+eureka576_random()%3u;
        eureka576_until=time+duration*E576_STEP_TIME;
    }
    eureka576_dirty=1u;
}
static int eureka576_step(void)
{
    uint32_t frame;
    if(eureka576_effect==2u) {
        frame=eureka576_stutter_start+
              (eureka576_stutter_step++%eureka576_stutter_span);
        return eureka576_seek(frame);
    }
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
        eureka576_have_change=0u;
    }
    eureka576_was_playing=playing;
    if(eureka576_enabled && playing) {
        uint32_t tempo=(fw_tempo>=3600 && fw_tempo<=36000)?(uint32_t)fw_tempo:14400u;
        /* TRIGs are queued early; act only once their sound should play. */
        while(eureka576_event_head!=eureka576_event_tail) {
            uint32_t at=eureka576_event_times[eureka576_event_head];
            if((int32_t)(now-at)<0) break;
            eureka576_event_head=(eureka576_event_head+1u)&(E576_QUEUE_SIZE-1u);
            eureka576_trigger(at);
        }
        if(eureka576_effect && (int32_t)(now-eureka576_until)>=0) {
            eureka576_effect=0u;
            eureka576_speed=0u;
        }
        if(eureka576_effect!=1u || eureka576_speed!=3u) {
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
        eureka576_effect=0u;
        eureka576_speed=0u;
        eureka576_have_change=0u;
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
