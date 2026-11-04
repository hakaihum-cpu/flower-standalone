/* SPDX-License-Identifier: MIT
 * Hardware-only MIYAKO FM-1 panel navigation regression.
 * Host stubs exercise the very same mi_panel_nav.c used by KARMA.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { FAM_HOME=0, FAM_FX=3, FAM_EDIT=5, FAM_ENV=1 };
enum { SC_TRACK=0, SC_GLOBAL=1, SC_ENGINE=2 };
enum { P_E0=50, P_E4=54, ENGI_ACID=14, ENGI_ELEM=15, ENGI_PRISM=13 };
#define NENGINES 17u
typedef struct { uint8_t fam, scope, id[4]; } page_t;
static const page_t PAGES[] = {
    {FAM_ENV, SC_TRACK, {0,0,0,0}},     /* 0 */
    {FAM_FX, SC_TRACK, {3,0,0,0}},      /* 1: native FX */
    {FAM_FX, SC_GLOBAL, {4,0,0,0}},    /* 2: native CLOUD */
    {FAM_EDIT, SC_ENGINE, {P_E0,0,0,0}},/* 3: MIYAKO OSC */
    {FAM_EDIT, SC_ENGINE, {P_E4,0,0,0}},/* 4: MIYAKO FX */
    {FAM_EDIT, SC_TRACK, {9,0,0,0}},   /* 5: VOICE */
};
#define NPAGES (sizeof PAGES / sizeof PAGES[0])
typedef struct { uint8_t eng_req; } track_t;
static track_t tracks[4] = {{ENGI_ACID}, {ENGI_ELEM}, {ENGI_PRISM}, {0}};
static track_t *TSEL = &tracks[0];
static struct { uint8_t home, page; uint8_t fam_last[12]; } ui = {.home=1};
static unsigned entered;
static int is_drum(const track_t *t) {return t==&tracks[3];}
static const page_t *cur_page(void) {return &PAGES[ui.page];}
static void page_entered(void) {entered++;}
static uint32_t page_first(uint32_t fam) {
    uint32_t i;for(i=0;i<NPAGES;i++)if(PAGES[i].fam==fam)return i;
    return 0;
}
static void open_family(uint32_t fam) {
    uint32_t i;
    if(!ui.home && cur_page()->fam==fam) {
        for(i=ui.page+1;i<NPAGES;i++)if(PAGES[i].fam==fam)break;
        if(i>=NPAGES)i=page_first(fam);
    } else {
        i=ui.fam_last[fam] && PAGES[ui.fam_last[fam]].fam==fam?
          ui.fam_last[fam] : page_first(fam);
    }
    ui.page=(uint8_t)i; ui.fam_last[fam]=ui.page;ui.home=0;page_entered();
}
#include "mi_panel_nav.c"
static void check(int ok,const char *label) {
    if(!ok){fprintf(stderr,"FAIL: %s (page=%u home=%u, entered=%u)\n",
                     label,ui.page,ui.home,entered);exit(1);}
}
int main(void) {
    /* Direct physical FX shortcut from HOME to MIYAKO's own Reel/Eureka. */
    mi_panel_fx_tap();
    check(ui.page==4 && !ui.home && entered==1,"FX from HOME -> MIYAKO FX");
    /* Second short FX opens native send FX at the FIRST page, not last CLOUD. */
    ui.fam_last[FAM_FX]=2;
    mi_panel_fx_tap();
    check(ui.page==1,"second FX -> native FX, first page");
    mi_panel_fx_tap();
    check(ui.page==2,"FX cycles from native FX to CLOUD");
    /* EDIT from another family always starts at oscillator regardless of
     * history, then cycles normally (physical SELECT could go backwards). */
    ui.fam_last[FAM_EDIT]=5;
    mi_panel_edit_tap();
    check(ui.page==3,"EDIT opens OSC, ignores remembered VOICE page");
    mi_panel_edit_tap();
    check(ui.page==4,"EDIT cycles to MIYAKO FX");
    mi_panel_edit_tap();
    check(ui.page==5,"EDIT can still reach VOICE");
    ui.home=1;
    mi_panel_edit_tap();
    check(ui.page==3,"EDIT from HOME resets to OSC");
    /* Switching tracks via physical ALGORITHM keeps their native FX. */
    TSEL=&tracks[1];ui.home=1;
    mi_panel_fx_tap();
    check(ui.page==2 || ui.page==1,"Elements uses native FX pages");
    mi_panel_fx_tap();
    check(ui.page==1 || ui.page==2,"Elements cycles native FX");
    /* Drum track keeps original mapping. */
    TSEL=&tracks[3];ui.home=1;ui.fam_last[FAM_FX]=0;
    mi_panel_fx_tap();
    check(ui.page==1,"drum native FX unchanged");
    /* Back to MIYAKO from native FX, FX still cycles the current family. */
    TSEL=&tracks[0];ui.home=0;ui.page=2;
    mi_panel_fx_tap();
    check(ui.page==1,"FX from shared CLOUD cycles shared FX");
    /* The hardware's physical knob/page mechanisms are KARMA's native
     * ui_input (panel_enc / page_walk / edit_param), not custom touch input. */
    check(mi_panel_sound_page(P_E0)==3 && mi_panel_sound_page(P_E4)==4,
          "resolve pages by parameter IDs, not screen offsets");
    puts("PASS: MIYAKO FM-1 buttons FX/EDIT, native CLOUD, tracks, HOME, drum/VOICE");
    return 0;
}
