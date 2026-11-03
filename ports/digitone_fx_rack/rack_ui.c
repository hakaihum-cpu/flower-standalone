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
static uint32_t rack_ramp=0u;
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
    rack_owner.on=0;
    rack_active=-1;
    rack_pending=(mode>=0 && mode<DNFX_COUNT)?mode:-1;
    rack_pause=2u;
    rack_ramp=0u;
    rack_ui_open=mode>=0 && mode<DNFX_COUNT;
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
        uint32_t gain=rack_ramp<720u?rack_ramp:720u;
        for(ch=0u;ch<2u;++ch) {
            unsigned n=i*2u+ch;
            int32_t dry=in[n]>>16,wet=out[n]>>16;
            int32_t v=dry+((wet-dry)*(int32_t)gain)/720;
            out[n]=v*65536;
        }
        if(rack_ramp<720u) ++rack_ramp;
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
    if(rack_pause) --rack_pause;
    else if((pending=rack_pending)!=-2) {
        if(pending==DNFX_GRANULAR) rack_granular_reset();
        else if(pending>=DNFX_EUREKA && pending<DNFX_COUNT)
            rack_audio_reset(pending);
        rack_active=pending;
        rack_pending=-2;
        rack_owner.on=pending>=0;
        rack_mark_dirty(ctrl);
    }
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
    if(key==KEY_NO) {
        rack_ui_open=0u;rack_no_owned=1u;return 1;
    }
    if(key==KEY_LEFT || key==KEY_RIGHT || key==KEY_PAGE ||
       (key>=KEY_TRIG && key<=KEY_LFO)) {
        struct dnfx_state *s=&rack_state[(unsigned)rack_active];
        unsigned pages=dnfx_page_count(s->effect);
        unsigned next=s->page;
        if(pages>1u) {
            if(key==KEY_LEFT) next=(next+pages-1u)%pages;
            else if(key==KEY_TRIG) next=0u;
            else if(key==KEY_TRIG+1) next=1u;
            else next=(next+1u)%pages;
        }
        dnfx_set_page(s,next);
        rack_controls_owned[key]=1u;
        return 1;
    }
    if(key>=KEY_PUSH_A && key<=KEY_PUSH_H) {
        struct dnfx_state *s=&rack_state[(unsigned)rack_active];
        unsigned k=(unsigned)(key-KEY_PUSH_A);
        const struct dnfx_control *p=dnfx_control_at(s->effect,s->page,k);
        if(p) {
            s->value[s->page][k]=p->initial;
            s->focus=(uint8_t)k;s->flash=50u;
        }
        rack_controls_owned[key]=1u;
        return 1;
    }
    if(key==KEY_YES) {rack_controls_owned[key]=1u;return 1;}
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
    if(d) dnfx_edit(s,k,(int)d);
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
