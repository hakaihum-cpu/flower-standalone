/* Native model tests; no Digitone firmware or sound engine is modified. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Preserve the exact failing assertion line in Actions logs. The native
 * decoder/pixel suite contains many unconditional abort() checks. */
#define abort() do { \
    fprintf(stderr,"PG79_NATIVE_FAIL %s:%d\\n",__FILE__,__LINE__); \
    fflush(stderr); \
    __builtin_abort(); \
} while(0)
volatile uint32_t fw_transport=0u, fw_timeline=0u;
volatile int32_t fw_tempo=14400;
static uint8_t display[64][128];
static unsigned rectangle_count;
static unsigned text_count, panel_frames;
const char fw_font_title[1]={0}, fw_font_label[1]={0};
int32_t fw_textf(void *bmp,const void *font,int32_t x,int32_t y,
                 int32_t maxlen,const char *fmt,...)
{
    (void)bmp; (void)font; (void)fmt;
    if(x<0 || x>127 || y<0 || y>63 || (maxlen!= -1 && maxlen<1)) abort();
    ++text_count;
    return 0;
}
void fw_framerect(void *bmp,int32_t x0,int32_t y0,int32_t x1,
                  int32_t y1,int32_t c)
{
    (void)bmp;
    if(x0<0 || y0<0 || x1>127 || y1>63 ||
       x0>=x1 || y0>=y1 || c!=1) abort();
    ++panel_frames;
}
void fw_fillrect(void *bmp,int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t c)
{
    (void)bmp;
    if(x0<0 || y0<0 || x1>127 || y1>63 || x0>x1 || y0>y1 || c<0 || c>1) abort();
    ++rectangle_count;
    for(int32_t y=y0;y<=y1;++y)
        for(int32_t x=x0;x<=x1;++x) display[y][x]=(uint8_t)c;
}
/* Stock OS1.44 KEYDISP normally handles MIDI. The native test replaces
 * only the direct delayed MIDI-down with an observer. */
