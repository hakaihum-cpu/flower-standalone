/* SPDX-License-Identifier: GPL-3.0-only
 * Independent MIYAKO FM-1 app using only fm1-base's hardware platform API.
 * This is an INCOMPLETE port. The original EFFECTS animation frames are
 * converted at build time (16-colour lossy VQ). EUREKA is excluded at the
 * user's request. Full original MIYAKO DSP is not yet present.
 * Do not release this development image as the approved MIYAKO firmware.
 *
 * Compiled inside fm1-base's unity.c after gfx.c and lib/touch.c.
 * mi_control.c and mi_audio.c are separate -O2 units (fm1.toml).
 */
#include "mi_control.h"
#include "mi_audio.h"
#include "mi_video.h"
#include "mi_effects_vq_frames.h" /* generated: ALL 144 user-source frames */
#include "mi_memory.h"
#define YELLOW RGB(255,209,12)
#define DIM RGB(129,106,39)
#define GREY RGB(218,213,202)
#define SAVED_MAGIC 0x4D493132u /* MI12: effects enum changed when EUREKA was deferred */
typedef struct {
 uint32_t magic;
 mi_state_t state;
} saved_t;
static mi_state_t state;
static mi_audio_t audio;
static mi_video_t video;
static volatile float audio_activity;
static uint32_t last_keys,last_buttons;
static uint8_t held_note[NKEYS];
static int redraw;
static char line_a[24],line_b[24];
static uint32_t message_until;

static void textcpy(char *dest,const char *src,unsigned limit)
{
 unsigned i=0;if(!src)src="";
 for(;i+1<limit&&src[i];i++)dest[i]=src[i];
 dest[i]=0;
}
static void number(int32_t value,char *out)
{
 char tmp[16];unsigned n=0;uint32_t v;
 if(value<0){*out++='-';v=(uint32_t)(-value);}else v=(uint32_t)value;
 do{tmp[n++]=(char)('0'+v%10u);v/=10u;}while(v&&n<15u);
 while(n)*out++=tmp[--n];
 *out=0;
}
static int valid_state(const mi_state_t *s)
{
 return (unsigned)s->view<MI_VIEW_COUNT&&s->wave<=3u&&
        s->fx<MI_FX_COUNT&&s->width<=127u&&s->sub<=127u&&
        s->crush<=127u&&s->level<=127u&&s->wet<=127u&&
        s->project_slot>=1u&&s->project_slot<=4u&&s->preset<=5u&&
        s->octave>=-3&&s->octave<=3;
}
void app_init(void)
{
 saved_t saved;
 mi_init(&state);
 mi_audio_init(&audio,44100.f);
 mi_video_init(&video,MI_EFFECTS_ASSET_COUNT);
 if(plat_store_load(OBJ_SETTINGS,&saved,sizeof saved)==(int)sizeof saved&&
    saved.magic==SAVED_MAGIC&&valid_state(&saved.state)){
     state=saved.state;
     state.save_armed=0;
     state.playing=state.recording=0;
     state.view=MI_HOME;
 }
 last_keys=last_buttons=0;mi_memzero(held_note,sizeof held_note);redraw=1;
 message_until=0;line_a[0]=line_b[0]=0;
}
void app_panic(void){mi_audio_panic(&audio);}
/* Read MIDI on the audio callback, as fm1-base's app_render contract expects.
 * This handles USB MIDI and the FM-1's TRS MIDI IN; radio support can be
 * enabled independently in the platform configuration later.
 */
