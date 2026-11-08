/* SPDX-License-Identifier: GPL-3.0-only
 * Independent MIYAKO FM-1 app using only fm1-base's hardware platform API.
 * This is an INCOMPLETE port. Original EFFECTS and EUREKA frames are
 * encoded at build time (16-colour lossy VQ); additionally, the six
 * user-supplied image sheets are 628 frames in eight 16-colour resource banks.
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
#define SAVED_MAGIC 0x4D493135u /* MI15: expanded controls */
#define SAVED_MAGIC_M14 0x4D493134u /* MI14 settings */
#define SAVED_MAGIC_PREV 0x4D493133u /* MI13 settings */
typedef struct {
 uint32_t magic;
 mi_state_t state;
 } saved_t;
/* Exact persisted MI13/MI14 layout: FX_COUNT was seven. */
typedef struct {
    mi_view_t view;
    uint8_t wave,width,sub,crush,fx,wet,level,preset;
    uint8_t fx_options[7][2];
    uint8_t env[4],lfo[4],arp[4],seq[4];
    int16_t global[4];
    uint8_t fx2,wet2,fx_slot,project_slot;
    int8_t octave;
    uint8_t playing,recording,save_armed;
    uint32_t save_armed_ms;
    uint8_t osc2_wave,osc2_level,osc2_detune,osc2_octave,osc_page;
    uint8_t fx_extra[7][4],fx_page,osc1_wave;
} mi_state_v14_t;
typedef struct {uint32_t magic;mi_state_v14_t state;} saved_v14_t;
typedef struct {uint32_t magic;
    uint8_t state_prefix[offsetof(mi_state_v14_t,osc2_wave)];
} saved_legacy_t;
/* Never attempt migration if platform ABI differs from historic layout. */
typedef char mi_old_save_assert[(sizeof(saved_v14_t)==104u &&
    sizeof(saved_legacy_t)==68u)?1:-1];
static mi_state_t state;
/* New MI15 defaults are retained for newly added FX, FILTER, REC and SEL. */
static void mi_migrate_old(const mi_state_v14_t *old,int extended)
{
    unsigned i,j;
    state.view=old->view;
    state.wave=old->wave;state.width=old->width;state.sub=old->sub;
    state.crush=old->crush;state.fx=old->fx;state.wet=old->wet;
    state.level=old->level;state.preset=old->preset;
    for(i=0u;i<7u;i++){
        for(j=0u;j<2u;j++)state.fx_options[i][j]=old->fx_options[i][j];
        if(extended)
            for(j=0u;j<4u;j++)state.fx_extra[i][j]=old->fx_extra[i][j];
    }
    for(i=0u;i<4u;i++){
        state.env[i]=old->env[i];state.lfo[i]=old->lfo[i];
        state.arp[i]=old->arp[i];state.seq[i]=old->seq[i];
        state.global[i]=old->global[i];
    }
    state.fx2=old->fx2;state.wet2=old->wet2;state.fx_slot=old->fx_slot;
    state.project_slot=old->project_slot;state.octave=old->octave;
    state.playing=old->playing;state.recording=old->recording;
    state.save_armed=old->save_armed;state.save_armed_ms=old->save_armed_ms;
    if(extended){
        state.osc2_wave=old->osc2_wave;state.osc2_level=old->osc2_level;
        state.osc2_detune=old->osc2_detune;state.osc2_octave=old->osc2_octave;
        state.osc_page=old->osc_page;state.fx_page=old->fx_page;
        state.osc1_wave=old->osc1_wave;
    }
}

