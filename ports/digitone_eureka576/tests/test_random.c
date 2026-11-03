/* Native model tests; no Digitone firmware or sound engine is modified. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
volatile uint32_t fw_transport=0u, fw_timeline=0u;
volatile int32_t fw_tempo=14400;
static uint8_t display[64][128];
static unsigned rectangle_count;
void fw_fillrect(void *bmp,int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t c)
{
    (void)bmp;
    if(x0<0 || y0<0 || x1>127 || y1>63 || x0>x1 || y0>y1 || c<0 || c>1) abort();
    ++rectangle_count;
    for(int32_t y=y0;y<=y1;++y)
        for(int32_t x=x0;x<=x1;++x) display[y][x]=(uint8_t)c;
}
#include "../eureka576.c"
extern const uint8_t eureka576_reference[],eureka576_reference_end[];
static void check(unsigned index)
{
    if(index>=576u ||
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
    if((uintptr_t)eureka576_reference_end-(uintptr_t)eureka576_reference!=589824u)
        abort();
    key[3]=2;key[4]=1;
    if(eureka576_key(NULL,key)!=0) abort();
    key[3]=13;
    if(eureka576_key(NULL,key)!=1 || !eureka576_enabled) abort();
    check(0);
    /* Exhaustive all-frame random seek and 4,718,592 display pixel comparisons. */
    for(unsigned i=0u;i<576u;++i) {
        if(!eureka576_seek(i)) abort();
        check(i);
    }
    if(eureka576_seek(576u)) abort();
    fw_transport=1u;
    if(!eureka576_seek(0u)) abort();
    for(unsigned i=1u;i<576u;++i) {
        unsigned ticks=0;
        do {
            eureka576_tick(ctrl);
            if(++ticks>9u) abort();
        }while(eureka576_index!=i);
        check(i);
    }
    for(unsigned i=0u;i<9u && eureka576_index!=0u;++i)eureka576_tick(ctrl);
    if(eureka576_index!=0u) abort();
    check(0);
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
    key[3]=13;key[4]=1;
    if(eureka576_key(NULL,key)!=1 || eureka576_enabled)abort();
    printf("PASS: 576/576 seek + pixels, playback/wrap, 1200 note-on reactions, JUMP/REVERSE/SPEED/STUTTER, live/stop/toggle; %u rectangles\n",rectangle_count);
    return 0;
}
