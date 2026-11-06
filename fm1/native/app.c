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
 last_keys=last_buttons=0;mi_memzero(held_note,0,sizeof held_note);redraw=1;
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
     if(kind==0x90u&&velocity)mi_audio_note_on(&audio,note,velocity);
     else if(kind==0x80u||(kind==0x90u&&!velocity))
         mi_audio_note_off(&audio,note);
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
         mi_audio_note_on(&audio,note,(uint8_t)touch_vel(i));
     }
     if(!(keys&bit)&&(last_keys&bit))
         mi_audio_note_off(&audio,held_note[i]);
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
static void mi_media_draw(void)
{
 uint32_t y,x;
 int frame=video.effects_frame;
 if(frame<0)return;
 mi_media_decode((unsigned)frame);
 for(y=0;y<MI_EFFECTS_ASSET_H;y++){
     x=0;
     while(x<MI_EFFECTS_ASSET_W){
         unsigned b=y*MI_EFFECTS_ASSET_W+x;
         unsigned idx=(x&1u)?(mi_media_pixels[b>>1]&15u):(mi_media_pixels[b>>1]>>4);
         unsigned start=x++,end;
         if(idx==0u)continue;
         while(x<MI_EFFECTS_ASSET_W){
             b=y*MI_EFFECTS_ASSET_W+x;
             unsigned j=(x&1u)?(mi_media_pixels[b>>1]&15u):(mi_media_pixels[b>>1]>>4);
             if(j!=idx)break;
             ++x;
         }
         end=x;
         {
             int32_t x0=24+(int32_t)(start*3u/2u);
             int32_t x1=24+(int32_t)(end*3u/2u);
             int32_t y0=(int32_t)(y*3u/2u);
             int32_t y1=(int32_t)((y+1u)*3u/2u);
             if(x1>x0&&y1>y0)
                 cv_rect(x0,y0,x1-x0,y1-y0,mi_media_palette[idx]);
         }
     }
 }
}
static void draw(void)
{
 uint32_t i,now=plat_ms();
 char val[16];
 cv_begin(240,120,C_BLACK);
 if(state.view==MI_VIDEO){
     mi_media_draw();
 }else{
     cv_text(8,6,&FONT_B,APP_NAME,YELLOW);
     cv_text(140,10,&FONT_XS,mi_view_name(state.view),GREY);
     cv_rect(8,31,224,1,DIM);
     if(now<message_until){
         cv_text(12,50,&FONT_B,line_a,YELLOW);
         cv_text(12,76,&FONT_XS,line_b,GREY);
     }else if(state.view==MI_OSC||state.view==MI_HOME){
         int32_t prior=80;
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
         if(state.fx==MI_FX_DREAMY)
             cv_text(12,81,&FONT_XS,"AUDIO DSP PENDING",GREY);
     }else{
         cv_text(12,54,&FONT_B,mi_view_name(state.view),YELLOW);
     }
 }
 cv_blit(0,0);

 cv_begin(240,120,C_BLACK);
 cv_rect(8,9,224,1,DIM);
 for(i=0;i<4u;i++){
     int32_t x=8+(int32_t)i*58;
     cv_text(x,24,&FONT_XS,mi_param_name(&state,i),GREY);
     number(mi_param_value(&state,i),val);
     cv_text(x,49,&FONT_B,val,YELLOW);
 }
 cv_text(8,87,&FONT_XS,"MIYAKO FM-1 / NATIVE PREVIEW",DIM);
 cv_text(8,104,&FONT_XS,"NOT FULL MIYAKO DSP",DIM);
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
