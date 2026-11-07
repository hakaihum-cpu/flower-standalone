/* SPDX-License-Identifier: GPL-3.0-only
 * Independent MIYAKO FM-1 app using only fm1-base's hardware platform API.
 * This is an INCOMPLETE port. Original EFFECTS and EUREKA frames are
 * encoded at build time (16-colour lossy VQ); additionally, the six
 * user-supplied portrait/eye/city tile sheets are a 488-frame 1-bit atlas.
 * Full original MIYAKO DSP and independent video banks are not yet present.
 * Do not release this development image as the approved MIYAKO firmware.
 *
 * Compiled inside fm1-base's unity.c after gfx.c and lib/touch.c.
 * mi_control.c and mi_audio.c are separate -O2 units (fm1.toml).
 */
#include "mi_control.h"
#include "mi_audio.h"
#include "mi_video.h"
#include "mi_tile_atlas.h"
#include "mi_effects_vq_frames.h" /* 144 source frames */
#include "mi_eureka_vq_frames.h" /* 576 source frames */
#include "mi_memory.h"
#include <stddef.h>
#define YELLOW RGB(255,209,12)
#define DIM RGB(129,106,39)
#define GREY RGB(218,213,202)
#define SAVED_MAGIC 0x4D493134u /* MI14: HOME MULTI and OSC1 wave */
#define SAVED_MAGIC_PREV 0x4D493133u /* MI13: existing user's settings */
typedef struct {
 uint32_t magic;
 mi_state_t state;
 } saved_t;
/* MI13 originally stored exactly the prefix ending after save_armed_ms.
 * Its legacy image can be loaded even after extending mi_state_t with OSC2,
 * without resetting the user's sound settings to factory defaults. */
typedef struct {
  uint32_t magic;
  uint8_t state_prefix[offsetof(mi_state_t,osc2_wave)];
} saved_legacy_t;
static mi_state_t state;
static mi_audio_t audio;
static mi_video_t video;
static volatile float audio_activity;
static uint32_t last_keys,last_buttons,rec_pressed_at;
static uint8_t rec_long_done;
/* Visual selection is independent of audio FX or the saved sound preset.
 * 0=AUTO, 1..6=all six supplied photo sheets, 7=EFFECTS, 8=EUREKA. */
