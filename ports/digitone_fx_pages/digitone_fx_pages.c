/* SPDX-License-Identifier: GPL-2.0-or-later
 * Unique native parameter pages for GRANULAR, EUREKA audio, REEL and CRUSH.
 * Rendering follows the real Digitone layout: top tempo/title band, a
 * narrow left gutter and A-D / E-H in two rows of four on the 128x64 OLED.
 * Geometry informed by elekloader DigiPrism's GPL-2.0-or-later ui.c.
 * Keep these sources separate from the approved PG79 audio and video hooks.
 */
#include "digitone_fx_pages.h"
#include "digitone-mk1/core3.h"
#include <stdint.h>

#define K(name,lo,hi,def) {name,lo,hi,def,DNFX_DIAL}
#define S(name,lo,hi,def) {name,lo,hi,def,DNFX_SELECT}
#define U {0,0,0,0,DNFX_DIAL}
static const struct dnfx_control panels[DNFX_COUNT][2][8] = {
    { /* GRANULAR 0.5: the FIVE real CC-backed parameters, no invented controls */
        {S("PITA",0,7,4), S("PITB",0,7,6), K("SIZE",0,48,24),
         K("POS",0,127,0), K("MIX",0,127,64), U,U,U},
        {U,U,U,U,U,U,U,U}
    },
    { /* EUREKA: match HazeProcessor::Params, excluding global bypass */
        {K("MIX",0,127,70),K("TIME",0,127,64),K("HAZE",0,127,64),
         K("FILT",0,127,64),K("REPT",0,127,0),K("MOD",0,127,64),
         K("VERB",0,127,0),S("SPED",0,2,1)},
        {S("LOOP",0,2,1),S("WARB",0,2,0),S("TRNS",0,1,0),
         S("ECHO",0,1,0),S("OG",0,1,0),S("LOCK",0,1,0),
         S("GAIN",0,1,0),S("STEO",0,1,1)}
    },
    { /* MIYAKO Master REEL: exact five source parameters */
        {K("DRIV",0,127,38),K("WOW",0,127,25),
         K("FLUT",0,127,13),K("NOIS",0,127,0),
         K("WET",0,127,0),U,U,U},
        {U,U,U,U,U,U,U,U}
    },
    { /* Proposed independent bitcrusher, four true DSP controls */
        {K("BITS",4,16,12),K("RATE",1,32,1),
         K("MIX",0,127,64),K("OUT",0,127,100),U,U,U,U},
        {U,U,U,U,U,U,U,U}
    }
};
#undef K
#undef S
#undef U

static const char *const titles[DNFX_COUNT] = {
    "GRANULAR", "EUREKA", "REEL", "BITCRUSH"
};
static const char *const eureka_subtitle[2]={"SOUND", "PLAY"};
static const int8_t tip[29][2] = {
    {-4,-4},{-5,-3},{-5,-3},{-6,-2},{-6,-1},{-6,0},{-6,1},{-6,2},
    {-5,3},{-4,4},{-4,5},{-3,5},{-2,6},{-1,6},{0,6},
    {1,6},{2,6},{3,5},{4,5},{4,4},{5,3},{6,2},{6,1},{6,0},
    {6,-1},{6,-2},{5,-3},{5,-3},{4,-4}
};
static const uint8_t xcol[4]={33u,59u,86u,112u};
static const int8_t pitch_st[8]={-12,-7,-5,0,5,7,12,24};

