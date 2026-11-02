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
    if(index>=343u ||
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
    int32_t key[6]={0},note[20]={0};
    unsigned speeds=0,stutters=0,jumps=0;
    if((uintptr_t)eureka576_reference_end-(uintptr_t)eureka576_reference!=351232u)
        abort();
    key[3]=2;key[4]=1;
    if(eureka576_key(NULL,key)!=0) abort();
    key[3]=13;
    if(eureka576_key(NULL,key)!=1 || !eureka576_enabled) abort();
    check(0);
    /* Exhaustive all-frame random seek and 2,809,856 display pixel comparisons. */
    for(unsigned i=0u;i<343u;++i) {
        if(!eureka576_seek(i)) abort();
        check(i);
    }
    if(eureka576_seek(343u)) abort();
    fw_transport=1u;
    if(!eureka576_seek(0u)) abort();
    for(unsigned i=1u;i<343u;++i) {
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
    note[0]=0;note[2]=0;note[10]=FW_EVENT_NOTE_TRIG;
    for(unsigned i=0u;i<1200u;++i) {
        uint32_t at=fw_timeline+E576_STEP_TIME;
        uint32_t before=eureka576_index;
        eureka576_note_queue(note,at);
        fw_timeline=at-1u;
        eureka576_tick(ctrl);
        if(eureka576_event_head==eureka576_event_tail)abort();
        fw_timeline=at;
        eureka576_tick(ctrl);
        if(eureka576_event_head!=eureka576_event_tail)abort();
        check(eureka576_index);
        if(eureka576_effect==1u)++speeds;
        if(eureka576_effect==2u)++stutters;
        if(eureka576_index!=before && eureka576_effect==0u)++jumps;
        fw_timeline+=2u*E576_STEP_TIME;
        eureka576_tick(ctrl);
        check(eureka576_index);
        if(!eureka576_enabled) abort();
    }
    if(!speeds || !stutters || !jumps) {
        fprintf(stderr,"not all effects fired: %u/%u/%u\n",jumps,speeds,stutters);
        abort();
    }
    fw_transport=0u;eureka576_tick(ctrl);
    unsigned frozen=eureka576_index;
    for(unsigned i=0u;i<50u;++i)eureka576_tick(ctrl);
    if(frozen!=eureka576_index)abort();
    key[3]=13;key[4]=1;
    if(eureka576_key(NULL,key)!=1 || eureka576_enabled)abort();
    printf("PASS: 343/343 seek + pixels, playback/wrap, 1200 timed triggers, JUMP/SPEED/STUTTER, stop/toggle; %u rectangles\n",rectangle_count);
    return 0;
}
