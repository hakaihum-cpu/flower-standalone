/* SPDX-License-Identifier: GPL-2.0-or-later
 * Dedicated TRACK-hold Mod Menu entries and Digitone-native parameter pages.
 * No stock audio path is modified. One CORE_AUDIO_INSERT owner only.
 */
#include "rack.h"
#include "digitone-mk1/core3.h"
#include <stdint.h>
#define KEY_MIDI 2
#define KEY_FUNC 1
#define KEY_TEMPO 9
#define KEY_PLAY 11
#define KEY_STOP 12
#define KEY_YES 13
#define KEY_NO 14
#define KEY_LEFT 17
#define KEY_RIGHT 18
#define KEY_PAGE 19
#define KEY_TRIG 20
#define KEY_LFO 25
#define KEY_STEP1 26
#define KEY_STEP16 41
#define KEY_T1 42
#define KEY_T4 45
#define KEY_PUSH_A 46
#define KEY_PUSH_H 53
#define COUNTS_PER_DETENT 4
struct dnfx_state rack_state[DNFX_COUNT];
volatile int32_t rack_active=-1; /* OFF */
static volatile int32_t rack_pending=-2;
static uint8_t rack_pause=0u,rack_ui_open=0u,rack_no_owned=0u;
static uint8_t rack_controls_owned[64];
static uint8_t rack_ready=0u;
static volatile uint32_t rack_ramp=0u;
static volatile uint8_t rack_fading=0u;
static int32_t rack_acc[8];
struct core_audio_owner rack_owner={0,rack_render,CORE_AUDIO_INSERT};

static void rack_init(void)
{
    unsigned i,j;
    if(rack_ready) return;
    for(i=0u;i<DNFX_COUNT;++i) dnfx_defaults(&rack_state[i],i);
    for(j=0u;j<8u;++j) rack_acc[j]=0;
    rack_ready=1u;
}
void rack_choose(int mode)
{
    /* No large ring clearing in the render callback. The UI waits two
     * UI ticks with stock audio before rearming the one insert owner. */
    rack_init();
    /* Reselecting the current effect opens its page WITHOUT restarting
     * capture history or turning off an active sustained loop. */
    if(mode==rack_active && rack_pending==-2 && !rack_fading) {
        rack_ui_open=mode>=0;
        return;
    }
    rack_pending=(mode>=0 && mode<DNFX_COUNT)?mode:-1;
    if(rack_owner.on && rack_active>=0) {
        rack_fading=1u; /* old path fades to dry before release */
    } else {
        rack_owner.on=0;
        rack_active=-1;
        rack_pause=2u;
        rack_ramp=0u;
        rack_fading=0u;
    }
    rack_ui_open=mode>=0 && mode<DNFX_COUNT;
    {
        unsigned i;
        for(i=0u;i<8u;++i) rack_acc[i]=0;
    }
}
void rack_show_page(void) {if(rack_active>=0) rack_ui_open=1u;}
void rack_hide_page(void) {rack_ui_open=0u;}
static void menu_granular(void *b,void *e,int t)
{(void)b;(void)e;(void)t;rack_choose(DNFX_GRANULAR);}
static void menu_eureka(void *b,void *e,int t)
{(void)b;(void)e;(void)t;rack_choose(DNFX_EUREKA);}
static void menu_reel(void *b,void *e,int t)
{(void)b;(void)e;(void)t;rack_choose(DNFX_REEL);}
static void menu_crush(void *b,void *e,int t)
{(void)b;(void)e;(void)t;rack_choose(DNFX_CRUSH);}
static void menu_off(void *b,void *e,int t)
{(void)b;(void)e;(void)t;rack_choose(-1);}
const struct core_menu_item rack_menu_granular={"GRANULAR",menu_granular,0,0};
const struct core_menu_item rack_menu_eureka={"EUREKA FX",menu_eureka,0,0};
const struct core_menu_item rack_menu_reel={"REEL",menu_reel,0,0};
const struct core_menu_item rack_menu_crush={"BITCRUSH",menu_crush,0,0};
const struct core_menu_item rack_menu_off={"FX OFF",menu_off,0,0};