unsigned dnfx_page_count(unsigned effect)
{
    return effect==DNFX_EUREKA ? 2u : effect<DNFX_COUNT ? 1u : 0u;
}
const struct dnfx_control *dnfx_control_at(unsigned effect,unsigned page,unsigned knob)
{
    if(effect>=DNFX_COUNT || page>=dnfx_page_count(effect) || knob>=8u)
        return 0;
    return panels[effect][page][knob].label ? &panels[effect][page][knob] : 0;
}
void dnfx_defaults(struct dnfx_state *s,unsigned effect)
{
    unsigned pg,i;
    if(!s || effect>=DNFX_COUNT) return;
    s->effect=(uint8_t)effect;
    s->page=0u; s->focus=0u; s->flash=0u;
    for(pg=0;pg<2u;++pg)
        for(i=0;i<8u;++i)
            s->value[pg][i]=panels[effect][pg][i].initial;
}
int dnfx_set_page(struct dnfx_state *s,unsigned page)
{
    if(!s || page>=dnfx_page_count(s->effect)) return 0;
    if(s->page==page) return 0;
    s->page=(uint8_t)page; s->flash=0u;
    return 1;
}
int dnfx_edit(struct dnfx_state *s,unsigned knob,int detents)
{
    const struct dnfx_control *p;
    int next;
    if(!s || !detents) return 0;
    p=dnfx_control_at(s->effect,s->page,knob);
    if(!p) return 0; /* A missing label is genuinely inactive */
    next=(int)s->value[s->page][knob]+detents;
    if(next<(int)p->minimum) next=p->minimum;
    if(next>(int)p->maximum) next=p->maximum;
    if(next==s->value[s->page][knob]) return 0;
    s->value[s->page][knob]=(uint8_t)next;
    s->focus=(uint8_t)knob; s->flash=50u;
    return 1;
}
void dnfx_tick(struct dnfx_state *s)
{
    if(s && s->flash) --s->flash;
}
static void dot(void *bmp,int x,int y)
{
    fw_fillrect(bmp,x,y,x,y,1);
}
static void stroke(void *bmp,int x0,int y0,int x1,int y1)
{
    int dx=x1>x0?x1-x0:x0-x1, sx=x0<x1?1:-1;
    int dy=-(y1>y0?y1-y0:y0-y1), sy=y0<y1?1:-1;
    int err=dx+dy;
    for(;;) {
        int e2;
        dot(bmp,x0,y0);
        if(x0==x1 && y0==y1) return;
        e2=2*err;
        if(e2>=dy) {err+=dy;x0+=sx;}
        if(e2<=dx) {err+=dx;y0+=sy;}
    }
}
static void dial(void *bmp,int cx,int cy,int norm)
{
    int i,k=(norm*28+63)/127;
    /* 17x17 stock-like ring, not the rectangular PG79 value panels. */
    fw_fillrect(bmp,cx-2,cy+8,cx+2,cy+8,1);
    fw_fillrect(bmp,cx-2,cy-8,cx+2,cy-8,1);
    fw_fillrect(bmp,cx-8,cy-2,cx-8,cy+2,1);
    fw_fillrect(bmp,cx+8,cy-2,cx+8,cy+2,1);
    for(i=-1;i<=1;i+=2) {
        fw_fillrect(bmp,cx+3*i,cy+7,cx+4*i,cy+7,1);
        fw_fillrect(bmp,cx+3*i,cy-7,cx+4*i,cy-7,1);
        fw_fillrect(bmp,cx+7*i,cy+3,cx+7*i,cy+4,1);
        fw_fillrect(bmp,cx+7*i,cy-4,cx+7*i,cy-3,1);
        dot(bmp,cx+5*i,cy+6);dot(bmp,cx+5*i,cy-6);
        dot(bmp,cx+6*i,cy+5);dot(bmp,cx+6*i,cy-5);
    }
    stroke(bmp,cx,cy,cx+tip[k][0],cy+tip[k][1]);
}
static unsigned letters(const char *s)
{
    unsigned n=0u;while(s[n]) ++n;return n;
}
static void label(void *bmp,int cx,int y,const char *s)
{
    int w=(int)letters(s)*4;
    fw_textf(bmp,(const void *)fw_font_label,cx-(w/2),y,-1,"%s",s);
}
static void print_value(void *bmp,int x,int y,const struct dnfx_state *s,unsigned i)
{
    unsigned fx=s->effect,pg=s->page;
    int v=s->value[pg][i];
    const void *font=(const void *)fw_font5;
    if(fx==DNFX_GRANULAR) {
        if(i<2u) {
            int pitch=pitch_st[v>7?7:v];
            fw_textf(bmp,font,x,y,-1,"%+dST",pitch);
        } else if(i==2u) {
            fw_textf(bmp,font,x,y,-1,"%dMS",120+5*v);
        } else if(i==3u) {
            int tenth=(v*625+63)/127;
            fw_textf(bmp,font,x,y,-1,"%d.%dMS",tenth/10,tenth%10);
        } else fw_textf(bmp,font,x,y,-1,"%d%%",(v*100+63)/127);
        return;
    }
    if(fx==DNFX_EUREKA) {
        if(pg==0u && i==7u) {
            static const char *const speed[]={"0.5X","1X","2X"};
            fw_textf(bmp,font,x,y,-1,"%s",speed[v>2?2:v]);return;
        }
        if(pg==1u) {
            if(i==0u) {
                static const char *const loops[]={"1","2","2+"};
                fw_textf(bmp,font,x,y,-1,"%s",loops[v>2?2:v]);
            } else if(i==1u) {
                static const char *const warble[]={"OFF","LITE","HEAV"};
                fw_textf(bmp,font,x,y,-1,"%s",warble[v>2?2:v]);
            } else if(i==7u) {
                fw_textf(bmp,font,x,y,-1,"%s",v?"ST":"MO");
            } else fw_textf(bmp,font,x,y,-1,"%s",v?"ON":"OFF");
            return;
        }
    }
    if(fx==DNFX_CRUSH) {
        if(i==0u) fw_textf(bmp,font,x,y,-1,"%dBIT",v);
        else if(i==1u) fw_textf(bmp,font,x,y,-1,"%dX",v);
        else fw_textf(bmp,font,x,y,-1,"%d%%",(v*100+63)/127);
        return;
    }
    fw_textf(bmp,font,x,y,-1,"%d%%",(v*100+63)/127);
}
void dnfx_draw(void *bmp,const struct dnfx_state *s,unsigned bpm)
{
    unsigned i;
    if(!bmp || !s || s->effect>=DNFX_COUNT ||
       s->page>=dnfx_page_count(s->effect)) return;
    fw_fillrect(bmp,0,0,127,63,0);
    fw_textf(bmp,(const void *)fw_font_title,2,56,-1,"%d",bpm);
    fw_fillrect(bmp,0,54,17,63,-1); /* inverted tempo box */
    fw_fillrect(bmp,20,0,20,52,1);  /* left channel gutter */
    fw_textf(bmp,(const void *)fw_font5,3,39,-1,"FX");
    fw_textf(bmp,(const void *)fw_font5,3,18,-1,"%d",s->page+1u);
    if(s->flash && dnfx_control_at(s->effect,s->page,s->focus)) {
        const struct dnfx_control *p=dnfx_control_at(s->effect,s->page,s->focus);
        fw_textf(bmp,(const void *)fw_font_title,22,56,-1,"%s",p->label);
        print_value(bmp,70,56,s,s->focus);
    } else {
        fw_textf(bmp,(const void *)fw_font_title,22,56,-1,"%s",titles[s->effect]);
        if(s->effect==DNFX_EUREKA)
            fw_textf(bmp,(const void *)fw_font5,91,56,-1,"%s",eureka_subtitle[s->page]);
    }
    for(i=0u;i<8u;++i) {
        const struct dnfx_control *p=dnfx_control_at(s->effect,s->page,i);
        int cx=xcol[i&3u],cy=i<4u?42:15;
        int v,norm;
        if(!p) continue;
        v=s->value[s->page][i];
        norm=((v-(int)p->minimum)*127)/((int)p->maximum-(int)p->minimum);
        if(p->style==DNFX_DIAL) {
            dial(bmp,cx,cy,norm);
        } else {
            fw_framerect(bmp,cx-8,cy-8,cx+8,cy+8,1);
            fw_textf(bmp,(const void *)fw_font5,cx-4,cy-2,-1,"%d",v);
        }
        label(bmp,cx,i<4u?27:0,p->label);
    }
}