static void midi_audio_poll(void)
{
 uint32_t pkt;
 int port;
 while(plat_midi_in(&pkt,&port)){
     uint8_t status=(uint8_t)(pkt>>8);
     uint8_t note=(uint8_t)((pkt>>16)&127u);
     uint8_t velocity=(uint8_t)((pkt>>24)&127u);
     uint8_t kind=status&0xF0u;
     (void)port;
     if(kind==0x90u&&velocity)mi_audio_input_on(&audio,&state,note,velocity);
     else if(kind==0x80u||(kind==0x90u&&!velocity))
         mi_audio_input_off(&audio,&state,note);
     else if(kind==0xB0u&&note==123u)
         mi_audio_panic(&audio);
 }
}
void app_render(int32_t *lr,uint32_t n,uint32_t master_q12)
{
 uint32_t i;
 int32_t peak=0;
 midi_audio_poll();
 mi_audio_render(&audio,&state,lr,n,master_q12);
 for(i=0;i<n;i++){
     int32_t sample=lr[2u*i];
     if(sample<0)sample=-sample;
     if(sample>peak)peak=sample;
 }
 audio_activity=(float)peak/8388607.f;
}
void app_say(const char *a,const char *b)
{
 textcpy(line_a,a,sizeof line_a);
 textcpy(line_b,b,sizeof line_b);
 message_until=plat_ms()+1300u;
 redraw=1;
}
static mi_button_t map_button(unsigned k)
{
 switch(k){
 case B_HOME:return MI_B_HOME;case B_EDIT:return MI_B_EDIT;
 case B_FX:return MI_B_FX;case B_SEL:return MI_B_SEL;
 case B_ENV:return MI_B_ENV;case B_LFO:return MI_B_LFO;
 case B_ARP:return MI_B_ARP;case B_SEQ:return MI_B_SEQ;
 case B_GLO:return MI_B_GLO;case B_SAVE:return MI_B_SAVE;
 case B_PLAY:return MI_B_PLAY;case B_REC:return MI_B_REC;
 case B_OCTDN:return MI_B_OCT_DOWN;case B_OCTUP:return MI_B_OCT_UP;
 default:return MI_B_HOME;
 }
}
static mi_encoder_t map_encoder(unsigned k)
{
 switch(k){
 case EN_SELECT:return MI_EN_SELECT;case EN_ALGO:return MI_EN_ALGORITHM;
 case EN_PRESET:return MI_EN_PRESETS;
 case EN_K1:return MI_EN_K1;case EN_K2:return MI_EN_K2;
 case EN_K3:return MI_EN_K3;default:return MI_EN_K4;
 }
}
static void input(void)
{
 uint32_t now=plat_ms(),keys=touch_keys(),buttons=plat_buttons(),i,bit;
 int32_t steps;
 touch_poll();
 keys=touch_keys();
 for(i=0;i<NKEYS;i++){
     bit=1u<<i;
     uint8_t note=(uint8_t)(53+(int)i+12*(int)state.octave);
     if((keys&bit)&&!(last_keys&bit)){
         held_note[i]=note;
         mi_audio_input_on(&audio,&state,note,(uint8_t)touch_vel(i));
     }
     if(!(keys&bit)&&(last_keys&bit))
         mi_audio_input_off(&audio,&state,held_note[i]);
 }
 if(keys!=last_keys)redraw=1;
 last_keys=keys;
 for(i=0;i<NB;i++){
     mi_result_t action;
     bit=1u<<i;
     if(!(buttons&bit)||(last_buttons&bit))continue;
     action=mi_button(&state,map_button(i),now);
     redraw=1;
     if(action.action==MI_ACTION_SAVE){
         saved_t save;
         save.magic=SAVED_MAGIC;
         save.state=state;
         save.state.save_armed=0;
         app_say(plat_store_save(OBJ_SETTINGS,&save,sizeof save)==0?"SAVED":"SAVE FAILED","");
     }else if(action.action==MI_ACTION_PLAY||action.action==MI_ACTION_RECORD)
         app_say("SEQ/REC","NOT READY");
 }
 last_buttons=buttons;
 for(i=0;i<NE;i++){
     steps=plat_enc((int)i);
     if(steps){mi_rotate(&state,map_encoder(i),steps);redraw=1;}
 }
 plat_glow(1);
 plat_leds((1u<<(state.view==MI_OSC?B_EDIT:
                   state.view==MI_VIDEO?B_SEL:
                   state.view==MI_FX?B_FX:
                   state.view==MI_SAVE?B_SAVE:B_HOME))|buttons,keys);
}
void app_input_only(void){input();}
/* Decode all original-source frame slots to bounded 4bpp scratch, using
 * independent 16-colour palettes for each 36-frame clip.
 * No runtime dependency on other firmware or their DSP.
 * Source imagery is perceptually quantised by the build generator: NOT bit-exact.
 */
