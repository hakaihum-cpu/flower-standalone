/* SPDX-License-Identifier: GPL-3.0-only
 * Independent MIYAKO FM-1 app using only fm1-base's hardware platform API.
 * This is an INCOMPLETE port. Original EFFECTS and EUREKA frames are
 * encoded at build time (16-colour lossy VQ); additionally, the six
 * user-supplied portrait/eye/city tile sheets are a 488-frame 16-colour source VQ atlas.
 * Full original MIYAKO DSP and independent video banks are not yet present.
 * Do not release this development image as the approved MIYAKO firmware.
 *
 * Compiled inside fm1-base's unity.c after gfx.c and lib/touch.c.
 * mi_control.c and mi_audio.c are separate -O2 units (fm1.toml).
 */
#include "mi_control.h"
#include "mi_audio.h"
#include "mi_video.h"
#include "mi_resource_media.h"
#include "mi_dreamy_dot_portrait.h"
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
static uint8_t backup_hold;
/* Visual selection is independent of audio FX or the saved sound preset.
 * 0=AUTO, 1..6=supplied photo sheets, 7=EFFECTS, 8=EUREKA,
 * 9=original DREAMY DOT portrait, 10..11=two new supplied image sheets. */
static uint8_t visual_choice,visual_last_mode=255u,visual_cooldown;
static uint16_t visual_tile;
static uint8_t normal_visual_bank; /* SEL independent integrated playlist */
static float visual_last_activity;
static uint32_t visual_rng=0x6d697961u,visual_generation;
static float visual_dot_phase; /* original 24-Hz MiyakoPixelMotion phase */
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
 visual_last_activity=0.f;visual_tile=0u;normal_visual_bank=6u;
  visual_dot_phase=0.f;
 mi_memzero(held_note,sizeof held_note);redraw=1;
 message_until=0;line_a[0]=line_b[0]=0;
}
void app_panic(void){mi_audio_panic(&audio);}
/* fm1-base backup hooks are REQUIRED by the stock resources installer.
 * Settings are already saved synchronously through plat_store_save().
 * No new save format, project store or asynchronous worker is introduced. */
