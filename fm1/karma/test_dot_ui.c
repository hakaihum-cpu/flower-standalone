/* SPDX-License-Identifier: MIT
 * Host-only geometry smoke test for the native MIYAKO 240x124 DOT canvas.
 * Bounds are stricter than gfx.c (which clips), so regressions fail early.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define P_E0 0
#define P_E1 1
#define P_E4 4
#define P_E5 5
#define P_E6 6
#define P_E7 7
#define RGB(r,g,b) ((uint16_t)((((r)>>3)<<11)|(((g)>>2)<<5)|((b)>>3)))
#define C_BLACK 0u
#define C_WHITE 0xFFFFu
typedef struct { uint8_t h; } test_font_t;
static const test_font_t FONT_S={16};
typedef struct { int16_t p[8]; int32_t peak; } track_t;
static struct { uint32_t frame; } ui;
static int32_t cv_oy;
static uint32_t fm1_ms;
static unsigned long draws, oob;
static int32_t clamp(int32_t v,int32_t a,int32_t b){return v<a?a:v>b?b:v;}
static int32_t sine_i(uint32_t p){return (int32_t)(32767.0*sin((double)p*(6.283185307179586/4294967296.0)));}
static void point(int32_t x,int32_t y){
    draws++; if(x<0||y<0||x>=240||y>=124)oob++;
}
static void cv_pset(int32_t x,int32_t y,uint16_t c){(void)c;point(x,y+cv_oy);}
static void cv_rect(int32_t x,int32_t y,int32_t w,int32_t h,uint16_t c){
    (void)c;draws++;
    if(x<0||y<0||w<0||h<0||x+w>240||y+h>124)oob++;
}
static void cv_line(int32_t x0,int32_t y0,int32_t x1,int32_t y1,uint16_t c){
    (void)c;point(x0,y0+cv_oy);point(x1,y1+cv_oy);
}
static int32_t cv_text(int32_t x,int32_t y,const test_font_t*f,const char*s,uint16_t c){
    (void)c;draws++;
    if(x<0||y<0||x+(int32_t)strlen(s)*8>240||y+f->h>124)oob++;
    return x+(int32_t)strlen(s)*8;
}
#include "mi_dot_ui.c"
int main(void) {
    track_t t={0}; int wave,mode,d,e,frame; unsigned scenes=0;
    for(wave=0;wave<4;wave++) for(frame=0;frame<2;frame++) {
        t.p[P_E0]=wave; t.p[P_E1]=frame?127:0;
        t.peak=frame?100:0; ui.frame=(unsigned)(frame*71u);
        mi_dot_graph(&t,0); scenes++;
    }
    for(mode=0;mode<3;mode++)for(d=0;d<2;d++)for(e=0;e<2;e++) {
        t.p[P_E4]=mode;t.p[P_E5]=d?127:0;
        t.p[P_E6]=e?127:0;t.p[P_E7]=e?127:0;
        t.peak=e?100:0;ui.frame=70u;
        mi_dot_graph(&t,1); scenes++;
    }
    /* Every one of the 576 supplied frames must decode in the C runtime.
     * Verify sequential playback, random seeks, and clip keyframe resets. */
    {
        unsigned f;
        uint32_t checksum = 0u;
        for(f=0;f<MI_EUREKA_ASSET_COUNT;f++) {
            unsigned k;
            if(!mi_ev_seek(f)) {
                fprintf(stderr,"FAIL: EUREKA decoder frame %u\n",f);
                return 1;
            }
            for(k=0;k<MI_EUREKA_ASSET_BYTES;k++)
                checksum = (checksum * 33u) ^ mi_ev_pixels[k];
        }
        if(!mi_ev_seek(0u) || !mi_ev_seek(575u) || checksum==0u) {
            fprintf(stderr,"FAIL: EUREKA random seek/integrity\n");
            return 1;
        }
        /* Original MIYAKO does NOT loop on a held input level.
         * Meaningful amplitude changes advance 1..4 frames or change clip,
         * while silence and a steady level freeze the current real frame. */
        t.p[P_E4]=2;
        mi_ev_enabled=0u; mi_ev_clip=-1; mi_ev_prior=0u;
        { unsigned changed=0, switched=0; uint32_t prev=0u;
          for(f=0;f<128u;f++) {
              t.peak=(int32_t)(1500u+(f%4u)*3200u);
              ui.frame+=3u;
              mi_dot_graph(&t,1);
              if(f && mi_ev_visible!=prev) changed++;
              if(f && mi_ev_visible/36u!=prev/36u) switched++;
              prev=mi_ev_visible;
          }
          if(changed<16u || switched==0u) {
              fprintf(stderr,"FAIL: EUREKA original audio dynamics/clip switch (changed=%u switched=%u)\n",changed,switched);
              return 1;
          }
        }
        /* First sample after activity drop may change the frame, then stop. */
        t.peak=0; mi_dot_graph(&t,1);
        { uint32_t frozen=mi_ev_visible;
          for(f=0;f<16u;f++) {
              t.peak=0; ui.frame+=4u; mi_dot_graph(&t,1);
              if(mi_ev_visible!=frozen) {
                  fprintf(stderr,"FAIL: EUREKA auto-plays in silence\n");
                  return 1;
              }
          }
        }
        t.peak=8000; mi_dot_graph(&t,1);
        { uint32_t frozen=mi_ev_visible;
          for(f=0;f<16u;f++) {
              t.peak=8000; ui.frame+=3u; mi_dot_graph(&t,1);
              if(mi_ev_visible!=frozen) {
                  fprintf(stderr,"FAIL: EUREKA auto-plays on steady held level\n");
                  return 1;
              }
          }
        }
        printf("PASS: EUREKA 576/576 frames C decode, random seeks, audio-dynamic/random-clip and idle/held freeze\n");
    }
    if(oob||draws<1000){fprintf(stderr,"FAIL: scenes=%u draws=%lu oob=%lu\n",scenes,draws,oob);return 1;}
    printf("PASS: %u MIYAKO DOT scenes, %lu bounded draw operations\n",scenes,draws);
    return 0;
}