static mi_audio_t audio;
static mi_video_t video;
static volatile float audio_activity;
static uint8_t osc_scope_last_active;
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
 /* Check actual stored bytes before loading to avoid truncated legacy data. */
 if(stored_bytes==(int)sizeof saved &&
    plat_store_load(OBJ_SETTINGS,&saved,sizeof saved)==(int)sizeof saved &&
    saved.magic==SAVED_MAGIC&&mi_state_valid(&saved.state)){
     state=saved.state;restored=1;
 }else if(stored_bytes==(int)sizeof(saved_v14_t)){
     saved_v14_t old14;
     if(plat_store_load(OBJ_SETTINGS,&old14,sizeof old14)==(int)sizeof old14){
         if(old14.magic==SAVED_MAGIC_M14){
             mi_migrate_old(&old14.state,1);
             restored=mi_state_valid(&state);
         }else if(old14.magic==SAVED_MAGIC_PREV){
             mi_migrate_old(&old14.state,0);
             restored=mi_state_valid(&state);
         }
     }
 }else if(stored_bytes==(int)sizeof(saved_legacy_t)){
     saved_legacy_t old;
     if(plat_store_load(OBJ_SETTINGS,&old,sizeof old)==(int)sizeof old&&
        old.magic==SAVED_MAGIC_PREV){
         mi_state_v14_t decoded;
         mi_memzero(&decoded,sizeof decoded);
         mi_memcopy(&decoded,old.state_prefix,sizeof old.state_prefix);
         mi_migrate_old(&decoded,0);
         restored=mi_state_valid(&state);
     }
 }
 if(!restored)mi_init(&state);
 state.save_armed=0;
 state.playing=state.recording=0;
 state.view=MI_HOME;
 state.osc_page=0u;
 osc_scope_last_active=0u;
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
     if(status==0xF8u||status==0xFAu||
        status==0xFBu||status==0xFCu){
         mi_audio_midi_clock(&audio,status);
         continue;
     }
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
         if(i==EN_SELECT&&state.view==MI_VIDEO){
             /* SEL alone uses SELECT for image banks; FX allows page navigation. */
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
/* Eight photo banks, 628 frames, rebuilt from approved original sheets at 96x96. */
/* Photo frames use the separately installed 96x96 RGB565 resource;
 * EFFECTS/EUREKA still use their independent compiled 128x78 VQ tables. */
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
        visual_rng^=visual_rng<<13;visual_rng^=visual_rng>>17;
        visual_rng^=visual_rng<<5;
        normal_visual_bank=(uint8_t)(visual_rng%10u<3u?
            visual_rng%11u:(normal_visual_bank+1u)%11u);
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
        /* 30% ignore the current photo bank and draw from all frames. */
         next=visual_rng%10u<3u?visual_rng%MI_TILE_COUNT:
              (unsigned)mi_tile_bank_first[photo_bank]+visual_rng%count;
        if(MI_TILE_COUNT>1u&&next==visual_tile)
             next=(next+1u)%MI_TILE_COUNT;
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
/* Original-source 96x96 VQ: 288 tile IDs/photo, cached for 2 LCD passes. */
/* Clip only image rectangles; FX right background must not paint the knobs. */
static void mi_image_rect(int32_t x,int32_t y,int32_t w,int32_t h,
                          int32_t clip_left,uint16_t rgb)
{
    if(x<clip_left){w-=clip_left-x;x=clip_left;}
    if(x+w>240)w=240-x;
    if(w>0&&h>0)cv_rect(x,y,w,h,rgb);
}
/* Real fixed 16 RGB565 colours are encoded in the NEW original-source
 * 96x96 VQ payload. Do not remap them by brightness (the previous buggy
 * method made every real picture look like coloured grayscale). */
static uint8_t mi_photo_pixels[MI_TILE_SIDE*MI_TILE_SIDE/2u];
static uint16_t mi_photo_palette[16];
static int16_t mi_photo_cached=-1;
static void mi_photo_decode(unsigned frame)
{
    uint32_t length=0u;
    const uint8_t *res;
    unsigned clip,ty,tx,row,col,i;
    if(frame>=MI_TILE_COUNT||mi_photo_cached==(int16_t)frame)return;
    /* FM-1_938 requires its 279936-byte photo resource. All older
     * 119520-byte resources are INCOMPATIBLE and show DOT fallback. */
    res=plat_res(&length);
    if(!res||length<MI_TILE_DATA_BYTES){
        mi_photo_cached=-1;
        return;
    }
    clip=frame/MI_TILE_VQ_FRAMES_PER_CLIP;
    if(clip>=MI_TILE_VQ_CLIPS){mi_photo_cached=-1;return;}
    for(i=0u;i<16u;i++){
        unsigned off=MI_TILE_VQ_PALETTE_OFFSET+clip*32u+i*2u;
        mi_photo_palette[i]=(uint16_t)((uint16_t)res[off]|
                                      ((uint16_t)res[off+1u]<<8u));
    }
    for(ty=0u;ty<MI_TILE_VQ_ROWS;ty++)
        for(tx=0u;tx<MI_TILE_VQ_COLS;tx++){
            unsigned id=res[MI_TILE_VQ_MAP_OFFSET+
                            frame*MI_TILE_VQ_MAP_STRIDE+
                            ty*MI_TILE_VQ_COLS+tx];
            const uint8_t *tile=res+MI_TILE_VQ_BOOK_OFFSET+
                                (clip*256u+id)*16u;
            for(row=0u;row<4u;row++)
                for(col=0u;col<4u;col++){
                    unsigned at=(ty*4u+row)*(MI_TILE_SIDE/2u)+tx*4u+col;
                    mi_photo_pixels[at]=tile[row*4u+col];
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
/* SEL K1 GLITCH / K2 TILE transform existing source pixels;
 * K3/K4 shift X/Y. No new mode, frames or decoder. */
static void mi_visual_map(unsigned *sx,unsigned *sy,unsigned width,unsigned height)
{
    unsigned tile=audio.video_effective[1],tear=audio.video_effective[0];
    if(tile){
        unsigned grid=2u+tile*6u/127u;
        unsigned cellw=(width+grid-1u)/grid;
        unsigned cellh=(height+grid-1u)/grid;
        unsigned gx=*sx/cellw,gy=*sy/cellh;
        unsigned pattern=(gx*17u+gy*23u+visual_generation)%7u;
        if(pattern==2u||pattern==5u)*sx=(*sx+cellw/2u)%width;
        if(pattern==3u)*sy=(*sy+cellh/2u)%height;
    }
    if(tear&&((*sy*23u+visual_generation*7u)&127u)<tear/3u)
        *sx=(*sx+tear/2u+visual_generation%11u)%width;
}
static void mi_dreamy_dot_pixel(unsigned half,int32_t px,int32_t py,
                               int32_t size,int32_t clip_left,
                               int x,int y,uint16_t color)
{
    int32_t top,bottom,left,right;
    if(x<0||y<0||x>=65||y>=65)return;
    {
        unsigned xx=(unsigned)x,yy=(unsigned)y;
        mi_visual_map(&xx,&yy,65u,65u);
        x=(int)xx;y=(int)yy;
    }
    top=py+(int32_t)((unsigned)y*(unsigned)size/65u)-(int32_t)half*120;
    bottom=py+(int32_t)((unsigned)(y+1)*(unsigned)size/65u)-(int32_t)half*120;
    if(bottom<=0||top>=120)return;
    if(top<0)top=0;
    if(bottom>120)bottom=120;
    left=px+(int32_t)((unsigned)x*(unsigned)size/65u);
    right=px+(int32_t)((unsigned)(x+1)*(unsigned)size/65u);
    if(left<clip_left)left=clip_left;
    if(right>240)right=240;
    if(right>left&&bottom>top)cv_rect(left,top,right-left,bottom-top,color);
}
static void mi_dreamy_dot_draw_half(unsigned half,int32_t px,int32_t py,
                                    int32_t size,int32_t clip_left)
{
    unsigned x,y,i;
    const float sadness=0.5f,fragility=0.4f,nostalgia=0.3f,melancholy=0.3f;
    const uint16_t amber=RGB(255,216,54),ink=RGB(255,219,70);
    const uint16_t dim=RGB(128,103,30);
    mi_dreamy_dot_unpack();
    if(!mi_dreamy_dot_ready)return;
    px+=((int32_t)audio.video_effective[2]-64)/4;
    py+=((int32_t)audio.video_effective[3]-64)/4;
    {   /* Clip the square's background to the active 120-row LCD half. */
        int32_t top=py-(int32_t)half*120,bottom=top+size;
        if(top<0)top=0;
        if(bottom>120)bottom=120;
        if(bottom>top)
            mi_image_rect(px,top,size,bottom-top,clip_left,RGB(17,16,11));
    }
    for(y=0u;y<MI_DREAMY_DOT_H;y++)
        for(x=0u;x<MI_DREAMY_DOT_W;x++){
            unsigned k=y*MI_DREAMY_DOT_W+x;
            unsigned pixel=mi_dreamy_dot_packed[k>>2u];
            unsigned col=(pixel>>(6u-(k&3u)*2u))&3u;
            if(col==1u)mi_dreamy_dot_pixel(half,px,py,size,clip_left,(int)x+2,
                                          (int)y+9,RGB(150,114,18));
            else if(col==2u)mi_dreamy_dot_pixel(half,px,py,size,clip_left,(int)x+2,
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
            mi_dreamy_dot_pixel(half,px,py,size,clip_left,xdot,ydot-(int)t,
                               t==0u?ink:dim);
        if(fragility>0.05f && cycle>0.25f+0.5f*(1.f-fragility)){
            for(p=0u;p<4u;p++){
                int spread=(int)(fragility*(float)(p+1u)*2.f);
                mi_dreamy_dot_pixel(half,px,py,size,clip_left,
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
static unsigned mi_photo_visual_index(unsigned sy,unsigned sx)
{
    mi_visual_map(&sx,&sy,MI_TILE_SIDE,MI_TILE_SIDE);
    return mi_photo_index(sy,sx);
}
static void mi_tile_draw_half(unsigned half,int32_t px,int32_t py,
                              int32_t size,int32_t clip_left,float activity)
{
    const int32_t shift=(int32_t)half*120;
    uint32_t resource_length=0u;
    unsigned sy;
    (void)activity;
    px+=((int32_t)audio.video_effective[2]-64)/4;
    py+=((int32_t)audio.video_effective[3]-64)/4;
    if(visual_tile>=MI_TILE_COUNT||
       !plat_res(&resource_length)||
       resource_length<MI_TILE_DATA_BYTES){
        mi_photo_cached=-1;
        mi_dreamy_dot_draw_half(half,px,py,size,clip_left);
        return;
    }
    mi_photo_decode((unsigned)visual_tile);
    if(mi_photo_cached!=(int16_t)visual_tile){
        mi_dreamy_dot_draw_half(half,px,py,size,clip_left);
        return;
    }
    /* Original-source 96x96 pixel map: no artificial tearing or stripes. */
    for(sy=0u;sy<MI_TILE_SIDE;sy++){
        int32_t top=py+(int32_t)(sy*(unsigned)size/MI_TILE_SIDE)-shift;
        int32_t bottom=py+(int32_t)((sy+1u)*(unsigned)size/MI_TILE_SIDE)-shift;
        unsigned sx=0u;
        if(bottom<=0||top>=120)continue;
        while(sx<MI_TILE_SIDE){
            unsigned first=sx,idx=mi_photo_visual_index(sy,sx);
            ++sx;
            while(sx<MI_TILE_SIDE&&mi_photo_visual_index(sy,sx)==idx)++sx;
            if(idx){
                int32_t x0=px+(int32_t)(first*(unsigned)size/MI_TILE_SIDE);
                int32_t x1=px+(int32_t)(sx*(unsigned)size/MI_TILE_SIDE);
                if(x1>x0)mi_image_rect(x0,top,x1-x0,bottom-top,
                                       clip_left,mi_photo_palette[idx]);
            }
        }
    }
}
/* Original 144 + 576 VQ frame banks remain available alongside photo tiles.
 * The original encoding is 128x78; its aspect is not distorted here. */
static unsigned mi_media_visual_index(unsigned sy,unsigned sx)
{
    unsigned at;
    mi_visual_map(&sx,&sy,MI_EFFECTS_ASSET_W,MI_EFFECTS_ASSET_H);
    at=sy*(MI_EFFECTS_ASSET_W/2u)+(sx>>1u);
    return (sx&1u)?(mi_media_pixels[at]&15u):
                    (mi_media_pixels[at]>>4u);
}
static void mi_media_draw_scaled_half(unsigned half,int32_t px,int32_t py,
                                      int32_t width,int32_t height,
                                      int32_t clip_left,int eureka)
{
    unsigned sy;
    int frame=eureka?video.eureka_frame:video.effects_frame;
    int32_t shift=(int32_t)half*120;
    if(frame<0)return;
    px+=((int32_t)audio.video_effective[2]-64)/4;
    py+=((int32_t)audio.video_effective[3]-64)/4;
    mi_media_decode((unsigned)frame,(unsigned)eureka);
    for(sy=0;sy<MI_EFFECTS_ASSET_H;sy++){
        int32_t y0=py+(int32_t)(sy*(unsigned)height/MI_EFFECTS_ASSET_H)-shift;
        int32_t y1=py+(int32_t)((sy+1u)*(unsigned)height/MI_EFFECTS_ASSET_H)-shift;
        unsigned x=0;
        if(y1<=0||y0>=120)continue;
        while(x<MI_EFFECTS_ASSET_W){
            unsigned idx=mi_media_visual_index(sy,x);
            unsigned first=x++,last;
            if(idx==0u)continue;
            while(x<MI_EFFECTS_ASSET_W){
                if(mi_media_visual_index(sy,x)!=idx)break;
                ++x;
            }
            last=x;
            {
                int32_t dx0=px+(int32_t)(first*(unsigned)width/MI_EFFECTS_ASSET_W);
                int32_t dx1=px+(int32_t)(last*(unsigned)width/MI_EFFECTS_ASSET_W);
                if(dx1>dx0)
                    mi_image_rect(dx0,y0,dx1-dx0,y1-y0,
                                  clip_left,mi_media_palette[idx]);
            }
        }
    }
}
static void mi_media_draw_half(unsigned half)
{
    unsigned mode=mi_visual_mode();
    if(mi_visual_photo_bank(mode)>=0)
        mi_tile_draw_half(half,0,0,240,0,audio_activity);
    else if(mode==8u)mi_dreamy_dot_draw_half(half,0,0,240,0);
    else{
        mi_media_draw_scaled_half(half,0,47,240,146,0,mode==7u);
    }
}
/* Intentionally limited by the FM-1's physical 240x240 LCD: 2x2 compact
 * hardware knobs on the left; a live square source tile on the right. */
/* 240px-wide real video above the four compact K1-K4 readouts.
 * EUREKA, DREAMY and all user-photo banks use the same enlarged FX area. */
static void mi_fx_dial(unsigned half,int32_t x,int32_t y,unsigned knob)
{
    static const int8_t cx[12]={0,5,8,10,8,5,0,-5,-8,-10,-8,-5};
    static const int8_t cy[12]={-10,-8,-5,0,5,8,10,8,5,0,-5,-8};
    static const int8_t hx[11]={-6,-8,-8,-6,-3,0,3,6,8,8,6};
    static const int8_t hy[11]={6,4,1,-3,-7,-8,-7,-3,1,4,6};
    char val[16];
    unsigned i,fx=state.fx_slot?state.fx2:state.fx;
    const char *label=mi_param_name(&state,knob);
    int32_t v=mi_param_value(&state,knob);
    unsigned dial=(unsigned)(v<0?0:v);
    int32_t oy=y-(int32_t)half*120,center=x+29,mid=oy+13;
    if(knob==0u&&!state.fx_page){
        textcpy(val,mi_fx_name((mi_fx_t)fx),sizeof val);
        dial=fx*127u/(MI_FX_COUNT-1u);
    }else{
        number(v,val);
        if(state.fx_page&&fx<MI_FX_COUNT){
            if(knob==0u)dial=state.fx_extra[fx][0];
            if(knob==1u)dial=state.fx_extra[fx][1]>=64u?127u:0u;
        }
    }
    cv_rect(x,oy,58,57,C_WHITE);
    cv_rect(x+1,oy+1,56,55,C_BLACK);
    for(i=0;i<12u;i++){
        unsigned next=(i+1u)%12u;
        cv_line(center+cx[i],mid+cy[i],center+cx[next],mid+cy[next],C_WHITE);
    }
    i=dial*10u/127u;
    cv_line(center,mid,center+hx[i],mid+hy[i],YELLOW);
    cv_text(x+3,oy+27,&FONT_XS,label,C_WHITE);
    cv_text(x+3,oy+42,&FONT_XS,val,YELLOW);
}
static void mi_fx_draw_half(unsigned half)
{
    const int32_t shift=(int32_t)half*120;
    const unsigned mode=mi_visual_mode();
    const unsigned fx=state.fx_slot?state.fx2:state.fx;
    cv_begin(240,120,C_BLACK);
    cv_text(3,3-shift,&FONT_XS,mi_fx_name((mi_fx_t)fx),C_WHITE);
    cv_text(67,3-shift,&FONT_XS,state.fx_page?"P2":"P1",YELLOW);
    cv_text(87,3-shift,&FONT_XS,state.fx_slot?"FX B":"FX A",C_WHITE);
    cv_text(129,3-shift,&FONT_XS,
            (fx==MI_FX_DREAMY||fx==MI_FX_EUREKA)?
            "PRESET:P1/2":"FX:A/B",GREY);
    if(mi_visual_photo_bank(mode)>=0){
        mi_tile_draw_half(half,40,18,160,0,audio_activity);
    }else if(mode==8u){
        mi_dreamy_dot_draw_half(half,40,18,160,0);
    }else{
        mi_media_draw_scaled_half(half,0,25,240,146,0,mode==7u);
    }
    mi_fx_dial(half,1,183,0u);
    mi_fx_dial(half,61,183,1u);
    mi_fx_dial(half,121,183,2u);
    mi_fx_dial(half,181,183,3u);
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
            state.view==MI_SEQ?(audio.recorder.clock_active?"MIDI 2BAR":
                state.rec_bars==4u?"REC 4BAR":"REC 2BAR"):
            state.view==MI_OSC?(state.osc_page==2u?"FILTER":
                state.wave==MI_WAVE_FMDRUM?"FMDRUM":
                state.osc_page==1u?"OSC2":"OSC1"):
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
        /* Live pre-FX OSC audio waveform (SINE/SAW/SQUARE/PULSE). */
        {
            uint32_t head=audio.osc_scope_head;
            unsigned stride=audio.osc_scope_stride;
            if(stride<1u||stride>4u)stride=1u;
            cv_line(8,76,232,76,DIM);
            for(i=0;i<224u;i++){
                uint32_t idx=(head-1u-(223u-i)*stride)&
                             (MI_OSC_SCOPE_SAMPLES-1u);
                int32_t y=76-(int32_t)audio.osc_scope[idx]*24/127;
                if(i)cv_line((int32_t)i+7,prior,
                             (int32_t)i+8,y,YELLOW);
                prior=y;
            }
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
        if(((state.view==MI_OSC&&state.osc_page<2u&&
                 state.wave!=MI_WAVE_FMDRUM)||state.view==MI_HOME)&&i==0u)
            textcpy(val,mi_wave_name(state.view==MI_HOME?state.wave:
                   (state.osc_page==1u?state.osc2_wave:
                    state.wave==MI_WAVE_MULTI?state.osc1_wave:state.wave)),
                   sizeof val);
        else if(state.view==MI_HOME&&state.wave==MI_WAVE_MULTI&&i==1u)
            textcpy(val,state.osc_page?"OSC2":"OSC1",sizeof val);
        else if(state.view==MI_LFO&&i==1u)
            textcpy(val,mi_lfo_wave_name(state.lfo[1]),sizeof val);
        else if(state.view==MI_LFO&&i==3u)
             textcpy(val,mi_lfo_dest_name(state.lfo[3]),sizeof val);
        else if(state.view==MI_ARP&&i==0u)
            textcpy(val,mi_arp_mode_name(state.arp[0]),sizeof val);
        else if(state.view==MI_ARP&&i==1u){
            static const char *const rates[]={"1/4","1/8","1/16","1/32"};
            textcpy(val,rates[state.arp[1]<=3u?state.arp[1]:3u],sizeof val);
        }else if(state.view==MI_SEQ&&i==2u){
            static const int semitone[7]={-12,-7,-5,0,5,7,12};
            number(semitone[state.seq[2]<=6u?state.seq[2]:3u],val);
        }else number(mi_param_value(&state,i),val);
        cv_text(x,49,(state.view==MI_LFO&&i==3u)?&FONT_XS:&FONT_S,
                 val,YELLOW);
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
     if((state.view==MI_VIDEO||state.view==MI_FX)&&
        state.lfo[2]&&state.lfo[3]>=24u&&state.lfo[3]<=27u)
         redraw=1;
     if(state.view==MI_SEQ && (audio.recorder.recording||
                                audio.recorder.playing||audio.recorder.armed))
         redraw=1;
     if(state.view==MI_OSC||state.view==MI_HOME){
         uint8_t active=audio.osc_scope_active;
         if(active||osc_scope_last_active)redraw=1;
         osc_scope_last_active=active;
     }else osc_scope_last_active=0u;
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
 else if(mi_streq(what,"fx_page")) *got=(int)state.fx_page;
 else if(mi_streq(what,"wave")) *got=(int)state.wave;
 else if(mi_streq(what,"osc_page")) *got=(int)state.osc_page;
 else if(mi_streq(what,"osc1_wave")) *got=(int)state.osc1_wave;
 else if(mi_streq(what,"osc2_wave")) *got=(int)state.osc2_wave;
 else if(mi_streq(what,"osc_scope_active")) *got=(int)audio.osc_scope_active;
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