static uint8_t mi_media_pixels[MI_EFFECTS_ASSET_BYTES];
static int16_t mi_media_prior=-1;
static const uint16_t *mi_media_palette;
static void mi_media_decode(unsigned frame)
{
 unsigned ty,tx,row,at;
 const uint8_t (*book)[16];
 const uint8_t *map;
 if(frame>=MI_EFFECTS_ASSET_COUNT)return;
 if(mi_media_prior==(int16_t)frame)return;
 book=mi_effects_vq_tilebook[frame/36u];
 map=mi_effects_vq_maps[frame];
 mi_media_palette=mi_effects_vq_palette[frame/36u];
 for(ty=0;ty<20u;ty++)for(tx=0;tx<16u;tx++){
    const uint8_t *tile=book[map[ty*16u+tx]];
    for(row=0;row<4u;row++){
        unsigned y=ty*4u+row;
        if(y>=MI_EFFECTS_ASSET_H)break;
        at=y*64u+tx*4u;
        mi_media_pixels[at]=tile[row*4u];
        mi_media_pixels[at+1u]=tile[row*4u+1u];
        mi_media_pixels[at+2u]=tile[row*4u+2u];
        mi_media_pixels[at+3u]=tile[row*4u+3u];
    }
 }
 mi_media_prior=(int16_t)frame;
}
/* Draw each 120-row LCD half separately. SEL displays only the full-width
 * original animation (128x78 -> 240x146) centered at y=47, with no labels
 * or hidden FX parameter values. No source pixels are cropped or stretched.
 */