void rack_render(int32_t *out,const int32_t *in)
{
    int32_t active=rack_active;
    unsigned i;
    if(active==DNFX_GRANULAR)
        rack_granular_render(out,in);
    else if(active>=DNFX_EUREKA && active<DNFX_COUNT)
        rack_audio_render(out,in,active);
    else {
        for(i=0u;i<64u;++i) out[i]=in[i];
        return;
    }
    /* Smooth wet engagement; restarts on an FX choice or a bypass.
     * No dynamic allocation, no buffer resets in the audio callback. */
    for(i=0u;i<32u;++i) {
        unsigned ch;
        uint32_t gain;
        if(rack_fading) {
            if(rack_ramp>0u) --rack_ramp;
        } else if(rack_ramp<720u) ++rack_ramp;
        gain=rack_ramp;
        for(ch=0u;ch<2u;++ch) {
            unsigned n=i*2u+ch;
            if(gain==720u || out[n]==in[n]) continue;
            if(gain==0u) {out[n]=in[n];continue;}
            /* Only the transitional crossfade is 16-bit; the steady
             * dry path retains the stock master's full Q1.31 word. */
            {
                int32_t dry=in[n]>>16,wet=out[n]>>16;
                int32_t v=dry+((wet-dry)*(int32_t)gain)/720;
                out[n]=v*65536;
            }
        }
    }
}
static void rack_mark_dirty(void *ctrl)
{
    if(ctrl) *((volatile uint8_t *)ctrl+0x20)=1u;
}
void rack_tick(void *ctrl)
{
    int32_t pending;
    rack_init();
    if(rack_fading && rack_ramp==0u) {
        rack_owner.on=0;
        rack_active=-1;
        rack_fading=0u;
        rack_pause=2u;
    } else if(rack_fading) {
        /* Old insert stays active during its 15ms fade to stock sound. */
    } else if(rack_pause) --rack_pause;
    else if((pending=rack_pending)!=-2) {
        if(pending==DNFX_GRANULAR) rack_granular_reset();
        else if(pending>=DNFX_EUREKA && pending<DNFX_COUNT)
            rack_audio_reset(pending);
        rack_active=pending;
        rack_pending=-2;
        rack_owner.on=pending>=0;
        rack_mark_dirty(ctrl);
    }
    if(rack_active==DNFX_REEL && rack_owner.on)
        rack_audio_prepare_reel(rack_state[DNFX_REEL].value[0][0]);
    if(rack_ui_open && rack_active>=0) {
        dnfx_tick(&rack_state[(unsigned)rack_active]);
        rack_mark_dirty(ctrl);
    }
}
int rack_key(void *brain,void *ev)
{
    int key=*(int *)((uint8_t *)ev+12);
    int flags=*(int *)((uint8_t *)ev+16);
    int press=(flags&1) && !(flags&8);
    int down=(flags&1)!=0;
    (void)brain;
    if(key==KEY_NO && rack_no_owned && !down) {
        rack_no_owned=0u;return 1;
    }
    if(key>=0 && key<64 && rack_controls_owned[key]) {
        if(!down) rack_controls_owned[key]=0u;
        return 1;
    }
    if(!rack_ui_open || rack_active<0) return 0;
    /* Keep transport, all sixteen trigs, TRACK/MUTE, FUNC modifiers and
     * video shortcut MIDI+YES owned by the original firmware/EUREKA. */
    if(key==KEY_MIDI) {
        if(press) rack_ui_open=0u;
        return 0;
    }
    if(key==KEY_FUNC || key==KEY_PLAY || key==KEY_STOP ||
       key==KEY_TEMPO || key==10 ||
       (key>=KEY_STEP1 && key<=KEY_STEP16) ||
       (key>=KEY_T1 && key<=KEY_T4)) return 0;
    if(!press) return 0;
    /* No audio-panel operation may steal FUNC combinations used by the
     * original Digitone's pages, MIDI, or MUTE. */
    if(flags&2) return 0;
    if(key==KEY_NO) {
        rack_ui_open=0u;rack_no_owned=1u;return 1;
    }
    if(key>=KEY_TRIG && key<=KEY_LFO) {
        /* Stock page selection is always available while an FX runs.
         * The visual EUREKA module also needs these six native keys. */
        rack_ui_open=0u;
        return 0;
    }
    if(key==KEY_LEFT || key==KEY_RIGHT || key==KEY_PAGE) {
        struct dnfx_state *s=&rack_state[(unsigned)rack_active];
        unsigned pages=dnfx_page_count(s->effect);
        if(pages>1u) {
            unsigned next=(s->page+(key==KEY_LEFT?pages-1u:1u))%pages;
            dnfx_set_page(s,next);
            rack_controls_owned[key]=1u;
            return 1;
        }
        /* Other FX pages have no second panel; use stock page key. */
        rack_ui_open=0u;
        return 0;
    }
    if(key>=KEY_PUSH_A && key<=KEY_PUSH_H) {
        struct dnfx_state *s=&rack_state[(unsigned)rack_active];
        unsigned k=(unsigned)(key-KEY_PUSH_A);
        const struct dnfx_control *p=dnfx_control_at(s->effect,s->page,k);
        if(p) {
            s->value[s->page][k]=p->initial;
            s->focus=(uint8_t)k;s->flash=50u;
            if(s->effect==DNFX_GRANULAR)
                rack_granular_set(k,p->initial);
        }
        if(p) {
            rack_controls_owned[key]=1u;
            return 1;
        }
        return 0; /* Unassigned knob presses belong to stock. */
    }
    /* YES and all other keys keep their original Digitone meaning. */
    return 0;
}
int rack_enc(void *brain,void *ev)
{
    int id=*(int *)((uint8_t *)ev+12),delta=*(int *)((uint8_t *)ev+16);
    unsigned k;
    struct dnfx_state *s;
    int32_t d;
    (void)brain;
    if(!rack_ui_open || rack_active<0 || id<1 || id>8) return 0;
    s=&rack_state[(unsigned)rack_active];
    k=(unsigned)(id-1);
    rack_acc[k]+=delta;
    d=rack_acc[k]/COUNTS_PER_DETENT;
    rack_acc[k]-=d*COUNTS_PER_DETENT;
    if(d && dnfx_edit(s,k,(int)d) && s->effect==DNFX_GRANULAR)
        rack_granular_set(k,s->value[s->page][k]);
    return 1;
}
int rack_cc(int track,int cc,int value,int flags)
{
    if(rack_active!=DNFX_GRANULAR) return 0;
    return rack_granular_cc(track,cc,value,flags);
}
void rack_draw(void *bmp,void *ctrl)
{
    (void)ctrl;
    if(!rack_ui_open || rack_active<0) return;
    dnfx_draw(bmp,&rack_state[(unsigned)rack_active],
              (unsigned)fw_tempo/120u);
}
