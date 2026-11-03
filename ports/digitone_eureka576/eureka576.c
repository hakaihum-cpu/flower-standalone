/* SPDX-License-Identifier: GPL-2.0-or-later
 * EUREKA 576-frame Digitone mk1 / Keys OS 1.44 display-only mod.
 * EUREKA user frames retain the approved 04 HYBRID black/yellow images.
 * Stock MUTE key events pass through; actual MUTE AUDIO with
 * video ON is still unverified on hardware. Stock audio untouched.
 */
#include <stdint.h>
#include "digitone-mk1/core3.h"
#define E576_COUNT 576u
#define E576_FRAME_BYTES 1024u
#define E576_KEY_MIDI 2
#define E576_KEY_YES 13
#define E576_KEY_FUNC 1
/* Video CCs intentionally avoid all stock Digitone OS1.41 Appendix B
 * parameter CCs and the existing granular 33/34/36/37/60.
 * MIDI standard: 65/67/68/69 are generic pedal switches; use only
 * on Digitone AUTO channel with the video mod enabled.
 */
#define E576_CC_GLITCH 40
#define E576_CC_STUTTER 65
#define E576_CC_SPEED 67
#define E576_CC_SCRUB 68
#define E576_CC_INVERT 69
extern const uint8_t eureka576_frames[];
extern const uint8_t eureka576_frames_end[];
extern const uint8_t eureka576_seek_table[];
extern const uint8_t eureka576_seek_table_end[];
static uint8_t eureka576_framebuf[E576_FRAME_BYTES];
static uint32_t eureka576_readpos, eureka576_index, eureka576_phase;
static uint32_t eureka576_midi_held, eureka576_enabled, eureka576_dirty;
/* MIDI+YES must never select the stock MIDI MUTE bank. The MIDI-down event
 * comes BEFORE the YES down event, so forwarding MIDI immediately already
 * changes the bank. Defer it, and either consume MIDI+YES as a whole or
 * replay the stock MIDI-down before any other chord's first key event. */
enum { E576_MIDI_IDLE=0, E576_MIDI_PENDING=1,
       E576_MIDI_PASSED=2, E576_MIDI_VIDEO_CHORD=3 };
static uint32_t eureka576_midi_state;
/* Captured YES down must have a captured YES up, even when MIDI is
 * released before YES. No other key is intercepted by this flag. */
static uint32_t eureka576_yes_owned;
/* OS1.44 original KEYDISP, as documented by the pinned core-dn1 port.
 * In tests the exact same call is replaced with a key-event observer.
 * Never recursively call core_dn_key/core_key: those dispatch to us. */
#ifndef E576_STOCK_KEYDISP
#define E576_STOCK_KEYDISP ((void (*)(void *,void *))0x40016fc0u)
#endif
static void eureka576_stock_midi_down(void *brain,void *ev)
{
    int *key=(int *)((uint8_t *)ev+12);
    int *flags=(int *)((uint8_t *)ev+16);
    int old_key=*key, old_flags=*flags;
    *key=E576_KEY_MIDI; *flags=1;
    E576_STOCK_KEYDISP(brain,ev);
    *key=old_key; *flags=old_flags;
}
/* Video-only random sequence state; no changes to audio or sound parameters. */
#define E576_SEEK_ENTRY 1028u
#define E576_SEEK_EVERY 6u
#define E576_SEEK_COUNT 96u
#define E576_STEP_TIME 5400000u
#define E576_CLIP_FRAMES 36u
#define E576_CLIPS (E576_COUNT / E576_CLIP_FRAMES)
#define E576_QUEUE_SIZE 32u
_Static_assert(E576_CLIPS == 16u, "16 separate EUREKA scenes");
static uint32_t eureka576_rng=0x534a5531u;
static uint32_t eureka576_effect, eureka576_speed, eureka576_until, eureka576_reverse;
static uint32_t eureka576_stutter_start, eureka576_stutter_span, eureka576_stutter_step;
static uint32_t eureka576_was_playing;
/* Auto scene-cut deadline in the musical timeline. UI task only;
 * zero means "schedule a new random cut interval". */