static void mi_media_draw_half(unsigned half)
{
    unsigned sy;
    int frame=video.effects_frame;
    const int32_t shift=(int32_t)half*120;
    if(frame<0)return;
    mi_media_decode((unsigned)frame);
    for(sy=0;sy<MI_EFFECTS_ASSET_H;sy++){
        int32_t absolute_y0=47+(int32_t)(sy*146u/MI_EFFECTS_ASSET_H);
        int32_t absolute_y1=47+(int32_t)((sy+1u)*146u/MI_EFFECTS_ASSET_H);
        int32_t y0=absolute_y0-shift,y1=absolute_y1-shift;
        unsigned x=0;
        uint32_t row=sy*(MI_EFFECTS_ASSET_W/2u);
        if(y0<0)y0=0;
        if(y1>120)y1=120;
        if(y1<=y0)continue;
        while(x<MI_EFFECTS_ASSET_W){
            unsigned at=row+(x>>1);
            unsigned idx=(x&1u)?(mi_media_pixels[at]&15u):(mi_media_pixels[at]>>4);
            unsigned from=x++,to;
            if(idx==0u)continue;
            while(x<MI_EFFECTS_ASSET_W){
                at=row+(x>>1);
                unsigned j=(x&1u)?(mi_media_pixels[at]&15u):(mi_media_pixels[at]>>4);
                if(j!=idx)break;
                ++x;
            }
            to=x;
            {
                int32_t dx0=(int32_t)(from*240u/MI_EFFECTS_ASSET_W);
                int32_t dx1=(int32_t)(to*240u/MI_EFFECTS_ASSET_W);
                if(dx1>dx0)cv_rect(dx0,y0,dx1-dx0,y1-y0,mi_media_palette[idx]);
            }
        }
    }
}
static void mi_graph_env(void)
{
    int32_t base=96,peak=40;
    int32_t sustain=base-(base-peak)*(int32_t)state.env[2]/127;
    int32_t a=20+(int32_t)state.env[0]/4;
    int32_t d=20+(int32_t)state.env[1]/4;
    int32_t r=20+(int32_t)state.env[3]/4;
    int32_t x0=12,x1=x0+a,x2=x1+d,x3=228-r;
    if(x3<x2+12)x3=x2+12;
    cv_line(12,base,228,base,DIM);
    cv_line(x0,base,x1,peak,YELLOW);
    cv_line(x1,peak,x2,sustain,YELLOW);
    cv_line(x2,sustain,x3,sustain,YELLOW);
    cv_line(x3,sustain,228,base,YELLOW);
    cv_text(12,104,&FONT_XS,"A",GREY);
    cv_text(x1,104,&FONT_XS,"D",GREY);
    cv_text(x2,104,&FONT_XS,"S",GREY);
    cv_text(x3,104,&FONT_XS,"R",GREY);
}
static float mi_lfo_graph_value(unsigned wave,float p)
{
    if(wave==1u)return p<0.5f?4.f*p-1.f:3.f-4.f*p;
    if(wave==2u)return 2.f*p-1.f;
    if(wave==3u)return p<0.5f?1.f:-1.f;
    /* Zero-crossing 1-period curved waveform, no libm dependency. */
    if(p<0.5f){
        float q=p*2.f;
        return 4.f*q*(1.f-q);
    }else{
        float q=(p-0.5f)*2.f;
        return -4.f*q*(1.f-q);
    }
}
static void mi_graph_lfo(void)
{
    unsigned x;
    float cycles=1.f+(float)state.lfo[0]/50.f;
    int32_t amp=12+(int32_t)state.lfo[2]*22/127;
    int32_t prior=75;
    cv_line(12,75,229,75,DIM);
    for(x=12;x<228;x++){
        float p=((float)(x-12)*cycles)/216.f;
        p-=(float)(int)p;
        int32_t y=75-(int32_t)(mi_lfo_graph_value(state.lfo[1],p)*(float)amp);
        if(x>12)cv_line((int32_t)x-1,prior,(int32_t)x,y,YELLOW);
        prior=y;
    }
}
static void mi_graph_arp(void)
{
    unsigned x;
    unsigned mode=state.arp[0];
    for(x=0;x<8u;x++){
        int32_t h=mode==2u?(int32_t)(8u-x)*5:
                  mode==3u?(int32_t)((x<4u?x:7u-x)+1u)*10:
                  (int32_t)(x+1u)*5;
        cv_rect(18+(int32_t)x*27,98-h,18,h,mode?YELLOW:DIM);
    }
}
static void draw(void)
{
    uint32_t i,now=plat_ms();
    char val[16];
    if(state.view==MI_VIDEO){
        /* Both LCD halves are image-only: never draw labels/values/footers. */
        cv_begin(240,120,C_BLACK);
        mi_media_draw_half(0);
        cv_blit(0,0);
        cv_begin(240,120,C_BLACK);
        mi_media_draw_half(1);
        cv_blit(0,120);
        return;
    }

    cv_begin(240,120,C_BLACK);
    cv_text(8,6,&FONT_B,APP_NAME,YELLOW);
    cv_text(140,10,&FONT_XS,mi_view_name(state.view),GREY);
    cv_rect(8,31,224,1,DIM);
    if(now<message_until){
        cv_text(12,50,&FONT_B,line_a,YELLOW);
        cv_text(12,76,&FONT_XS,line_b,GREY);
    }else if(state.view==MI_OSC||state.view==MI_HOME){
        int32_t prior=80;
        cv_text(12,35,&FONT_XS,mi_wave_name(state.wave),GREY);
        for(i=0;i<224;i++){
            float p=(float)(i%112u)/112.f;
            float value=state.wave==0?(p<0.5f?4.f*p*(1.f-2.f*p):-(4.f*(p-0.5f)*(2.f-2.f*p))):
                state.wave==1?(2.f*p-1.f):
                state.wave==2?(p<0.5f?1.f:-1.f):
                (p<0.05f+0.9f*state.width/127.f?1.f:-1.f);
            int32_t y=76-(int32_t)(value*26.f);
            if(i)cv_line((int32_t)(i+7u),prior,(int32_t)(i+8u),y,YELLOW);
            prior=y;
        }
    }else if(state.view==MI_FX){
        cv_text(12,52,&FONT_B,mi_fx_name((mi_fx_t)state.fx),YELLOW);
    }else if(state.view==MI_ENV){
        mi_graph_env();
    }else if(state.view==MI_LFO){
        mi_graph_lfo();
    }else if(state.view==MI_ARP){
        mi_graph_arp();
        cv_text(12,38,&FONT_XS,mi_arp_mode_name(state.arp[0]),GREY);
    }else{
        cv_text(12,54,&FONT_B,mi_view_name(state.view),YELLOW);
    }
    cv_blit(0,0);

    cv_begin(240,120,C_BLACK);
    cv_rect(8,9,224,1,DIM);
    for(i=0;i<4u;i++){
        int32_t x=8+(int32_t)i*58;
        cv_text(x,24,&FONT_XS,mi_param_name(&state,i),GREY);
        if((state.view==MI_OSC||state.view==MI_HOME)&&i==0u)
            textcpy(val,mi_wave_name(state.wave),sizeof val);
        else if(state.view==MI_LFO&&i==1u)
            textcpy(val,mi_lfo_wave_name(state.lfo[1]),sizeof val);
        else if(state.view==MI_LFO&&i==3u)
            textcpy(val,state.lfo[3]==0u?"PITCH":
                         state.lfo[3]==1u?"PWM":"AMP",sizeof val);
        else if(state.view==MI_ARP&&i==0u)
            textcpy(val,mi_arp_mode_name(state.arp[0]),sizeof val);
        else if(state.view==MI_ARP&&i==1u){
            static const char *const rates[]={"1/4","1/8","1/16","1/32"};
            textcpy(val,rates[state.arp[1]<=3u?state.arp[1]:3u],sizeof val);
        }else number(mi_param_value(&state,i),val);
        cv_text(x,49,&FONT_S,val,YELLOW);
    }
    cv_blit(0,120);
}
void app_ui_init(void)
{
 touch_on(1);
 lcd_fill(0,0,240,240,C_BLACK);
 redraw=1;
}
void app_frame(void)
{
 static uint32_t video_time;
 uint32_t now;
 input();
 now=plat_ms();
 if(now-video_time>=33u){
     video_time=now;
     int16_t old_effects=video.effects_frame;
     mi_video_tick(&video,audio_activity,0);
     if(state.view==MI_VIDEO && old_effects!=video.effects_frame)
         redraw=1;
 }
 if(message_until&&now>=message_until){message_until=0;redraw=1;}
 if(redraw){draw();redraw=0;}
}
#ifdef PLAT_HOST
int app_expect(const char *what,int *got)
{
 if(mi_streq(what,"view")) *got=(int)state.view;
 else if(mi_streq(what,"fx")) *got=(int)state.fx;
 else if(mi_streq(what,"wave")) *got=(int)state.wave;
 else if(mi_streq(what,"octave")) *got=(int)state.octave;
 else if(mi_streq(what,"arp_mode")) *got=(int)state.arp[0];
 else if(mi_streq(what,"arp_note")) *got=(int)audio.arp_note;
 else if(mi_streq(what,"arp_sounding")) *got=(int)audio.arp_sounding;
 else if(mi_streq(what,"lfo_wave")) *got=(int)state.lfo[1];
 else if(mi_streq(what,"preset")) *got=(int)state.preset;
 else if(mi_streq(what,"slot")) *got=(int)state.project_slot;
 else if(mi_streq(what,"active")) {
    unsigned i;int n=0;
    for(i=0;i<MI_NATIVE_POLY;i++)if(audio.v[i].active)n++;
    *got=n;
 }
 else return 0;
 return 1;
}
#endif
