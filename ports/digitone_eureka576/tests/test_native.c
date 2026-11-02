/* Host-only test: verifies the runtime code without creating or flashing a .syx. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t canvas[64][128];
static unsigned rectangle_count;
void fw_fillrect(void *bmp, int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t colour)
{
    (void)bmp;
    if (x0<0 || y0<0 || x1>127 || y1>63 || x1<x0 || y1<y0 ||
        (colour!=0 && colour!=1)) abort();
    ++rectangle_count;
    for(int y=y0;y<=y1;++y)
        for(int x=x0;x<=x1;++x)
            canvas[y][x]=(uint8_t)colour;
}
#include "../eureka576.c"
extern const uint8_t eureka576_reference[];
extern const uint8_t eureka576_reference_end[];
static void check_display(unsigned index)
{
    memset(canvas,0xa5,sizeof(canvas));
    eureka576_draw(NULL,NULL);
    for(unsigned y=0;y<64;y++)
        for(unsigned x=0;x<128;x++) {
            unsigned bit=((unsigned)eureka576_reference[index*1024+
                         (7u-(y>>3))+8u*x]>>(y&7u))&1u;
            if(canvas[63-y][x]!=bit) {
                fprintf(stderr,"pixel mismatch: frame=%u x=%u y=%u\n",index,x,y);
                abort();
            }
        }
}
static void check_frame(unsigned index)
{
    if(memcmp(eureka576_framebuf,eureka576_reference+index*1024u,1024)!=0){
        fprintf(stderr,"decoder mismatch at frame %u\n",index);
        abort();
    }
    if(index==0 || index==35 || index==36 || index==143 ||
       index==144 || index==395 || index==396 || index==575)
        check_display(index);
}
int main(void)
{
    uint8_t ctrl[128]={0};
    int32_t event[6]={0};
    if((size_t)((uintptr_t)eureka576_reference_end-(uintptr_t)eureka576_reference)!=576u*1024u)
        abort();
    event[3]=2; event[4]=1; /* MIDI key press */
    if(eureka576_key(NULL,event)!=0)abort();
    event[3]=13; event[4]=1; /* YES key press */
    if(eureka576_key(NULL,event)!=1 || !eureka576_enabled ||
       eureka576_index!=0)abort();
    check_frame(0);
    for(unsigned i=1;i<576;i++){
        unsigned ticks=0;
        do {
            eureka576_tick(ctrl);
            ++ticks;
        } while(eureka576_index!=i && ticks<10);
        if(eureka576_index!=i || ticks>7 || !eureka576_enabled ||
           !ctrl[0x20])abort();
        check_frame(i);
        ctrl[0x20]=0;
    }
    for(unsigned i=0;i<8 && eureka576_index!=0;i++)
        eureka576_tick(ctrl);
    if(eureka576_index!=0 || !eureka576_enabled)abort();
    check_frame(0);
    if(eureka576_readpos==0u ||
       eureka576_readpos>(uint32_t)((uintptr_t)eureka576_frames_end-(uintptr_t)eureka576_frames)) abort();
    event[3]=13; event[4]=1;
    if(eureka576_key(NULL,event)!=1 || eureka576_enabled)abort();
    ctrl[0x20]=0; eureka576_tick(ctrl);
    if(!ctrl[0x20])abort();
    event[3]=2; event[4]=16; /* MIDI key release */
    eureka576_key(NULL,event);
    event[3]=13; event[4]=1; /* standalone YES must not reactivate */
    if(eureka576_key(NULL,event)!=0 || eureka576_enabled)abort();
    printf("PASS: all 576 frames, 16 clip boundaries, drawing, wrap, key toggle, redraw (%u rectangles)\n",rectangle_count);
    return 0;
}