static uint32_t eureka576_next_scene_cut;
/* MIDI task writes only aligned scalar controls, never framebuffer/state.
 * UI tick applies seeks; UI draw applies lightweight per-row transforms.
 * Defaults: glitch off, stutter off, speed auto, scrub off, invert off.
 */
static volatile uint8_t eureka576_cc_glitch = 0u;
static volatile uint8_t eureka576_cc_stutter = 0u;
static volatile uint8_t eureka576_cc_speed = 64u;
static volatile uint8_t eureka576_cc_scrub = 0u;
static volatile uint8_t eureka576_cc_invert = 0u;
static volatile uint32_t eureka576_cc_serial;
static uint32_t eureka576_cc_serial_seen;
static uint32_t eureka576_stutter_anchor, eureka576_cc_stutter_step;
static uint8_t eureka576_stutter_seen, eureka576_scrub_seen;
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
/* Before the stock MIDI CC router applies a CC, core's AUTO flag
 * identifies the selected track's AUTO channel. Only take video CCs
 * while EUREKA is enabled, and never consume other stock controls. */
int eureka576_cc(int32_t track, int32_t cc, int32_t value, int32_t flags)
{
    volatile uint8_t *destination;
    (void)track;
    if(!eureka576_enabled || (((uint32_t)flags & CORE_MIDI_CC_AUTO)==0u) ||
       value<0 || value>127) return 0;
    switch(cc) {
    case E576_CC_GLITCH: destination=&eureka576_cc_glitch; break;
    case E576_CC_STUTTER: destination=&eureka576_cc_stutter; break;
    case E576_CC_SPEED: destination=&eureka576_cc_speed; break;
    case E576_CC_SCRUB: destination=&eureka576_cc_scrub; break;
    case E576_CC_INVERT: destination=&eureka576_cc_invert; break;
    default: return 0;
    }
    *destination=(uint8_t)value;
    ++eureka576_cc_serial;  /* UI task notices MIDI updates even when STOPPED */
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
/* The original 576 frames are sixteen independent 36-frame scenes.
 * Pick a DIFFERENT scene, so cuts never merely jump within one scene
 * or step to the next scene in chronological order. */
static int eureka576_random_scene(void)
{
    uint32_t old=eureka576_index/E576_CLIP_FRAMES;
    uint32_t delta=1u+eureka576_random()%(E576_CLIPS-1u);
    uint32_t scene=(old+delta)%E576_CLIPS;
    uint32_t frame=scene*E576_CLIP_FRAMES+(eureka576_random()%6u);
    eureka576_effect=0u;
    eureka576_speed=0u;
    eureka576_phase=0u;
    eureka576_stutter_anchor=frame;
    eureka576_cc_stutter_step=0u;
    return eureka576_seek(frame);
}
/* An audible note still produces a video action, but a majority of
 * note actions now visibly CUT to a different 36-frame scene. */
static void eureka576_trigger(uint32_t time)
{
    uint32_t action, inside, duration, frame;
    action=eureka576_random()%100u;
    eureka576_effect=0u;
    eureka576_speed=0u;
    eureka576_phase=0u; /* keep the audible note's first visual jump visible */
    if(action<55u) {
        if(!eureka576_random_scene()) eureka576_enabled=0u;
    } else if(action<70u) {
        /* Reverse stays enabled until the next reverse event. */
        eureka576_reverse^=1u;
        frame=(eureka576_index+(eureka576_reverse?E576_COUNT-6u:6u))
              %E576_COUNT;
        if(!eureka576_seek(frame)) eureka576_enabled=0u;
    } else if(action<85u) {
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
    eureka576_dirty=1u;
}
static int eureka576_step(void)
{
    uint32_t frame, span;
    if(eureka576_cc_stutter) {
        /* Span is 2..8 frames, constrained within the source 36-frame clip. */
        span=2u+(((uint32_t)eureka576_cc_stutter-1u)*6u)/126u;
        frame=eureka576_stutter_anchor-
              (eureka576_stutter_anchor%36u)+
              ((eureka576_stutter_anchor%36u+
                (eureka576_cc_stutter_step++%span))%36u);
        return eureka576_seek(frame);
    }
    if(eureka576_cc_speed>=112u) {
        uint32_t start=(eureka576_index/E576_CLIP_FRAMES)*E576_CLIP_FRAMES;
        return eureka576_seek(eureka576_index>start
                              ?eureka576_index-1u:start+E576_CLIP_FRAMES-1u);
    }
    if(eureka576_effect==2u) {
        frame=eureka576_stutter_start+
              (eureka576_stutter_step++%eureka576_stutter_span);
        return eureka576_seek(frame);
    }
    if(eureka576_reverse) {
        uint32_t start=(eureka576_index/E576_CLIP_FRAMES)*E576_CLIP_FRAMES;
        return eureka576_seek(eureka576_index>start
                              ?eureka576_index-1u:start+E576_CLIP_FRAMES-1u);
    }
    /* Stay inside the current scene until a genuine random scene cut.
     * Never expose the next scene merely by playing frames in order. */
    if((eureka576_index+1u)%E576_CLIP_FRAMES==0u)
        return eureka576_seek(eureka576_index+1u-E576_CLIP_FRAMES);
    ++eureka576_index;
    return eureka576_next();
}
void eureka576_tick(void *ctrl)
{
    uint32_t playing=(fw_transport==1u), now=fw_timeline;
    uint32_t had_note=0u;
    if(!playing && eureka576_was_playing) {
        eureka576_event_head=eureka576_event_tail;
        eureka576_effect=0u;
        eureka576_speed=0u;
        eureka576_next_scene_cut=0u;
    }
    if(playing && !eureka576_was_playing)
        eureka576_next_scene_cut=0u;
    eureka576_was_playing=playing;
    /* UI-only handling: never decode 576-frame data in the MIDI task. */
    if(eureka576_cc_serial_seen!=eureka576_cc_serial) {
        eureka576_cc_serial_seen=eureka576_cc_serial;
        eureka576_dirty=1u;
    }
    if(eureka576_cc_stutter!=eureka576_stutter_seen) {
        eureka576_stutter_seen=eureka576_cc_stutter;
        eureka576_stutter_anchor=eureka576_index;
        eureka576_cc_stutter_step=0u;
    }
    if(eureka576_cc_scrub!=eureka576_scrub_seen) {
        eureka576_scrub_seen=eureka576_cc_scrub;
        if(eureka576_enabled && eureka576_scrub_seen!=0u) {
            uint32_t index=
                ((uint32_t)(eureka576_scrub_seen-1u)*575u)/126u;
            if(!eureka576_seek(index)) eureka576_enabled=0u;
        }
    }
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
        if(have_note && !eureka576_cc_scrub) {
            had_note=1u;
            eureka576_trigger(note_time);
            if(eureka576_cc_stutter) {
                eureka576_stutter_anchor=eureka576_index;
                eureka576_cc_stutter_step=0u;
            }
        }
    }
    /* A reliable random cut with no new voices is essential: a muted or
     * quiet sequencer must not reduce EUREKA to sequential playback.
     * 8/12/16 sixteenth steps = 2/3/4 beats at the host tempo.
     * Suppress a second cut in a tick where a note just caused one. */
    if(eureka576_enabled && playing && !eureka576_cc_scrub &&
       eureka576_cc_speed>=16u) {
        if(eureka576_next_scene_cut==0u) {
            uint32_t steps=8u+4u*(eureka576_random()%3u);
            eureka576_next_scene_cut=now+steps*E576_STEP_TIME;
        } else if(!had_note &&
                  (int32_t)(now-eureka576_next_scene_cut)>=0) {
            if(!eureka576_random_scene()) eureka576_enabled=0u;
            eureka576_next_scene_cut=0u;
        }
    }
    if(eureka576_enabled && playing && !eureka576_cc_scrub) {
        uint32_t tempo=(fw_tempo>=3600 && fw_tempo<=36000)?(uint32_t)fw_tempo:14400u;
        if(eureka576_effect && (int32_t)(now-eureka576_until)>=0) {
            eureka576_effect=0u;
            eureka576_speed=0u;
        }
        {
            /* 0..15 freeze, 16..47 half, 48..79 AUTO, 80..111 2x,
             * 112..127 reverse. AUTO retains the HG76 note effects. */
            uint32_t speed=eureka576_cc_speed;
            uint32_t auto_speed=(speed>=48u && speed<80u);
            uint32_t freeze=(speed<16u) ||
                (auto_speed && eureka576_effect==1u &&
                 eureka576_speed==3u);
            uint32_t delta=9u*tempo;
            if(speed<48u) delta/=2u;
            else if(speed>=80u && speed<112u) delta*=2u;
            else if(auto_speed && eureka576_effect==1u) {
                if(eureka576_speed==1u) delta/=2u;
                if(eureka576_speed==2u) delta*=2u;
            }
            if(!freeze) {
                eureka576_phase+=delta;
                if(eureka576_phase>=864000u) {
                    eureka576_phase-=864000u;
                    if(!eureka576_step()) eureka576_enabled=0u;
                    eureka576_dirty=1u;
                }
            }
        }
    }
    if(eureka576_enabled && (eureka576_cc_glitch || eureka576_cc_invert))
        eureka576_dirty=1u; /* animate row effects at UI tick, not audio rate */
    if(eureka576_dirty) {
        *((volatile uint8_t *)ctrl+0x20)=1u;
        eureka576_dirty=0u;
    }
}
/* Draw black frame, then horizontal yellow runs on the portable Bitmap.
 * y=0 is bottom in the firmware; y=0 is top in the source image.
 */
void eureka576_draw(void *bmp, void *ctrl)
{
    uint32_t x,y;
    (void)ctrl;
    if(!eureka576_enabled) return;
    fw_fillrect(bmp,0,0,127,63,0);
    for(y=0u;y<64u;++y) {
        uint32_t page=7u-(y>>3);
        uint8_t bit=(uint8_t)(1u<<(y&7u));
        uint32_t amp=((uint32_t)eureka576_cc_glitch+15u)/16u;
        uint32_t stripe=y>>2;
        uint32_t invert=0u, hash=0u;
        int32_t shift=0, start=-1;
        if(amp) {
            /* One offset per 4-line band: bounded +-8 pixels, not a
             * per-pixel random generator. Decoding cost stays fixed. */
            hash=(stripe*1103515245u) ^
                 ((eureka576_index+(fw_timeline>>15))*2654435761u);
            hash^=hash>>13;
            shift=(int32_t)(hash%(2u*amp+1u))-(int32_t)amp;
        }
        if(eureka576_cc_invert) {
            /* 0 off; 127 inverted everywhere; intermediate values
             * invert a variable fraction of four-line video stripes. */
            if(eureka576_cc_invert==127u) invert=1u;
            else {
                hash=(stripe*29u+(fw_timeline>>17))&127u;
                invert=(hash<(uint32_t)eureka576_cc_invert);
            }
        }
        /* Scan pixels once; flush consecutive yellow runs with 1 rect.
         * When all CCs are zero this is exactly the original 128x64
         * 1-bit image, not a new drawing, mask or source animation.
         */
        for(x=0u;x<=128u;++x) {
            int32_t sx=(int32_t)x-shift;
            uint32_t lit=0u;
            if(x<128u && sx>=0 && sx<128) {
                lit=(eureka576_framebuf[page+8u*(uint32_t)sx]&bit)!=0u;
            }
            if(x<128u && invert) lit^=1u;
            if(lit) {
                if(start<0) start=(int32_t)x;
            } else if(start>=0) {
                fw_fillrect(bmp,start,(int32_t)(63u-y),
                            (int32_t)x-1,(int32_t)(63u-y),1);
                start=-1;
            }
        }
    }
}
int eureka576_key(void *brain,void *ev)
{
    int key=*(int *)((uint8_t *)ev+12);
    int flags=*(int *)((uint8_t *)ev+16);
    int first=(flags&1) && !(flags&8);

    if(key==E576_KEY_MIDI) {
        if(first) {
            eureka576_midi_held=1u;
            if(!(flags&2)) {
                /* A potential MIDI+YES chord must be invisible to the
                 * stock MIDI track-bank selector until YES is known. */
                eureka576_midi_state=E576_MIDI_PENDING;
                return 1;
            }
            /* FUNC+MIDI belongs to the stock firmware. */
            eureka576_midi_state=E576_MIDI_PASSED;
            return 0;
        }
        if(!(flags&1)) {
            eureka576_midi_held=0u;
            if(eureka576_midi_state==E576_MIDI_PENDING) {
                /* MIDI alone: complete a normal stock press/release
                 * on release, with no duplicated MIDI-down event. */
                eureka576_midi_state=E576_MIDI_IDLE;
                eureka576_stock_midi_down(brain,ev);
                return 0; /* stock receives the genuine MIDI release */
            }
            if(eureka576_midi_state==E576_MIDI_VIDEO_CHORD) {
                eureka576_midi_state=E576_MIDI_IDLE;
                return 1; /* stock saw neither half of this MIDI */
            }
            eureka576_midi_state=E576_MIDI_IDLE;
            return 0; /* prior stock MIDI DOWN gets its stock UP */
        }
        /* Hold/repeat events from a deferred/captured MIDI key must not
         * leak into stock when it never received the DOWN event. */
        if(eureka576_midi_state==E576_MIDI_PENDING ||
           eureka576_midi_state==E576_MIDI_VIDEO_CHORD)
            return 1;
        return 0;
    }

    /* A consumed YES-down has a consumed YES-up regardless of MIDI order. */
    if(key==E576_KEY_YES && eureka576_yes_owned && !(flags&1)) {
        eureka576_yes_owned=0u;
        return 1;
    }

    if(eureka576_midi_state==E576_MIDI_PENDING && first) {
        if(key==E576_KEY_YES && !(flags&2)) {
            eureka576_midi_state=E576_MIDI_VIDEO_CHORD;
        } else {
            /* MIDI+TRACK, MIDI+bank, FUNC+YES etc remain stock chords.
             * Give stock MIDI DOWN before forwarding the second key. */
            eureka576_midi_state=E576_MIDI_PASSED;
            eureka576_stock_midi_down(brain,ev);
        }
    }

    /* All FUNC+BANK, FUNC+TRACK, PTN+TRACK, and stock key navigation
     * pass unchanged. No parallel MUTE database and no LED spoofing. */
    if((flags&2)!=0) return 0;
    if(key==E576_KEY_YES && first &&
       eureka576_midi_state==E576_MIDI_VIDEO_CHORD &&
       eureka576_midi_held) {
        eureka576_yes_owned=1u;
        eureka576_enabled^=1u;
        eureka576_phase=0u;
        eureka576_effect=0u;
        eureka576_speed=0u;
        eureka576_reverse=0u;
        eureka576_event_head=eureka576_event_tail;
        eureka576_next_scene_cut=0u;
        eureka576_scrub_seen=0u;
        eureka576_stutter_seen=0u;
        eureka576_cc_glitch=0u;
        eureka576_cc_stutter=0u;
        eureka576_cc_speed=64u;
        eureka576_cc_scrub=0u;
        eureka576_cc_invert=0u;
        eureka576_cc_serial++;
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