int app_backup_playing(void)
{
    unsigned i;
    if(audio.recorder.recording||audio.recorder.playing||audio.held_total||
       audio.arp_sounding)return 1;
    for(i=0u;i<MI_NATIVE_POLY;i++)if(audio.v[i].active)return 1;
    return 0;
}
void app_backup_flush(void){/* current SAVE is synchronous */ }
void app_backup_hold(int on){backup_hold=(uint8_t)(on!=0);}
void app_backup_restored(void){app_init();}
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
 /* REC: release starts/stops a take (OVERDUB if a clip exists).
  * Only a 700ms hold clears the clip on the audio callback. */
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
     /* SEL always returns to normal animation, never locks to FX imagery. */
     if(i==B_SEL){
         visual_choice=0u;visual_last_mode=255u;
     }
     action=mi_button(&state,map_button(i),now);
     redraw=1;
     if(action.action==MI_ACTION_SAVE){
         saved_t save;
         save.magic=SAVED_MAGIC;
         save.state=state;
         save.state.save_armed=0;
         app_say((!backup_hold&&plat_store_save(OBJ_SETTINGS,&save,sizeof save)==0)?
                  "SAVED":"SAVE FAILED","");
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
             int32_t next=((int32_t)visual_choice+steps)%12;
             if(next<0)next+=12;
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
/* Photo frames use the same static RGB565 palette + 8x4 VQ arrays
 * as mi_media_decode() uses for EFFECTS/EUREKA. No base64 runtime work. */
/* Display effect follows the actual active FX, independently of which
 * picture bank was selected by the performer. */
static unsigned mi_visual_fx(void)
{
    unsigned fx=state.fx_slot?state.fx2:state.fx;
    if(fx==MI_FX_OFF)fx=state.fx!=MI_FX_OFF?state.fx:state.fx2;
    return fx;
}
/* SEL AUTO is independent of any audio FX; its playlist includes all
 * available integrated sources. FX OFF always uses this same base feed. */
static unsigned mi_visual_mode(void)
{
    unsigned fx=mi_visual_fx();
    /* SEL normal animation is independent of sound FX; SELECT may pin
     * any source bank after entering SEL. With AUTO, sound events cycle
     * through all currently integrated banks, even with both FX OFF. */
    if(state.view==MI_VIDEO){
        if(visual_choice>=1u&&visual_choice<=11u)
            return visual_choice-1u;
        return normal_visual_bank;
    }
    /* FX OFF always falls back to the regular animation feed. */
    if(state.fx==MI_FX_OFF&&state.fx2==MI_FX_OFF)
        return normal_visual_bank;
    if(visual_choice>=1u&&visual_choice<=11u)
        return visual_choice-1u;
    if(state.fx==MI_FX_EUREKA||state.fx2==MI_FX_EUREKA)return 7u;
    switch(fx){
    case MI_FX_DREAMY:return 8u; /* original DOT Dreamy */
    case MI_FX_DELAY:return 2u;
    case MI_FX_REVERB:return 3u;
    case MI_FX_CHORUS:return 4u;
    default:return normal_visual_bank;
    }
}
/* Normal SEL keeps original EFFECTS/EUREKA (6/7) and DREAMY DOT (8).
 * Supplied extra image sheets appear in modes 9/10. */
static int mi_visual_photo_bank(unsigned mode)
{
    if(mode<6u)return (int)mode;
    if(mode==9u||mode==10u)return (int)(mode-3u);
    return -1;
}
static unsigned mi_visual_step(float activity)
{
    int changed=0;
    unsigned mode;
    int photo_bank;
    /* Rotate the normal SEL AUTO bank ONLY at audible note attacks.
     * All six user sheets plus original EFFECTS/EUREKA are in this cycle. */
    if(state.view==MI_VIDEO&&visual_choice==0u &&
       activity>0.012f&&activity>visual_last_activity*1.5f+0.004f){
        normal_visual_bank=(uint8_t)((normal_visual_bank+1u)%11u);
        visual_last_mode=255u;
        changed=1;
    }
    mode=mi_visual_mode();
    photo_bank=mi_visual_photo_bank(mode);
    if(mode!=visual_last_mode){
        visual_last_mode=(uint8_t)mode;
        if(photo_bank>=0)visual_tile=mi_tile_bank_first[photo_bank];
        visual_cooldown=0u;
        visual_last_activity=0.f;
        ++visual_generation;
        changed=1;
    }
    if(visual_cooldown)--visual_cooldown;
    if(photo_bank>=0 && activity>0.003f &&
       (activity>visual_last_activity*1.28f+0.0015f||!visual_cooldown)){
        unsigned count=mi_tile_bank_count[photo_bank],next;
        visual_rng^=visual_rng<<13;
        visual_rng^=visual_rng>>17;
        visual_rng^=visual_rng<<5;
        next=(unsigned)mi_tile_bank_first[photo_bank]+visual_rng%count;
        if(count>1u && next==visual_tile)
            next=(unsigned)mi_tile_bank_first[photo_bank]+
                 (next-(unsigned)mi_tile_bank_first[photo_bank]+1u)%count;
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
/* Native 16-colour RGB565 VQ 8x4, identical to EFFECTS/EUREKA.
 * Each frame is 72 byte IDs; this scratch frame is cached until the
 * performer actually switches frame, so animation cannot cause redundant
 * full-frame decompression in both LCD halves. */
static uint8_t mi_photo_pixels[MI_TILE_SIDE*MI_TILE_SIDE/2u];
static uint16_t mi_photo_palette[16];
static int16_t mi_photo_cached=-1;
static void mi_photo_decode(unsigned frame)
{
    uint32_t length=0u;
    const uint8_t *res;
    unsigned clip,ty,tx,row,col,i;
    if(frame>=MI_TILE_COUNT||mi_photo_cached==(int16_t)frame)return;
    /* Standard fm1-base CRC-checked resource area; NEVER access it on the
     * audio callback. Without the separate resource install, no fake frames. */
    res=plat_res(&length);
    if(!res||length<MI_TILE_DATA_BYTES){
        mi_photo_cached=-1;
        return;
    }
    /* Same existing 16-colour 8x4 VQ decoder for ALL 628 frames.
     * The previous 488 frames retain byte-for-byte resource offsets.
     * Each new 10x7 sheet starts a 36-frame-palette pair, so frames 558+
     * do not unintentionally share a codebook with sheet 1's last clip. */
    {
        unsigned palette_base,book_base,map_base,local_frame;
        if(frame<MI_TILE_OLD_COUNT){
            clip=frame/MI_TILE_VQ_FRAMES_PER_CLIP;
            palette_base=MI_TILE_VQ_PALETTE_OFFSET;
            book_base=MI_TILE_VQ_BOOK_OFFSET;
            map_base=MI_TILE_VQ_MAP_OFFSET;
            local_frame=frame;
        }else{
            local_frame=frame-MI_TILE_NEW_FIRST;
            clip=frame<MI_TILE_NEW_SHEET2_FIRST?
                local_frame/36u:2u+(frame-MI_TILE_NEW_SHEET2_FIRST)/36u;
            palette_base=MI_TILE_NEW_PALETTE_OFFSET;
            book_base=MI_TILE_NEW_BOOK_OFFSET;
            map_base=MI_TILE_NEW_MAP_OFFSET;
        }
        for(i=0u;i<16u;i++){
            unsigned off=palette_base+clip*32u+i*2u;
            mi_photo_palette[i]=(uint16_t)((uint16_t)res[off]|
                                          ((uint16_t)res[off+1u]<<8));
        }
        for(ty=0u;ty<MI_TILE_VQ_ROWS;ty++)
            for(tx=0u;tx<MI_TILE_VQ_COLS;tx++){
                unsigned id=res[map_base+
                                local_frame*MI_TILE_VQ_MAP_STRIDE+
                                ty*MI_TILE_VQ_COLS+tx];
                const uint8_t *tile=res+book_base+clip*4096u+id*16u;
            for(row=0u;row<4u;row++)
                for(col=0u;col<4u;col++){
                    unsigned at=(ty*4u+row)*(MI_TILE_SIDE/2u)+tx*4u+col;
                    mi_photo_pixels[at]=tile[row*4u+col];
                }
    }
    }
    mi_photo_cached=(int16_t)frame;
}
/* Original MIYAKO DOT Dreamy (MiyakoPixelMotion.h::Scene::Dreamy).
 * The portrait and tears are NOT new generated media: source is the
 * unmodified 60x46 2-bit image in initPortrait() and its eight source
 * procedural streams. The 64-sample sin lookup replaces std::sin on the
 * freestanding pi32v2 target, without allocating a framebuffer. */
static uint8_t mi_dreamy_dot_packed[MI_DREAMY_DOT_BYTES];
static uint8_t mi_dreamy_dot_ready;
static const int16_t mi_dreamy_dot_sin64[64]={
    0,100,200,297,392,483,569,650,
    724,792,851,903,946,980,1004,1019,
    1024,1019,1004,980,946,903,851,792,
    724,650,569,483,392,297,200,100,
    0,-100,-200,-297,-392,-483,-569,-650,
    -724,-792,-851,-903,-946,-980,-1004,-1019,
    -1024,-1019,-1004,-980,-946,-903,-851,-792,
    -724,-650,-569,-483,-392,-297,-200,-100
};
static void mi_dreamy_dot_unpack(void)
{
    unsigned acc=0u,bits=0u,cursor=0u;
    const char *p=mi_dreamy_dot_portrait_b64;
    if(mi_dreamy_dot_ready)return;
    while(*p && cursor<MI_DREAMY_DOT_BYTES){
        char c=*p++;
        int n=-1;
        if(c>='A'&&c<='Z')n=c-'A';
        else if(c>='a'&&c<='z')n=c-'a'+26;
        else if(c>='0'&&c<='9')n=c-'0'+52;
        else if(c=='+')n=62;
        else if(c=='/')n=63;
        if(n<0)continue;
        acc=(acc<<6)|(unsigned)n;
        bits+=6u;
        if(bits>=8u){
            bits-=8u;
            mi_dreamy_dot_packed[cursor++]=(uint8_t)((acc>>bits)&255u);
        }
    }
    mi_dreamy_dot_ready=(uint8_t)(cursor==MI_DREAMY_DOT_BYTES);
}
/* 65 cells across matches the original MiyakoPixelMotion 720-square scene
 * (60x46 portrait, centered at 2,9). Direct rectangles mirror the original
 * nearest-neighbour JUCE drawing and retain the original source aspect. */
static void mi_dreamy_dot_pixel(unsigned half,int32_t px,int32_t py,
                               int32_t size,int x,int y,uint16_t color)
{
    int32_t top,bottom,left,right;
    if(x<0||y<0||x>=65||y>=65)return;
    top=py+(int32_t)((unsigned)y*(unsigned)size/65u)-(int32_t)half*120;
    bottom=py+(int32_t)((unsigned)(y+1)*(unsigned)size/65u)-(int32_t)half*120;
    if(bottom<=0||top>=120)return;
    if(top<0)top=0;
    if(bottom>120)bottom=120;
    left=px+(int32_t)((unsigned)x*(unsigned)size/65u);
    right=px+(int32_t)((unsigned)(x+1)*(unsigned)size/65u);
    if(left<0)left=0;
    if(right>240)right=240;
    if(right>left&&bottom>top)cv_rect(left,top,right-left,bottom-top,color);
}
static void mi_dreamy_dot_draw_half(unsigned half,int32_t px,int32_t py,
                                    int32_t size)
{
    unsigned x,y,i;
    const float sadness=0.5f,fragility=0.4f,nostalgia=0.3f,melancholy=0.3f;
    const uint16_t amber=RGB(255,216,54),ink=RGB(255,219,70);
    const uint16_t dim=RGB(128,103,30);
    mi_dreamy_dot_unpack();
    if(!mi_dreamy_dot_ready)return;
    {   /* Clip the square's background to the active 120-row LCD half. */
        int32_t top=py-(int32_t)half*120,bottom=top+size;
        if(top<0)top=0;
        if(bottom>120)bottom=120;
        if(bottom>top)cv_rect(px,top,size,bottom-top,RGB(17,16,11));
    }
    for(y=0u;y<MI_DREAMY_DOT_H;y++)
        for(x=0u;x<MI_DREAMY_DOT_W;x++){
            unsigned k=y*MI_DREAMY_DOT_W+x;
            unsigned pixel=mi_dreamy_dot_packed[k>>2u];
            unsigned col=(pixel>>(6u-(k&3u)*2u))&3u;
            if(col==1u)mi_dreamy_dot_pixel(half,px,py,size,(int)x+2,
                                          (int)y+9,RGB(150,114,18));
            else if(col==2u)mi_dreamy_dot_pixel(half,px,py,size,(int)x+2,
                                               (int)y+9,amber);
        }
    for(i=0u;i<2u+(unsigned)(sadness*12.f);i++){
        float seed=(float)i*1.763f;
        float cycle=visual_dot_phase*(0.010f+
                   0.025f*(1.f-melancholy))+seed;
        float angle;
        unsigned wave_index,t,p;
        int xdot,ydot;
        /* Original fmod(positive_phase, 1.0f) without libm. */
        cycle-=(float)((unsigned)cycle);
        if(i>1u && !(sadness>((float)(i-1u)/12.f)))continue;
        angle=cycle*2.f+seed;
        while(angle>=6.2831853f)angle-=6.2831853f;
        wave_index=((unsigned)(angle*10.18591636f+0.5f))&63u;
        xdot=2+(i%2u==0u?27:46)+mi_dreamy_dot_sin64[wave_index]*2/1024;
        ydot=9+20+(int)(cycle*65.f*0.62f);
        for(t=0u;t<1u+(unsigned)(nostalgia*9.f);t++)
            mi_dreamy_dot_pixel(half,px,py,size,xdot,ydot-(int)t,
                               t==0u?ink:dim);
        if(fragility>0.05f && cycle>0.25f+0.5f*(1.f-fragility)){
            for(p=0u;p<4u;p++){
                int spread=(int)(fragility*(float)(p+1u)*2.f);
                mi_dreamy_dot_pixel(half,px,py,size,
                    xdot+(p%2u?spread:-spread),ydot+(int)p*2,dim);
            }
        }
    }
}
static unsigned mi_photo_index(unsigned sy,unsigned sx)
{
    unsigned v=mi_photo_pixels[sy*(MI_TILE_SIDE/2u)+(sx>>1)];
    return (sx&1u)?v&15u:v>>4;
}
static void mi_tile_draw_half(unsigned half,int32_t px,int32_t py,
                              int32_t size,float activity)
{
    const int32_t shift=(int32_t)half*120;
    const unsigned fx=mi_visual_fx();
    unsigned sy;
    /* The approved 628 photos are stored in FM-1's separate CRC-checked
     * resource area, not in FWSC. An updater that writes FWSC alone leaves
     * plat_res() absent. Do not silently draw an empty black square in
     * either SEL or FX: use the already-integrated original DREAMY DOT
     * animation until the matching resource is installed.
     * Check even a cached photo, as the host/device resource can disappear.
     * plat_res() is a cheap validity-gated pointer lookup (device/res.c). */
    {
        uint32_t resource_length=0u;
        if(visual_tile>=MI_TILE_COUNT ||
           !plat_res(&resource_length) ||
           resource_length<MI_TILE_DATA_BYTES){
            mi_photo_cached=-1;
            mi_dreamy_dot_draw_half(half,px,py,size);
            return;
        }
    }
    mi_photo_decode((unsigned)visual_tile);
    if(mi_photo_cached!=(int16_t)visual_tile){
        mi_dreamy_dot_draw_half(half,px,py,size);
        return;
    }
    for(sy=0u;sy<MI_TILE_SIDE;sy++){
        int32_t top=py+(int32_t)(sy*(unsigned)size/MI_TILE_SIDE)-shift;
        int32_t bottom=py+(int32_t)((sy+1u)*(unsigned)size/MI_TILE_SIDE)-shift;
        unsigned sample_row=sy,sx=0u;
        int32_t tear=0;
        if(bottom<=0||top>=120)continue;
        if(activity>0.025f&&((sy+visual_generation)%9u)==0u){
            tear=(int32_t)((visual_rng>>(sy%16u))&7u)-3;
            if(fx==MI_FX_CHORUS)tear+=(sy&1u)?2:-2;
            if(fx==MI_FX_DELAY)tear+=((visual_generation>>1u)&1u)?3:0;
            if(fx==MI_FX_REEL)tear+=(int32_t)((visual_rng>>(sy&15u))&3u);
            if(fx==MI_FX_DREAMY)tear/=2;
        }
        if(activity>0.15f &&
           ((visual_rng>>((sy/4u*3u)%24u))&3u)==0u &&
           (fx==MI_FX_EUREKA||fx==MI_FX_DELAY||fx==MI_FX_REEL))
            sample_row=(sy+4u)%MI_TILE_SIDE;
        while(sx<MI_TILE_SIDE){
            unsigned first=sx,idx=0u;
            int32_t col=(int32_t)sx+tear;
            if(col>=0&&col<(int32_t)MI_TILE_SIDE)
                idx=mi_photo_index(sample_row,(unsigned)col);
            ++sx;
            while(sx<MI_TILE_SIDE){
                unsigned next=0u;
                col=(int32_t)sx+tear;
                if(col>=0&&col<(int32_t)MI_TILE_SIDE)
                    next=mi_photo_index(sample_row,(unsigned)col);
                if(next!=idx)break;
                ++sx;
            }
            if(idx){
                int32_t x0=px+(int32_t)(first*(unsigned)size/MI_TILE_SIDE);
                int32_t x1=px+(int32_t)(sx*(unsigned)size/MI_TILE_SIDE);
                if(x1>x0)
                    cv_rect(x0,top,x1-x0,bottom-top,mi_photo_palette[idx]);
            }
        }
    }
    /* Same event-driven native VFX overlay as the previous photo path.
     * FX OFF shows the original supplied image without FX-specific noise. */
    if(fx!=MI_FX_OFF&&activity>0.05f){
        unsigned j,n=activity>0.25f?5u:2u;
        if(fx==MI_FX_DREAMY)n=1u;
        for(j=0u;j<n;j++){
            unsigned q=(visual_rng>>(j*5u))^(visual_generation*127u+j*43u);
            int32_t y=py+(int32_t)(q%(unsigned)size)-shift;
            int32_t x=px+(int32_t)((q>>7u)%(unsigned)(size-19));
            cv_rect(x,y,12+(int32_t)(q%8u),1,C_WHITE);
        }
        if(activity>0.28f){
            unsigned k=(visual_generation*17u+visual_rng)%81u;
            int32_t x=px+(int32_t)(k%9u)*(size/10);
            int32_t y=py+(int32_t)(k/9u)*(size/10)-shift;
            cv_rect(x,y,2,2,YELLOW);
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
    if(mi_visual_photo_bank(mode)>=0)
        mi_tile_draw_half(half,0,0,240,audio_activity);
    else if(mode==8u)mi_dreamy_dot_draw_half(half,0,0,240);
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
/* On the physical 240x240 square LCD, knobs take x=2..120 and the
 * entire right-hand width x=122..239 belongs to imagery, never captions.
 * A source-square tile fills 116x116 without stretching its aspect ratio. */
static void mi_fx_draw_half(unsigned half)
{
    const int32_t shift=(int32_t)half*120;
    const unsigned mode=mi_visual_mode();
    const unsigned fx=state.fx_slot?state.fx2:state.fx;
    cv_begin(240,120,C_BLACK);
    cv_text(4,4-shift,&FONT_S,mi_fx_name((mi_fx_t)fx),C_WHITE);
    cv_text(75,5-shift,&FONT_XS,state.fx_page?"P2":"P1",YELLOW);
    cv_text(98,5-shift,&FONT_XS,state.fx_slot?"B":"A",C_WHITE);
    cv_rect(3,28-shift,117,1,RGB(100,100,100));
    mi_fx_dial(half,3,36,0u);
    mi_fx_dial(half,63,36,1u);
    mi_fx_dial(half,3,123,2u);
    mi_fx_dial(half,63,123,3u);
    if(mi_visual_photo_bank(mode)>=0){
        mi_tile_draw_half(half,123,62,116,audio_activity);
    }else if(mode==8u){
        mi_dreamy_dot_draw_half(half,123,62,116);
    }else{
        /* Existing MIYAKO VQ source is 128x78. Keep its actual aspect. */
        mi_media_draw_scaled_half(half,123,85,116,70,mode==7u);
        mi_video_vfx_half(half,123,85,116,70,audio_activity);
    }
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
     if(mode==6u||mode==7u)mi_video_tick(&video,audio_activity,mode==7u);
     else video.eureka_previous=0.f; /* original source disabled gate */
     if(mode==8u&&(state.view==MI_VIDEO||state.view==MI_FX)){
         /* Original Dreamy DOT clock is 24 Hz; FM-1 redraw tick is 30 Hz. */
         visual_dot_phase+=0.8f; /* no non-source periodic restart */
         redraw=1;
     }
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
 else if(mi_streq(what,"visual_choice")) *got=(int)visual_choice;
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
  else if(mi_streq(what,"rec_overdub")) *got=(int)audio.recorder.overdubbing;
  else if(mi_streq(what,"media_ready")){
      uint32_t length=0u;
      *got=plat_res(&length)!=0&&length>=MI_TILE_DATA_BYTES?1:0;
  }
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