static uint8_t visual_choice,visual_last_mode=255u,visual_cooldown;
static uint16_t visual_tile;
static float visual_last_activity;
static uint32_t visual_rng=0x6d697961u,visual_generation;
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
void app_init(void)
{
 saved_t saved;
 int restored=0;
 int stored_bytes=plat_store_size(OBJ_SETTINGS);
 mi_init(&state);
 mi_audio_init(&audio,44100.f);
 mi_video_init(&video,MI_EFFECTS_ASSET_COUNT);
 mi_video_set_eureka(&video,MI_EUREKA_ASSET_COUNT);
 /* fm1-base plat_store_load may truncate to max and return that truncated
  * length. Check the ACTUAL stored size FIRST. Otherwise a corrupt new save
  * might be misinterpreted as a valid old MI13 prefix. */
 if(stored_bytes==(int)sizeof saved &&
    plat_store_load(OBJ_SETTINGS,&saved,sizeof saved)==(int)sizeof saved&&
    saved.magic==SAVED_MAGIC&&mi_state_valid(&saved.state)){
     state=saved.state;
     restored=1;
 }else if(stored_bytes==(int)sizeof saved &&
          plat_store_load(OBJ_SETTINGS,&saved,sizeof saved)==(int)sizeof saved&&
          saved.magic==SAVED_MAGIC_PREV){
     /* New osc1_wave occupies the old struct's tail padding; DO NOT treat
      * uninitialised MI13 padding as a valid oscillator or sound parameter. */
     mi_memcopy(&state,&saved.state,offsetof(mi_state_t,osc1_wave));
     restored=mi_state_valid(&state);
 }else if(stored_bytes==(int)sizeof(saved_legacy_t)){
     saved_legacy_t old;
     if(plat_store_load(OBJ_SETTINGS,&old,sizeof old)==(int)sizeof old&&
        old.magic==SAVED_MAGIC_PREV){
         mi_memcopy(&state,old.state_prefix,sizeof old.state_prefix);
         restored=mi_state_valid(&state);
     }
 }
 if(!restored)mi_init(&state);
 state.save_armed=0;
 state.playing=state.recording=0;
 state.view=MI_HOME;
 state.osc_page=0u;
 last_keys=last_buttons=rec_pressed_at=0u;rec_long_done=0u;
 visual_choice=0u;visual_last_mode=255u;visual_cooldown=0u;
 visual_rng=0x6d697961u;visual_generation=0u;
 visual_last_activity=0.f;visual_tile=0u;
 mi_memzero(held_note,sizeof held_note);redraw=1;
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
 /* One audio block can be shorter than the 33-ms video tick. Hold onset
  * peaks briefly so staccato notes cannot disappear before the UI sees them. */
 {
     float value=(float)peak/8388607.f;
     float retained=audio_activity*0.91f;
     audio_activity=value>retained?value:retained;
 }
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
 /* REC: release starts/stops an overwrite take, held 700ms clears.
  * The UI never clears PCM; the audio callback owns the clip. */
 {
     uint32_t bit_rec=1u<<B_REC;
     if((buttons&bit_rec)&&!(last_buttons&bit_rec)){
         rec_pressed_at=now;
         rec_long_done=0u;
     }
     if((buttons&bit_rec)&&!rec_long_done &&
        (uint32_t)(now-rec_pressed_at)>=700u){
         rec_long_done=1u;
         mi_button(&state,MI_B_SEQ,now);
         mi_audio_rec_request(&audio,3u);
         redraw=1;
     }
     if(!(buttons&bit_rec)&&(last_buttons&bit_rec)&&!rec_long_done){
         mi_button(&state,MI_B_REC,now);
         mi_audio_rec_request(&audio,1u);
         redraw=1;
     }
 }
 for(i=0;i<NB;i++){
     mi_result_t action;
     bit=1u<<i;
     if(i==B_REC||!(buttons&bit)||(last_buttons&bit))continue;
     action=mi_button(&state,map_button(i),now);
     redraw=1;
     if(action.action==MI_ACTION_SAVE){
         saved_t save;
         save.magic=SAVED_MAGIC;
         save.state=state;
         save.state.save_armed=0;
         app_say(plat_store_save(OBJ_SETTINGS,&save,sizeof save)==0?"SAVED":"SAVE FAILED","");
     }else if(action.action==MI_ACTION_RECORD){
         mi_audio_rec_request(&audio,1u);
    }else if(action.action==MI_ACTION_PLAY){
         mi_audio_rec_request(&audio,2u);
    }
 }
 last_buttons=buttons;
 for(i=0;i<NE;i++){
     steps=plat_enc((int)i);
     if(steps){
         if(i==EN_SELECT&&(state.view==MI_FX||state.view==MI_VIDEO)){
             /* Preserve each hardware detent; large steps must not be lost. */
             int32_t next=((int32_t)visual_choice+steps)%9;
             if(next<0)next+=9;
             visual_choice=(uint8_t)next;
             visual_last_mode=255u; /* force new bank selection */
         }else mi_rotate(&state,map_encoder(i),steps);
         redraw=1;
     }
 }
 plat_glow(1);
 plat_play_red(audio.recorder.recording!=0u);
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
static uint8_t mi_media_bank=255u;
static void mi_media_decode(unsigned frame,unsigned eureka)
{
 unsigned ty,tx,row,at;
 const uint8_t (*book)[16];
 const uint8_t *map;
 if(eureka){
    if(frame>=MI_EUREKA_ASSET_COUNT)return;
 }else if(frame>=MI_EFFECTS_ASSET_COUNT)return;
 if(mi_media_prior==(int16_t)frame&&mi_media_bank==eureka)return;
 if(eureka){
    book=mi_eureka_vq_tilebook[frame/36u];
    map=mi_eureka_vq_maps[frame];
    mi_media_palette=mi_eureka_vq_palette[frame/36u];
 }else{
    book=mi_effects_vq_tilebook[frame/36u];
    map=mi_effects_vq_maps[frame];
    mi_media_palette=mi_effects_vq_palette[frame/36u];
 }
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
 mi_media_bank=(uint8_t)eureka;
}
/* Draw each 120-row LCD half separately. SEL displays only the full-width
 * original animation (128x78 -> 240x146) centered at y=47, with no labels
 * or hidden FX parameter values. No source pixels are cropped or stretched.
 */
/* Six 32x32 black/white banks: each 8x8 source block is mapped to one
 * dictionary entry. Selective base64 byte reads keep only one 9.9 KiB
 * vector atlas in XIP; there is no 488-frame runtime allocation. */
static unsigned mi_atlas_sextet(char c)
{
    if(c>='A'&&c<='Z')return (unsigned)(c-'A');
    if(c>='a'&&c<='z')return (unsigned)(c-'a'+26);
    if(c>='0'&&c<='9')return (unsigned)(c-'0'+52);
    return c=='+'?62u:c=='/'?63u:0u;
}
static uint8_t mi_atlas_byte(unsigned pos)
{
    unsigned i=(pos/3u)*4u;
    unsigned v=(mi_atlas_sextet(mi_tile_atlas_base64[i])<<18) |
               (mi_atlas_sextet(mi_tile_atlas_base64[i+1u])<<12) |
               (mi_atlas_sextet(mi_tile_atlas_base64[i+2u])<<6) |
                mi_atlas_sextet(mi_tile_atlas_base64[i+3u]);
    return (uint8_t)(v>>(16u-8u*(pos%3u)));
}
/* Display effect follows the actual active FX, independently of which
 * picture bank was selected by the performer. */
static unsigned mi_visual_fx(void)
{
    unsigned fx=state.fx_slot?state.fx2:state.fx;
    if(fx==MI_FX_OFF)fx=state.fx!=MI_FX_OFF?state.fx:state.fx2;
    return fx;
}
/* Physical SELECT cycles AUTO, six original sheets, EFFECTS, and EUREKA.
 * AUTO prioritises the selected audio FX but never changes any sound params. */
static unsigned mi_visual_mode(void)
{
    unsigned fx=mi_visual_fx();
    if(visual_choice>=1u&&visual_choice<=6u)return visual_choice-1u;
    if(visual_choice==7u)return 6u;
    if(visual_choice==8u)return 7u;
    if(state.fx==MI_FX_EUREKA||state.fx2==MI_FX_EUREKA)return 7u;
    switch(fx){
    case MI_FX_DREAMY:return 0u; /* pose sheet */
    case MI_FX_REEL:return 1u;   /* eye motion */
    case MI_FX_DELAY:return 2u;  /* Shibuya/tape */
    case MI_FX_REVERB:return 3u; /* Shibuya/tape */
    case MI_FX_CHORUS:return 4u; /* portrait */
    default:return 6u;          /* existing original EFFECTS bank */
    }
}
static unsigned mi_visual_step(float activity)
{
    unsigned mode=mi_visual_mode();
    int changed=0;
    if(mode!=visual_last_mode){
        visual_last_mode=(uint8_t)mode;
        if(mode<MI_TILE_BANKS)visual_tile=mi_tile_bank_first[mode];
        visual_cooldown=0u;
        visual_last_activity=0.f;
        ++visual_generation;
        changed=1;
    }
    if(visual_cooldown)--visual_cooldown;
    if(mode<MI_TILE_BANKS && activity>0.003f &&
       (activity>visual_last_activity*1.28f+0.0015f||!visual_cooldown)){
        unsigned count=mi_tile_bank_count[mode],next;
        visual_rng^=visual_rng<<13;
        visual_rng^=visual_rng>>17;
        visual_rng^=visual_rng<<5;
        next=(unsigned)mi_tile_bank_first[mode]+visual_rng%count;
        if(count>1u && next==visual_tile)
            next=(unsigned)mi_tile_bank_first[mode]+
                 (next-(unsigned)mi_tile_bank_first[mode]+1u)%count;
        visual_tile=(uint16_t)next;
        ++visual_generation;
        visual_cooldown=activity>0.15f?3u:5u;
        changed=1;
    }
    visual_last_activity=activity;
    return (unsigned)changed;
}
/* Source-space bitmaps remain square even if the original sheet was 720x720.
 * Nearest-neighbour expansion is deliberately rough: analog video/glitch.
 * Only visible rows of each 120-row LCD half are touched. */
static void mi_tile_draw_half(unsigned half,int32_t px,int32_t py,
                              int32_t size,float activity)
{
    const int32_t shift=(int32_t)half*120;
    const unsigned fx=mi_visual_fx();
    unsigned sy;
    for(sy=0;sy<32u;sy++){
        int32_t top=py+(int32_t)(sy*(unsigned)size/32u)-shift;
        int32_t bottom=py+(int32_t)((sy+1u)*(unsigned)size/32u)-shift;
        uint8_t line[4];
        unsigned sample_row=sy;
        int32_t tear=0;
        unsigned bx,sx;
        if(bottom<=0||top>=120)continue;
        /* Effects are transformations of user-owned pixels only.
         * Delay = stagger, Chorus = double-tracking, Reel = jitter,
         * Eureka = rhythmic large tearing, Dreamy = restrained motion. */
        if(activity>0.025f&&((sy+visual_generation)%9u)==0u){
            tear=(int32_t)((visual_rng>>(sy%16u))&7u)-3;
            if(fx==MI_FX_CHORUS)tear+=sy&1u?2:-2;
            if(fx==MI_FX_DELAY)tear+=((visual_generation>>1u)&1u)?3:0;
            if(fx==MI_FX_REEL)tear+=((visual_rng>>(sy&15u))&3u);
            if(fx==MI_FX_DREAMY)tear/=2;
        }
        /* Displace whole 8x8 source blocks on strong attacks, VHS tile split.
         * No extra image data and no mutations to the original source atlas. */
        if(activity>0.15f &&
           ((visual_rng>>(sy/8u*3u))&3u)==0u &&
           (fx==MI_FX_EUREKA||fx==MI_FX_DELAY||fx==MI_FX_REEL))
            sample_row=(sy+8u)%32u;
        for(bx=0;bx<4u;bx++){
            unsigned map_pos=MI_TILE_CODE_BYTES+(unsigned)visual_tile*16u+
                             (sample_row/8u)*4u+bx;
            uint8_t code=mi_atlas_byte(map_pos);
            line[bx]=mi_atlas_byte((unsigned)code*8u+sample_row%8u);
        }
        for(sx=0;sx<32u;){
            unsigned first=sx;
            int32_t sample=(int32_t)sx+tear;
            int whiten=sample>=0&&sample<32&&
                ((line[(unsigned)sample/8u]>>(7u-((unsigned)sample%8u)))&1u);
            ++sx;
            while(sx<32u){
                int32_t sample2=(int32_t)sx+tear;
                int next=sample2>=0&&sample2<32&&
                    ((line[(unsigned)sample2/8u]>>(7u-((unsigned)sample2%8u)))&1u);
                if(next!=whiten)break;
                ++sx;
            }
            if(whiten){
                int32_t x0=px+(int32_t)(first*(unsigned)size/32u);
                int32_t x1=px+(int32_t)(sx*(unsigned)size/32u);
                if(x1>x0)cv_rect(x0,top,x1-x0,bottom-top,C_WHITE);
            }
        }
    }
    /* Render a few short, event-seeded VHS tracking tears. They only change
     * with sound onset, so silence is genuinely frozen, not autonomous noise. */
    if(activity>0.05f){
        unsigned n=activity>0.25f?5u:2u,j;
        if(fx==MI_FX_REEL)n+=2u;
        if(fx==MI_FX_DREAMY)n=1u;
        for(j=0;j<n;j++){
            unsigned h=(visual_rng>>(j*5u))^(visual_generation*127u+j*43u);
            int32_t y=py+(int32_t)(h%(unsigned)size)-shift;
            int32_t x=px+(int32_t)((h>>7)%(unsigned)(size-19));
            cv_rect(x,y,12+(int32_t)(h%8u),1,
                    fx==MI_FX_REVERB?RGB(160,160,160):C_WHITE);
        }
        if(activity>0.28f){
            unsigned k=(visual_generation*17u+visual_rng)%81u;
            int32_t x=px+(int32_t)(k%9u)*(size/10);
            int32_t y=py+(int32_t)(k/9u)*(size/10)-shift;
            cv_rect(x,y,2,2,YELLOW); /* sparse MIYAKO yellow DOT accent */
        }
    }
}
/* Original 144 + 576 VQ frame banks remain available alongside photo tiles.
 * The original encoding is 128x78; its aspect is not distorted here. */
static void mi_media_draw_scaled_half(unsigned half,int32_t px,int32_t py,
                                      int32_t width,int32_t height,int eureka)
{
    unsigned sy;
    int frame=eureka?video.eureka_frame:video.effects_frame;
    int32_t shift=(int32_t)half*120;
    if(frame<0)return;
    mi_media_decode((unsigned)frame,(unsigned)eureka);
    for(sy=0;sy<MI_EFFECTS_ASSET_H;sy++){
        int32_t y0=py+(int32_t)(sy*(unsigned)height/MI_EFFECTS_ASSET_H)-shift;
        int32_t y1=py+(int32_t)((sy+1u)*(unsigned)height/MI_EFFECTS_ASSET_H)-shift;
        unsigned x=0;
        uint32_t row=sy*(MI_EFFECTS_ASSET_W/2u);
        if(y1<=0||y0>=120)continue;
        while(x<MI_EFFECTS_ASSET_W){
            unsigned at=row+(x>>1);
            unsigned idx=(x&1u)?(mi_media_pixels[at]&15u):(mi_media_pixels[at]>>4);
            unsigned first=x++,last;
            if(idx==0u)continue;
            while(x<MI_EFFECTS_ASSET_W){
                at=row+(x>>1);
                if(((x&1u)?(mi_media_pixels[at]&15u):
                              (mi_media_pixels[at]>>4))!=idx)break;
                ++x;
            }
            last=x;
            {
                int32_t dx0=px+(int32_t)(first*(unsigned)width/MI_EFFECTS_ASSET_W);
                int32_t dx1=px+(int32_t)(last*(unsigned)width/MI_EFFECTS_ASSET_W);
                if(dx1>dx0)cv_rect(dx0,y0,dx1-dx0,y1-y0,mi_media_palette[idx]);
            }
        }
    }
}
/* Apply FX-linked analog-video defects to any media bank. The underlying
 * original EFFECTS/EUREKA frame sequence stays untouched. Event-seeded noise
 * is deterministic between played note attacks; never free-running random. */
static void mi_video_vfx_half(unsigned half,int32_t x,int32_t y,
                              int32_t w,int32_t h,float activity)
{
    unsigned fx=mi_visual_fx(),n,i;
    int32_t shift=(int32_t)half*120;
    uint32_t seed=visual_rng^(visual_generation*0x9e3779b9u);
    if(activity<0.007f)return;
    n=activity>0.18f?5u:2u;
    if(fx==MI_FX_DREAMY)n=1u;
    if(fx==MI_FX_REEL||fx==MI_FX_EUREKA)n+=2u;
    for(i=0u;i<n;i++){
        uint32_t q=seed^(i*0x45d9f3bu);
        int32_t liney=y+(int32_t)(q%(uint32_t)h)-shift;
        int32_t linex=x+(int32_t)((q>>13u)%(uint32_t)(w-18));
        int32_t width=8+(int32_t)((q>>7u)%18u);
        uint16_t color=fx==MI_FX_REVERB?RGB(116,116,116):
                       fx==MI_FX_DREAMY?RGB(170,170,170):C_WHITE;
        cv_rect(linex,liney,width,1,color);
    }
    if(fx==MI_FX_CHORUS && activity>0.08f){
        /* Lightweight video-double/ghost line, no second image buffer. */
        cv_rect(x+3,y+h/2-shift,w-8,1,RGB(122,122,122));
    }
    if(fx==MI_FX_REEL && activity>0.08f){
        /* Tracking line at 8-mm-like field boundary. */
        cv_rect(x,y+(int32_t)(visual_generation%(uint32_t)h)-shift,w,1,
                RGB(132,132,132));
    }
}
static void mi_media_draw_half(unsigned half)
{
    unsigned mode=mi_visual_mode();
    if(mode<MI_TILE_BANKS)mi_tile_draw_half(half,0,0,240,audio_activity);
    else{
        mi_media_draw_scaled_half(half,0,47,240,146,mode==7u);
        mi_video_vfx_half(half,0,47,240,146,audio_activity);
    }
}
/* Intentionally limited by the FM-1's physical 240x240 LCD: 2x2 compact
 * hardware knobs on the left; a live square source tile on the right. */
static void mi_fx_dial(unsigned half,int32_t x,int32_t y,unsigned knob)
{
    static const int8_t cx[12]={0,8,14,17,14,8,0,-8,-14,-17,-14,-8};
    static const int8_t cy[12]={-17,-14,-8,0,8,14,17,14,8,0,-8,-14};
    static const int8_t hx[11]={-10,-12,-12,-9,-5,0,5,9,12,12,10};
    static const int8_t hy[11]={10,7,2,-5,-10,-13,-10,-5,2,7,10};
    char val[16];
    unsigned i;
    int32_t oy=y-(int32_t)half*120;
    int32_t center=x+29,mid=oy+23;
    const char *label=mi_param_name(&state,knob);
    int32_t v=mi_param_value(&state,knob);
    unsigned dial=(unsigned)(v<0?0:v);
    if(knob==0u&&!state.fx_page){
        unsigned m=state.fx_slot?state.fx2:state.fx;
        textcpy(val,mi_fx_name((mi_fx_t)m),sizeof val);
        dial=m*127u/(MI_FX_COUNT-1u);
    }else{
        number(v,val);
        if(state.fx_page){
            unsigned fx=state.fx_slot?state.fx2:state.fx;
            if(fx<MI_FX_COUNT&&knob==0u)
                dial=state.fx_extra[fx][0]; /* +/-12 st value, 0..127 arc */
            if(fx<MI_FX_COUNT&&knob==1u)
                dial=state.fx_extra[fx][1]>=64u?127u:0u;
        }
    }
    cv_rect(x,oy,57,80,C_WHITE);
    cv_rect(x+1,oy+1,55,1,C_BLACK);
    cv_rect(x+1,oy+78,55,1,C_BLACK);
    for(i=0;i<12u;i++){
        unsigned next=(i+1u)%12u;
        cv_line(center+cx[i],mid+cy[i],center+cx[next],mid+cy[next],C_BLACK);
    }
    i=dial*10u/127u;
    cv_line(center,mid,center+hx[i],mid+hy[i],C_BLACK);
    cv_text(x+4,oy+43,&FONT_XS,label,C_BLACK);
    cv_rect(x+3,oy+59,51,17,C_BLACK);
    cv_text(x+5,oy+61,&FONT_XS,val,C_WHITE);
}
static void mi_fx_draw_half(unsigned half)
{
    const int32_t shift=(int32_t)half*120;
    const unsigned mode=mi_visual_mode();
    const unsigned fx=state.fx_slot?state.fx2:state.fx;
    const char *bankname=mode<MI_TILE_BANKS?mi_tile_bank_name[mode]:
                         mode==7u?"EUREKA":"EFFECTS";
    unsigned i;
    cv_begin(240,120,C_BLACK);
    cv_text(5,5-shift,&FONT_XS,"FX /",C_WHITE);
    cv_text(38,5-shift,&FONT_S,mi_fx_name((mi_fx_t)fx),C_WHITE);
    cv_text(163,5-shift,&FONT_XS,state.fx_page?"PG2":"PG1",C_WHITE);
    cv_text(199,5-shift,&FONT_XS,state.fx_slot?"FX B":"FX A",C_WHITE);
    cv_rect(4,27-shift,232,1,RGB(110,110,110));
    mi_fx_dial(half,5,34,0u);
    mi_fx_dial(half,65,34,1u);
    mi_fx_dial(half,5,122,2u);
    mi_fx_dial(half,65,122,3u);
    cv_text(133,32-shift,&FONT_XS,"PREVIEW",C_WHITE);
    if(mode<MI_TILE_BANKS){
        char ordinal[12];
        number((int32_t)(visual_tile-(uint16_t)mi_tile_bank_first[mode]+1u),ordinal);
        cv_text(196,32-shift,&FONT_XS,ordinal,C_WHITE);
    }
    cv_rect(130,46-shift,108,1,C_WHITE);
    cv_rect(130,46-shift,1,108,C_WHITE);
    cv_rect(237,46-shift,1,108,C_WHITE);
    cv_rect(130,153-shift,108,1,C_WHITE);
    if(mode<MI_TILE_BANKS){
        mi_tile_draw_half(half,133,49,102,audio_activity);
    }else{
        /* 128:78 remains 128:78, not stretched to a false square. */
        mi_media_draw_scaled_half(half,133,69,102,62,mode==7u);
        mi_video_vfx_half(half,133,69,102,62,audio_activity);
    }
    cv_text(133,164-shift,&FONT_XS,visual_choice?"LOCK":"AUTO",YELLOW);
    cv_text(133,180-shift,&FONT_XS,bankname,C_WHITE);
    cv_text(133,199-shift,&FONT_XS,"VHS / 8MM",RGB(180,180,180));
    cv_rect(133,213-shift,102,2,RGB(70,70,70));
    i=(unsigned)(audio_activity*102.f);
    if(i>102u)i=102u;
    if(i)cv_rect(133,213-shift,(int32_t)i,2,YELLOW);
    cv_rect(3,221-shift,234,1,RGB(100,100,100));
    cv_text(6,225-shift,&FONT_XS,"PRESETS PAGE / SELECT BANK / SEL FULL",C_WHITE);
    cv_blit(0,half*120u);
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
static void mi_graph_recorder(void)
{
    unsigned x;
    uint32_t count=audio.recorder.count;
    uint32_t inx=(uint32_t)state.seq[0]*223u/127u;
    uint32_t outx=(uint32_t)state.seq[1]*223u/127u;
    cv_text(12,36,&FONT_XS,
        audio.recorder.recording?"RECORDING":
        audio.recorder.playing?"LOOP PLAY":"STOPPED",YELLOW);
    cv_line(8,78,232,78,DIM);
    for(x=0;x<224u;x++){
        int8_t mn,mx;
        int32_t y0,y1;
        if(x*MI_REC_CAPACITY/224u>=count)break;
        mi_rec_peak(x,&mn,&mx);
        y0=78-(int32_t)mx/3;
        y1=78-(int32_t)mn/3;
        if(y1<y0)y1=y0;
        cv_line(8+(int32_t)x,y0,8+(int32_t)x,y1,YELLOW);
    }
    cv_line(8+(int32_t)inx,45,8+(int32_t)inx,114,GREY);
    cv_line(8+(int32_t)outx,45,8+(int32_t)outx,114,GREY);
    if(audio.recorder.playing){
        uint32_t cursor=(uint32_t)(audio.recorder.playback_pos*223.f/(float)MI_REC_CAPACITY);
        if(cursor>223u)cursor=223u;
        cv_line(8+(int32_t)cursor,45,8+(int32_t)cursor,114,RGB(120,240,140));
    }
}
static void draw(void)
{
    uint32_t i,now=plat_ms();
    char val[16];
    if(state.view==MI_FX){
        mi_fx_draw_half(0u);
        mi_fx_draw_half(1u);
        return;
    }
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
    cv_text(140,10,&FONT_XS,
            state.view==MI_OSC?(state.osc_page?"OSC2":"OSC1"):
            mi_view_name(state.view),GREY);
    cv_rect(8,31,224,1,DIM);
    if(now<message_until){
        cv_text(12,50,&FONT_B,line_a,YELLOW);
        cv_text(12,76,&FONT_XS,line_b,GREY);
    }else if(state.view==MI_OSC||state.view==MI_HOME){
        int32_t prior=80;
        unsigned show_wave=state.wave==MI_WAVE_MULTI?
                           (state.osc_page?state.osc2_wave:state.osc1_wave):
                           state.view==MI_OSC&&state.osc_page?
                           state.osc2_wave:state.wave;
        cv_text(12,35,&FONT_XS,
                state.view==MI_HOME&&state.wave==MI_WAVE_MULTI?
                "MULTI":mi_wave_name(show_wave),GREY);
        for(i=0;i<224;i++){
            float p=(float)(i%112u)/112.f;
            float value=show_wave==0?(p<0.5f?4.f*p*(1.f-2.f*p):-(4.f*(p-0.5f)*(2.f-2.f*p))):
                show_wave==1?(2.f*p-1.f):
                show_wave==2?(p<0.5f?1.f:-1.f):
                (p<0.05f+0.9f*state.width/127.f?1.f:-1.f);
            int32_t y=76-(int32_t)(value*26.f);
            if(i)cv_line((int32_t)(i+7u),prior,(int32_t)(i+8u),y,YELLOW);
            prior=y;
        }
    }else if(state.view==MI_FX){
        unsigned mode=state.fx_slot?state.fx2:state.fx;
        cv_text(12,36,&FONT_XS,state.fx_slot?"FX B":"FX A",GREY);
        cv_text(12,58,&FONT_B,mi_fx_name((mi_fx_t)mode),YELLOW);
    }else if(state.view==MI_ENV){
        mi_graph_env();
    }else if(state.view==MI_LFO){
        mi_graph_lfo();
    }else if(state.view==MI_SEQ){
        mi_graph_recorder();
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
            textcpy(val,mi_wave_name(state.view==MI_OSC?
                  (state.osc_page?state.osc2_wave:
                   state.wave==MI_WAVE_MULTI?state.osc1_wave:state.wave):
                   state.wave),sizeof val);
        else if(state.view==MI_HOME&&state.wave==MI_WAVE_MULTI&&i==1u)
            textcpy(val,state.osc_page?"OSC2":"OSC1",sizeof val);
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
        }else if(state.view==MI_SEQ&&i==2u){
            static const int semitone[7]={-12,-7,-5,0,5,7,12};
            number(semitone[state.seq[2]<=6u?state.seq[2]:3u],val);
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
     int16_t old_effects=video.effects_frame,old_eureka=video.eureka_frame;
     unsigned visual_changed=mi_visual_step(audio_activity);
     unsigned mode=mi_visual_mode();
     /* Never process off-screen video banks when a photo-tile sheet is chosen. */
     if(mode>=6u)mi_video_tick(&video,audio_activity,mode==7u);
     if((state.view==MI_VIDEO||state.view==MI_FX) &&
        (visual_changed||(mode==6u&&old_effects!=video.effects_frame)||
         (mode==7u&&old_eureka!=video.eureka_frame)))
         redraw=1;
     if(state.view==MI_SEQ && (audio.recorder.recording||audio.recorder.playing))
         redraw=1;
 }
  state.recording=audio.recorder.recording;
  state.playing=audio.recorder.playing;
  if(message_until&&now>=message_until){message_until=0;redraw=1;}
 if(redraw){draw();redraw=0;}
}
#ifdef PLAT_HOST
int app_expect(const char *what,int *got)
{
 if(mi_streq(what,"view")) *got=(int)state.view;
 else if(mi_streq(what,"fx")) *got=(int)state.fx;
  else if(mi_streq(what,"fx2")) *got=(int)state.fx2;
  else if(mi_streq(what,"fx_slot")) *got=(int)state.fx_slot;
 else if(mi_streq(what,"wave")) *got=(int)state.wave;
 else if(mi_streq(what,"octave")) *got=(int)state.octave;
 else if(mi_streq(what,"arp_mode")) *got=(int)state.arp[0];
 else if(mi_streq(what,"arp_note")) *got=(int)audio.arp_note;
 else if(mi_streq(what,"arp_sounding")) *got=(int)audio.arp_sounding;
 else if(mi_streq(what,"lfo_wave")) *got=(int)state.lfo[1];
 else if(mi_streq(what,"recording")) *got=(int)audio.recorder.recording;
  else if(mi_streq(what,"rec_playing")) *got=(int)audio.recorder.playing;
  else if(mi_streq(what,"rec_samples")) *got=(int)audio.recorder.count;
  else if(mi_streq(what,"rec_pitch")) *got=(int)state.seq[2];
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
