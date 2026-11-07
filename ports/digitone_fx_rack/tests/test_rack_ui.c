/* GPL-2.0-or-later: Host regression for 3-FX rack and PG79/PG84 stock keys. */
#include "rack.h"
#include "digitone-mk1/core3.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile int32_t fw_tempo=12000;
const char fw_font5[1]={0},fw_font_label[1]={0},fw_font_title[1]={0};
static unsigned rectangles,prints;
void fw_fillrect(void *bmp,int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t colour)
{(void)bmp;(void)x0;(void)y0;(void)x1;(void)y1;(void)colour;rectangles++;}
void fw_framerect(void *bmp,int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t colour)
{(void)bmp;(void)x0;(void)y0;(void)x1;(void)y1;(void)colour;rectangles++;}
int32_t fw_textf(void *bmp,const void *font,int32_t x,int32_t y,int32_t maxlen,const char *fmt,...)
{(void)bmp;(void)font;(void)x;(void)y;(void)maxlen;(void)fmt;prints++;return 0;}
static void event(int32_t *ev,int key,int flags)
{memset(ev,0,6*sizeof(int32_t));ev[3]=key;ev[4]=flags;}
extern struct core_audio_owner rack_owner;
extern volatile uint32_t rack_test_cycles,rack_test_render_cycles;
extern const struct core_menu_item rack_menu_granular,rack_menu_reel,rack_menu_crush,rack_menu_off;
static void tick5(uint8_t *ctrl) {for(int k=0;k<5;k++)rack_tick(ctrl);}
int main(void)
{
    int32_t ev[6],dry[64],wet[64];
    uint8_t ctrl[64]={0};
    for(unsigned i=0;i<64u;i++) dry[i]=1234*65536+123;
    rack_tick(ctrl);
    assert(rack_active==-1 && rack_owner.on==0);
    assert(DNFX_COUNT==3 && DNFX_REEL==1 && DNFX_CRUSH==2);
    assert(dnfx_page_count(DNFX_GRANULAR)==1u);
    assert(dnfx_page_count(DNFX_REEL)==1u);
    assert(dnfx_page_count(DNFX_CRUSH)==1u);
    rack_choose(DNFX_REEL);
    tick5(ctrl);
    assert(rack_active==DNFX_REEL && rack_owner.on==1);
    event(ev,26,1); assert(rack_key(0,ev)==0);
    event(ev,41,1); assert(rack_key(0,ev)==0);
    event(ev,42,9); assert(rack_key(0,ev)==0);
    event(ev,13,3); assert(rack_key(0,ev)==0);
    event(ev,13,1); assert(rack_key(0,ev)==0);
    event(ev,14,1); assert(rack_key(0,ev)==1);
    event(ev,14,16); assert(rack_key(0,ev)==1);
    rack_choose(DNFX_REEL);
    assert(rack_owner.on==1 && rack_active==DNFX_REEL);
    event(ev,19,1); assert(rack_key(0,ev)==0); /* no extra FX page */
    rack_choose(DNFX_CRUSH);
    for(int k=0;k<50;k++)rack_render(wet,dry);
    tick5(ctrl);
    assert(rack_active==DNFX_CRUSH && rack_owner.on);
    event(ev,46,1); assert(rack_key(0,ev)==1);
    event(ev,46,16); assert(rack_key(0,ev)==1);
    rack_choose(-1);
    for(int k=0;k<100;k++)rack_render(wet,dry);
    tick5(ctrl);
    assert(rack_active==-1 && !rack_owner.on);
    rack_choose(DNFX_REEL);
    tick5(ctrl);
    assert(rack_active==DNFX_REEL && rack_owner.on);
    rack_draw(wet,ctrl);
    assert(rectangles>0&&prints>0);
    assert(rack_menu_granular.open&&rack_menu_reel.open);
    assert(rack_menu_crush.open&&rack_menu_off.open);
    /* Simulated slow REEL render: owner must shut off, no auto-restart. */
    rack_test_cycles=500000u;
    rack_test_render_cycles=100000u;
    for(int k=0;k<15 && rack_owner.on;k++) {
        rack_test_cycles+=150000u;
        rack_render(wet,dry);
    }
    assert(rack_owner.on==0);
    assert(rack_overload_mode==(uint8_t)(DNFX_REEL+1));
    rack_tick(ctrl);
    assert(rack_owner.on==0);
    rack_test_render_cycles=0u;
    rack_choose(DNFX_REEL);
    tick5(ctrl);
    assert(rack_owner.on==1 && rack_overload_mode==0u);
    puts("PASS: 3-FX menu, MUTE/TRIG/YEs stock, 1-page UI, REEL CPU guard");
    return 0;
}
