/* SPDX-License-Identifier: MIT
 * Host-only MIYAKO boot UI geometry and skip-control test.
 * No LCD, firmware flash, or generated images.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define RGB(r,g,b) ((uint16_t)((((r)>>3)<<11)|(((g)>>2)<<5)|((b)>>3)))
#define C_BLACK 0u
#define C_WHITE 0xFFFFu
typedef struct {uint8_t h; uint8_t glyph_w;} font_t;
static const font_t FONT_S = {16,8}, FONT_L={32,16};
static uint32_t draw_count, illegal;
static int32_t cv_oy;
static uint32_t width, height, displayed;
static struct {uint32_t buttons,notes;} fm1_in;
static struct {int force;} ui;
static uint16_t boot_t=0xFFFFu;
#define BOOT_END 124u
static int32_t clamp(int32_t v,int32_t lo,int32_t hi){return v<lo?lo:v>hi?hi:v;}
static uint16_t mix565(uint16_t a,uint16_t b,int32_t fade){(void)b;(void)fade;return a;}
static int32_t text_w(const font_t*f,const char*s){return (int32_t)strlen(s)*f->glyph_w;}
static void lcd_fill(int32_t x,int32_t y,int32_t w,int32_t h,uint16_t c){
 (void)c;draw_count++;
 if(x<0||y<0||w<0||h<0||x+w>240||y+h>240)illegal++;
}
static void cv_begin(uint32_t w,uint32_t h,uint16_t c){
 (void)c;width=w;height=h;
 if(w>240||h>124)illegal++;
}
static void cv_rect(int32_t x,int32_t y,int32_t w,int32_t h,uint16_t c){
 (void)c;draw_count++;
 /* cv_oy clips at pass edges, but the absolute image positions must fit */
 if(x<0||w<0||x+w>240||y<0||h<0||y+h>240)illegal++;
}
static int32_t cv_text(int32_t x,int32_t y,const font_t*f,const char*s,uint16_t c){
 (void)c;draw_count++;
 if(x<0||x+text_w(f,s)>240||y<0||y+f->h>240)illegal++;
 return x+text_w(f,s);
}
static void cv_blit(int32_t x,int32_t y){
 draw_count++; displayed++;
 if(x<0||y<0||x+(int32_t)width>240||y+(int32_t)height>240)illegal++;
}
#include "mi_splash.c"
#include "mi_boot_anim.c"
int main(void){
 uint32_t f;int rc=0;
 sloop_splash();
 if(illegal){fprintf(stderr,"FAIL splash geometry: %u\n",illegal);return 1;}
 boot_t=0;ui.force=0;
 for(f=0;f<BOOT_END+2u;f++)rc=boot_draw();
 if(rc||illegal||draw_count<1000){fprintf(stderr,"FAIL animation: rc=%d illegal=%u draws=%u\n",rc,illegal,draw_count);return 1;}
 boot_t=1;fm1_in.notes=1;
 rc=boot_draw();
 if(rc||!ui.force){fprintf(stderr,"FAIL boot skip behavior\n");return 1;}
 printf("PASS: MIYAKO boot/splash %u draw ops, %u blits, safe bounds, skip button\n",draw_count,displayed);
 return 0;
}