static unsigned stock_midi_downs;
static void native_stock_keydisp(void *brain,void *ev)
{
    const int *key=(const int *)((const uint8_t *)ev+12);
    const int *flags=(const int *)((const uint8_t *)ev+16);
    (void)brain;
    if(*key!=2 || *flags!=1) abort();
    ++stock_midi_downs;
}
#define E576_STOCK_KEYDISP native_stock_keydisp
#include "../eureka576.c"
extern const uint8_t eureka576_reference[],eureka576_reference_end[];
static void check(unsigned index)
{
    if(index>=E576_COUNT ||
       memcmp(eureka576_framebuf,eureka576_reference+index*1024u,1024u)!=0) {
        fprintf(stderr,"bad decoded frame %u\n",index); abort();
    }
    memset(display,0xa5,sizeof(display));
    eureka576_draw(NULL,NULL);
    for(unsigned y=0;y<64u;++y) for(unsigned x=0;x<128u;++x) {
        unsigned expected=((unsigned)eureka576_reference[index*1024u+
                           (7u-(y>>3))+8u*x]>>(y&7u))&1u;
        if((unsigned)display[63u-y][x]!=expected) {
            fprintf(stderr,"pixel mismatch frame %u x %u y %u\n",index,x,y);
            abort();
        }
    }
}
int main(void)
{
    uint8_t ctrl[128]={0};
    int32_t key[6]={0};
    unsigned speeds=0,stutters=0,jumps=0,reverses=0;
    if((uintptr_t)eureka576_reference_end-(uintptr_t)eureka576_reference!=(uintptr_t)(E576_COUNT*E576_FRAME_BYTES))
        abort();
    key[3]=2;key[4]=1;
    if(eureka576_key(NULL,key)!=1 ||
       eureka576_midi_state!=E576_MIDI_PENDING) abort();
    if(stock_midi_downs!=0u) abort();
    key[3]=13;
    if(eureka576_key(NULL,key)!=1 || !eureka576_enabled ||
       eureka576_midi_state!=E576_MIDI_VIDEO_CHORD) abort();
    /* Crucial: this shortcut must not change the stock MIDI/MUTE bank.
     * The old firmware forwarded MIDI DOWN and broke audio-track MUTE. */
    if(stock_midi_downs!=0u) abort();
    if(!eureka576_yes_owned) abort();
    /* VM76 only intercepted YES DOWN and forwarded its unmatched UP.
     * Both halves of a consumed MIDI+YES gesture must stay out of stock.
     */
    key[3]=E576_KEY_YES;key[4]=0x10;
    if(eureka576_key(NULL,key)!=1 || eureka576_yes_owned) abort();
    /* A second, unowned YES key-up stays in the stock dispatcher. */
    key[4]=0;
    if(eureka576_key(NULL,key)!=0) abort();
    key[3]=E576_KEY_MIDI;key[4]=0;
    if(eureka576_key(NULL,key)!=1 ||
       eureka576_midi_state!=E576_MIDI_IDLE ||
       stock_midi_downs!=0u) abort();
    check(0);
    /* EUREKA VIDEO must NEVER suppress stock TRIG/KEY 1..16. */
    for(int k=E576_KEY_STEP1;k<=E576_KEY_STEP16;++k) {
        key[3]=k;key[4]=1;
        if(eureka576_key(NULL,key)!=0) abort();
        key[4]=9; /* stock-held event */
        if(eureka576_key(NULL,key)!=0) abort();
        key[4]=0;
        if(eureka576_key(NULL,key)!=0) abort();
    }
    /* MIDI first, FUNC second toggles a page-key mode without ever
     * dispatching MIDI DOWN to stock (critical audio MUTE regression). */
    key[3]=E576_KEY_MIDI;key[4]=1;
    if(eureka576_key(NULL,key)!=1) abort();
    key[3]=E576_KEY_FUNC;key[4]=3;
    if(eureka576_key(NULL,key)!=1 || !eureka576_param_armed ||
       eureka576_param_page!=0u || !eureka576_func_owned ||
       stock_midi_downs!=0u) abort();
    key[4]=0;
    if(eureka576_key(NULL,key)!=1 || eureka576_func_owned) abort();
    key[3]=E576_KEY_MIDI;key[4]=0;
    if(eureka576_key(NULL,key)!=1 || stock_midi_downs!=0u) abort();

    /* All six pages open with their own key. A-D are real encoder
     * controls, E-H are captured so hidden stock parameters don't turn.
     * TRACK keys and the full TRIG1..16 keyboard still reach stock. */
    for(int page=0;page<6;++page) {
        key[3]=E576_KEY_TRIG+page;key[4]=1;
        if(eureka576_key(NULL,key)!=1 ||
           eureka576_param_page!=(uint32_t)(page+1)) abort();
        key[4]=0;
        if(eureka576_key(NULL,key)!=1) abort();
        text_count=panel_frames=0u;
        eureka576_draw(NULL,NULL);
        if(text_count!=9u || panel_frames!=4u) abort();
        for(int k=E576_KEY_STEP1;k<=E576_KEY_STEP16;++k) {
            key[3]=k;key[4]=1;
            if(eureka576_key(NULL,key)!=0) abort();
            key[4]=9;
            if(eureka576_key(NULL,key)!=0) abort();
            key[4]=3; /* FUNC+STEP also remains with stock */
            if(eureka576_key(NULL,key)!=0) abort();
            key[4]=0;
            if(eureka576_key(NULL,key)!=0) abort();
        }
        key[3]=19;key[4]=3; /* FUNC+BANK MUTE */
        if(eureka576_key(NULL,key)!=0 || stock_midi_downs!=0u) abort();
        for(int k=42;k<=45;++k) {
            key[3]=k;key[4]=1;
            if(eureka576_key(NULL,key)!=0) abort();
        }
        for(int enc=5;enc<=8;++enc) {
            key[3]=enc;key[4]=4;
            if(eureka576_enc(NULL,key)!=1) abort();
        }
        /* Param changes route through the same callback as the Digitone.
         * Read both fields to confirm A-D are not decorative knobs. */
        for(int enc=1;enc<=4;++enc) {
            uint32_t before=eureka576_param_read((unsigned)page,
                                                 (unsigned)(enc-1));
            uint32_t max=eureka576_param_max[page][enc-1];
            uint32_t after;
            key[3]=enc;
            key[4]=(before>=max)?-4:4;
            if(eureka576_enc(NULL,key)!=1) abort();
            after=eureka576_param_read((unsigned)page,(unsigned)(enc-1));
            if(before==after) abort();
        }
        /* Same key again returns to the original animation. */
        key[3]=E576_KEY_TRIG+page;key[4]=1;
        if(eureka576_key(NULL,key)!=1 ||
           eureka576_param_page!=0u) abort();
        key[4]=0;
        if(eureka576_key(NULL,key)!=1) abort();
    }
    /* Restore exact original video defaults before pixel-test sweep. */
    {
        const uint8_t init[6][4]={
            {0u,55u,1u,0u},{64u,0u,0u,0u},
            {2u,100u,0u,64u},{0u,0u,8u,8u},
            {127u,0u,0u,0u},{32u,0u,0u,0u}
        };
        for(unsigned p=0u;p<6u;++p)
            for(unsigned k=0u;k<4u;++k)
                eureka576_param[p][k]=init[p][k];
        eureka576_cc_glitch=0u;
        eureka576_cc_invert=0u;
        eureka576_cc_speed=64u;
        eureka576_cc_stutter=0u;
        eureka576_cc_scrub=0u;
        eureka576_reverse=0u;
        eureka576_rng=0x534a5531u;
        eureka576_ui_ticks=0u;
        eureka576_next_scene_cut=0u;
        eureka576_cc_serial++;
    }
    if(!eureka576_seek(0u)) abort();
    check(0u);
    /* MIDI+FUNC while armed closes the dedicated page mode. */
    key[3]=E576_KEY_MIDI;key[4]=1;
    if(eureka576_key(NULL,key)!=1) abort();
    key[3]=E576_KEY_FUNC;key[4]=3;
    if(eureka576_key(NULL,key)!=1 || eureka576_param_armed ||
       eureka576_param_page!=0u || stock_midi_downs!=0u) abort();
    key[4]=0;
    if(eureka576_key(NULL,key)!=1) abort();
    key[3]=E576_KEY_MIDI;key[4]=0;
    if(eureka576_key(NULL,key)!=1) abort();
    /* With page mode disarmed all six normal page keys pass to stock. */
    for(int page=0;page<6;++page) {
        key[3]=E576_KEY_TRIG+page;key[4]=1;
        if(eureka576_key(NULL,key)!=0) abort();
        key[4]=0;
        if(eureka576_key(NULL,key)!=0) abort();
    }
    key[3]=1;key[4]=4;
    if(eureka576_enc(NULL,key)!=0) abort();
    /* Also accept reverse chord ordering: FUNC first, then MIDI. */
    key[3]=E576_KEY_FUNC;key[4]=1;
    if(eureka576_key(NULL,key)!=0) abort();
    key[3]=E576_KEY_MIDI;key[4]=3;
    if(eureka576_key(NULL,key)!=1 || !eureka576_param_armed ||
       stock_midi_downs!=0u) abort();
    key[4]=0;
    if(eureka576_key(NULL,key)!=1) abort();
    key[3]=E576_KEY_FUNC;key[4]=0;
    if(eureka576_key(NULL,key)!=0) abort();
    /* Leave page mode disarmed for original GH76 regression suite. */
    eureka576_param_armed=0u;
    eureka576_param_page=0u;
    check(0u);
    /* Exhaustive all-frame random seek and 4,718,592 display pixel comparisons. */
    for(unsigned i=0u;i<E576_COUNT;++i) {
        if(!eureka576_seek(i)) abort();
        check(i);
    }
    if(eureka576_seek(E576_COUNT)) abort();
    /* Preserve the two independent supplied banks on both sides of 576. */
    for(unsigned i=570u;i<=582u;++i) {
        if(!eureka576_seek(i)) abort();
        check(i);
    }
    if(!eureka576_seek(1151u)) abort();
    check(1151u);
    if(!eureka576_seek(576u)) abort();
    check(576u);

    /* Forward and reverse must loop INSIDE the selected 36-frame scene;
     * boundary must not enter the next scene in linear order. */
    if(!eureka576_seek(128u)) abort();
    eureka576_effect=0u;
    eureka576_reverse=1u;
    if(!eureka576_step() || eureka576_index!=127u) abort();
    check(127u);
    eureka576_reverse=0u;
    if(!eureka576_step() || eureka576_index!=128u) abort();
    check(128u);
    if(!eureka576_seek(0u)) abort();
    eureka576_reverse=1u;
    if(!eureka576_step() || eureka576_index!=35u) abort();
    check(35u);
    eureka576_reverse=0u;
    if(!eureka576_step() || eureka576_index!=0u) abort();
    check(0u);
    for(unsigned clip=0u;clip<E576_CLIPS;++clip) {
        unsigned base=clip*E576_CLIP_FRAMES;
        if(!eureka576_seek(base)) abort();
        for(unsigned i=1u;i<E576_CLIP_FRAMES;++i) {
            if(!eureka576_step() || eureka576_index!=base+i) abort();
            check(base+i);
        }
        if(!eureka576_step() || eureka576_index!=base) abort();
        check(base);
    }
    /* The UI timer must still play within the same clip at stock tempo. */
    fw_transport=1u;
    if(!eureka576_seek(0u)) abort();
    for(unsigned i=1u;i<E576_CLIP_FRAMES;++i) {
        unsigned ticks=0u;
        do {
            eureka576_tick(ctrl);
            if(++ticks>9u) abort();
        }while(eureka576_index!=i);
        check(i);
    }
    for(unsigned i=0u;i<9u && eureka576_index!=0u;++i)
        eureka576_tick(ctrl);
    if(eureka576_index!=0u) abort();
    check(0u);
    /* No note events: random scene cuts must still occur every 2-4 beats.
     * This used to be pure sequential animation in GH76. */
    if(!eureka576_seek(0u)) abort();
    eureka576_next_scene_cut=0u;
    {
        unsigned last_scene=0u, cuts=0u, nonsequential=0u, unique=0u;
        uint32_t scenes_seen=1u;
        for(unsigned k=0u;k<160u;++k) {
            unsigned scene;
            fw_timeline+=E576_STEP_TIME;
            eureka576_tick(ctrl);
            check(eureka576_index);
            scene=eureka576_index/E576_CLIP_FRAMES;
            if(scene!=last_scene) {
                ++cuts;
                if(scene!=(last_scene+1u)%E576_CLIPS) ++nonsequential;
                scenes_seen|=(1u<<scene);
                last_scene=scene;
            }
        }
        for(unsigned bit=0u;bit<E576_CLIPS;++bit)
            unique+=(scenes_seen>>bit)&1u;
        if(cuts<7u || nonsequential<4u || unique<5u) {
            fprintf(stderr,"insufficient true scene randomness: cuts=%u nonseq=%u unique=%u\n",
                    cuts,nonsequential,unique);
            abort();
        }
    }
    /* Each sounding FM voice, including a live note, must be visible. */
    for(unsigned i=0u;i<1200u;++i) {
        uint32_t before=eureka576_index;
        uint32_t dir=eureka576_reverse;
        unsigned was_clip=before/36u;
        fw_timeline+=3u*E576_STEP_TIME;
        eureka576_voice_on(0,0,NULL);
        eureka576_tick(ctrl);
        if(eureka576_event_head!=eureka576_event_tail) abort();
        if(before==eureka576_index) {
            fprintf(stderr,"note caused no visible frame change at %u\n",i);
            abort();
        }
        check(eureka576_index);
        if(dir!=eureka576_reverse) ++reverses;
        if(eureka576_effect==1u) ++speeds;
        if(eureka576_effect==2u) ++stutters;
        if(eureka576_index/36u!=was_clip) ++jumps;
        fw_timeline+=2u*E576_STEP_TIME;
        eureka576_tick(ctrl);
        check(eureka576_index);
        if(!eureka576_enabled) abort();
    }
    if(!speeds || !stutters || !jumps || !reverses) {
        fprintf(stderr,"missing visual effects: jump=%u speed=%u stutter=%u reverse=%u\n",
                jumps,speeds,stutters,reverses);
        abort();
    }
    /* Chords coalesce to one change, and non-FM voices must be ignored. */
    eureka576_voice_on(0,4,NULL);
    if(eureka576_event_head!=eureka576_event_tail) abort();
    fw_timeline+=E576_STEP_TIME;
    for(unsigned i=0u;i<4u;++i)eureka576_voice_on((int32_t)i,0,NULL);
    eureka576_tick(ctrl);
    if(eureka576_event_head!=eureka576_event_tail) abort();
    check(eureka576_index);
    fw_transport=0u;eureka576_tick(ctrl);
    unsigned frozen=eureka576_index;
    for(unsigned i=0u;i<50u;++i)eureka576_tick(ctrl);
    if(frozen!=eureka576_index)abort();
    /* Manual live note must change image with transport stopped. */
    unsigned before_live=eureka576_index;
    eureka576_voice_on(0,0,NULL);
    eureka576_tick(ctrl);
    if(eureka576_index==before_live) abort();
    check(eureka576_index);
    /* MUTE key passthrough only: no audio MUTE is simulated by this test.
     * Never consume any FUNC combination (including FUNC+BANK,
     * FUNC+T1..4, FUNC+YES while MIDI remains down).
     * PTN+TRACK and TRACK keys go to stock OS; EUREKA claims no LEDs. */
    for(int keycode=0;keycode<64;++keycode) {
        /* MIDI+FUNC is intentionally EUREKA-owned in v2.1.
         * It has its own two-press-order tests above.
         * Every other FUNC chord must pass through unchanged. */
        if(keycode==E576_KEY_MIDI) continue;
        key[3]=keycode; key[4]=3; /* down + FUNC */
        if(eureka576_key(NULL,key)!=0 || !eureka576_enabled) abort();
    }
    for(int keycode=42;keycode<46;++keycode) {
        key[3]=keycode; key[4]=1;
        if(eureka576_key(NULL,key)!=0 || !eureka576_enabled) abort();
    }
    key[3]=E576_KEY_YES; key[4]=3;
    if(eureka576_key(NULL,key)!=0 || !eureka576_enabled) abort();

    /* A normal standalone MIDI press must still activate stock MIDI
     * selection; defer the press but dispatch it before its real UP. */
    key[3]=E576_KEY_MIDI;key[4]=1;
    if(eureka576_key(NULL,key)!=1 ||
       eureka576_midi_state!=E576_MIDI_PENDING) abort();
    key[4]=9; /* real MIDI hold */
    if(eureka576_key(NULL,key)!=1 || stock_midi_downs!=0u) abort();
    key[4]=0;
    if(eureka576_key(NULL,key)!=0 || stock_midi_downs!=1u ||
       eureka576_midi_state!=E576_MIDI_IDLE) abort();

    /* MIDI+TRACK must reach stock with MIDI DOWN first, then the
     * regular track press; the input key event is restored afterward. */
    key[3]=E576_KEY_MIDI;key[4]=1;
    if(eureka576_key(NULL,key)!=1) abort();
    key[3]=42;key[4]=1;
    if(eureka576_key(NULL,key)!=0 || stock_midi_downs!=2u ||
       key[3]!=42 || key[4]!=1 ||
       eureka576_midi_state!=E576_MIDI_PASSED) abort();
    key[3]=E576_KEY_MIDI;key[4]=0;
    if(eureka576_key(NULL,key)!=0 ||
       eureka576_midi_state!=E576_MIDI_IDLE) abort();

    /* MIDI+YES again to exit must never select the MIDI mute bank. */
    key[3]=E576_KEY_MIDI;key[4]=1;
    if(eureka576_key(NULL,key)!=1) abort();
    key[3]=E576_KEY_YES;key[4]=1;
    if(eureka576_key(NULL,key)!=1 || eureka576_enabled ||
       stock_midi_downs!=2u) abort();
    key[4]=0;
    if(eureka576_key(NULL,key)!=1) abort();
    key[3]=E576_KEY_MIDI;key[4]=0;
    if(eureka576_key(NULL,key)!=1) abort();
    /* Re-enter video mode for remaining original video CC tests. */
    key[3]=E576_KEY_MIDI;key[4]=1;
    if(eureka576_key(NULL,key)!=1) abort();
    key[3]=E576_KEY_YES;key[4]=1;
    if(eureka576_key(NULL,key)!=1 || !eureka576_enabled ||
       stock_midi_downs!=2u) abort();
    key[4]=0;
    if(eureka576_key(NULL,key)!=1) abort();
    key[3]=E576_KEY_MIDI;key[4]=0;
    if(eureka576_key(NULL,key)!=1) abort();

    /* Video controls only consume AUTO-channel messages and never native
     * CC41/42/43/44 (AMP/FILTER/CHORUS), nor granular's MIDI controls. */
    if(eureka576_cc(0,40,127,0)!=0) abort();
    if(eureka576_cc(0,40,128,CORE_MIDI_CC_AUTO)!=0) abort();
    if(eureka576_cc(0,33,64,CORE_MIDI_CC_AUTO)!=0) abort();
    for(int cc=41;cc<=44;++cc)
        if(eureka576_cc(0,cc,127,CORE_MIDI_CC_AUTO)!=0) abort();
    for(int cc=0;cc<128;++cc) {
        if(eureka576_cc(0,40,cc,CORE_MIDI_CC_AUTO)!=1) abort();
        if(eureka576_cc(0,65,cc,CORE_MIDI_CC_AUTO)!=1) abort();
        if(eureka576_cc(0,67,cc,CORE_MIDI_CC_AUTO)!=1) abort();
        if(eureka576_cc(0,68,cc,CORE_MIDI_CC_AUTO)!=1) abort();
        if(eureka576_cc(0,69,cc,CORE_MIDI_CC_AUTO)!=1) abort();
    }
    for(int cc=0;cc<128;++cc) {
        if(cc==40 || cc==65 || cc==67 || cc==68 || cc==69) continue;
        if(eureka576_cc(0,cc,32,CORE_MIDI_CC_AUTO)!=0) abort();
    }
    if(eureka576_cc(0,40,0,CORE_MIDI_CC_AUTO)!=1)abort();
    if(eureka576_cc(0,65,0,CORE_MIDI_CC_AUTO)!=1)abort();
    if(eureka576_cc(0,67,64,CORE_MIDI_CC_AUTO)!=1)abort();
    if(eureka576_cc(0,68,0,CORE_MIDI_CC_AUTO)!=1)abort();
    if(eureka576_cc(0,69,0,CORE_MIDI_CC_AUTO)!=1)abort();
    eureka576_tick(ctrl);
    if(!eureka576_seek(0u))abort();
    check(0); /* all 5 controls at defaults, exact original 8192 pixels */

    /* Invert 127 means the exact 8192-pixel complement of the original. */
    if(eureka576_cc(0,69,127,CORE_MIDI_CC_AUTO)!=1)abort();
    fw_transport=0u;
    eureka576_tick(ctrl);
    memset(display,0xa5,sizeof(display));
    eureka576_draw(NULL,NULL);
    for(unsigned y=0;y<64u;++y)
        for(unsigned x=0;x<128u;++x) {
            unsigned orig=((unsigned)eureka576_framebuf[
                (7u-(y>>3))+8u*x]>>(y&7u))&1u;
            if((unsigned)display[63u-y][x]!=(orig^1u))abort();
        }
    if(eureka576_cc(0,69,0,CORE_MIDI_CC_AUTO)!=1)abort();
    if(eureka576_cc(0,40,127,CORE_MIDI_CC_AUTO)!=1)abort();
    eureka576_tick(ctrl);
    memset(display,0xa5,sizeof(display));
    eureka576_draw(NULL,NULL);
    unsigned glitch_changed=0u;
    for(unsigned y=0;y<64u;++y)
        for(unsigned x=0;x<128u;++x) {
            unsigned orig=((unsigned)eureka576_framebuf[
                (7u-(y>>3))+8u*x]>>(y&7u))&1u;
            if((unsigned)display[63u-y][x]!=orig)++glitch_changed;
        }
    if(glitch_changed<5u)abort();
    if(eureka576_cc(0,40,0,CORE_MIDI_CC_AUTO)!=1)abort();
    eureka576_tick(ctrl); check(0);

    /* SCRUB 127 is frame 1151, and a second CC changes while stopped. */
    if(eureka576_cc(0,68,127,CORE_MIDI_CC_AUTO)!=1)abort();
    eureka576_tick(ctrl);
    if(eureka576_index!=E576_COUNT-1u)abort();
    check(E576_COUNT-1u);
    if(eureka576_cc(0,68,1,CORE_MIDI_CC_AUTO)!=1)abort();
    eureka576_tick(ctrl);
    if(eureka576_index!=0u)abort();
    check(0u);
    if(eureka576_cc(0,68,0,CORE_MIDI_CC_AUTO)!=1)abort();
    eureka576_tick(ctrl);

    /* Force freeze, then reverse, then an 8-frame CC-driven stutter.
     * Stutter must never seek outside the 36-frame current source clip. */
    if(!eureka576_seek(35u))abort();
    if(eureka576_cc(0,67,0,CORE_MIDI_CC_AUTO)!=1)abort();
    fw_transport=1u;
    uint32_t freeze_index=eureka576_index;
    for(int i=0;i<20;++i)eureka576_tick(ctrl);
    if(eureka576_index!=freeze_index)abort();
    if(eureka576_cc(0,67,112,CORE_MIDI_CC_AUTO)!=1)abort();
    if(!eureka576_step() || eureka576_index!=34u)abort();
    if(eureka576_cc(0,67,64,CORE_MIDI_CC_AUTO)!=1)abort();
    if(!eureka576_seek(35u))abort();
    if(eureka576_cc(0,65,127,CORE_MIDI_CC_AUTO)!=1)abort();
    eureka576_tick(ctrl);
    for(int i=0;i<24;++i) {
        if(!eureka576_step() || eureka576_index>=36u)abort();
    }
    if(eureka576_cc(0,65,0,CORE_MIDI_CC_AUTO)!=1)abort();
    eureka576_tick(ctrl);

    /* A released MIDI key (0 or 0x10) cannot leave a stuck toggle chord. */
    key[3]=E576_KEY_MIDI;key[4]=0;
    if(eureka576_key(NULL,key)!=0)abort();
    key[3]=E576_KEY_YES;key[4]=1;
    if(eureka576_key(NULL,key)!=0 || !eureka576_enabled)abort();
    key[3]=E576_KEY_MIDI;key[4]=1;
    if(eureka576_key(NULL,key)!=1)abort();
    key[3]=13;key[4]=1;
    if(eureka576_key(NULL,key)!=1 || eureka576_enabled ||
       stock_midi_downs!=2u)abort();
    if(!eureka576_yes_owned)abort();
    /* MIDI released before YES: its pending UP must still be consumed. */
    key[3]=E576_KEY_MIDI;key[4]=0;
    if(eureka576_key(NULL,key)!=1)abort();
    key[3]=E576_KEY_YES;key[4]=0x10;
    if(eureka576_key(NULL,key)!=1 || eureka576_yes_owned)abort();
    /* FUNC+YES and free YES presses are left with the stock OS. */
    key[3]=E576_KEY_YES;key[4]=3;
    if(eureka576_key(NULL,key)!=0 || eureka576_enabled)abort();
    key[4]=1;
    if(eureka576_key(NULL,key)!=0 || eureka576_enabled)abort();
    key[4]=0;
    if(eureka576_key(NULL,key)!=0)abort();
    printf("PASS: 1152 frames, 32 random scenes, 1200 note reactions, CC sweeps, isolated MIDI+YES, solo MIDI and MIDI+TRACK stock replay, MUTE keys forwarded; %u rectangles\n",rectangle_count);
    return 0;
}
