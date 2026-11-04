/* SPDX-License-Identifier: MIT
 * Host-only fixed-point MIYAKO smoke checks.
 * Uses small stand-ins for the KARMA host; not a substitute for target build.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define P_E0 0
#define P_E1 1
#define P_E2 2
#define P_E3 3
#define P_E4 4
#define P_E5 5
#define P_E6 6
#define P_E7 7
#define ENGI_ACID 14u
/* Exact KARMA core.h parameter format ABI (pinned db3cda79).
 * Do not invent host-only numeric values: they must match the target. */
enum {
    F_INT, F_PCT, F_BIPCT, F_TIME, F_LFOHZ, F_CUTOFF, F_DB, F_SEMI,
    F_ENUM, F_BPM, F_NOTE, F_ONOFF, F_OCT, F_STEPS, F_SWING, F_FILT
};
typedef char host_fmt_int_is_zero[(F_INT == 0) ? 1 : -1];
typedef char host_fmt_pct_is_one[(F_PCT == 1) ? 1 : -1];
typedef char host_fmt_enum_is_eight[(F_ENUM == 8) ? 1 : -1];
#define NVOICE 8
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define FX(a,b,c,d) .fx={(a)+1,(b)+1,(c)+1,(d)+1}

typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min,max,def;
    const char *const *names;
    const char *unit;
} param_desc_t;
typedef struct {
    uint32_t ph[3];
    uint8_t vel;
} voice_t;
typedef struct {
    uint32_t inc;
    int32_t amp0,amp1,pitch16,cutoff,shape,envq15,fine;
} vmod_t;
typedef struct {
    const char *name;
    int8_t e[8];
    uint8_t env[4];
    int8_t fenv;
    uint8_t mono,fx[4];
} preset_t;
struct track {
    int16_t p[8];
    voice_t v[NVOICE];
    uint8_t engine;
};
typedef struct track track_t;
typedef struct {
    const char *name;
    const char *page_title[2];
    param_desc_t edit[8];
    const preset_t *presets;
    uint8_t npresets;
    int8_t fil_page;
    void (*note_on)(track_t *,voice_t *);
    void (*render)(track_t *,voice_t *,int32_t *,uint32_t,const vmod_t *);
    uint16_t color;
    uint8_t macro[4];
    uint8_t poly;
    const param_desc_t *(*desc)(const track_t *,uint32_t);
    void (*post)(track_t *,int32_t *,uint32_t);
} engine_t;

static track_t trk[3];
static void *eng_arena_need(const track_t *,uint32_t,uint32_t);
static inline int32_t clamp(int32_t x,int32_t a,int32_t b) { return x<a?a:x>b?b:x; }
static int32_t sine_i(uint32_t ph) {
    double angle=(double)ph*(6.2831853071795864769 / 4294967296.0);
    return (int32_t)(sin(angle)*32767.0);
}
static int32_t osc_saw(uint32_t ph,uint32_t inc) {
    (void)inc;return (int32_t)(ph>>16)-32768;
}
static int32_t osc_pulse(uint32_t ph,uint32_t inc,uint32_t width) {
    (void)inc;return ph<width?24000:-24000;
}
static int32_t softclip(int32_t x) {
    return clamp(x,-32767,32767);
}
static int32_t voice_amp(int32_t s,const vmod_t *m,uint32_t i) {
    (void)i;return s*m->amp0/32768;
}
#include "eng_miyako.c"
static unsigned char host_arena[16384];
static void *eng_arena_need(const track_t *t,uint32_t id,uint32_t n) {
    if (t!=&trk[0] || id!=ENGI_ACID || n>sizeof(host_arena))
        return 0;
    return host_arena;
}
static void require(int ok,const char *name) {
    if (!ok) {fprintf(stderr,"FAIL: %s\n",name);exit(1);}
}
int main(void) {
    vmod_t m={.inc=42852281u,.amp0=20000,.amp1=20000};
    track_t *t=&trk[0];
    t->engine = ENGI_ACID;
    int32_t b[32]={0};
    uint32_t w,i,loop;
    unsigned long nonzero=0;
    t->p[P_E1]=64;
    t->p[P_E6]=30;
    t->p[P_E7]=64;
    for (w=0; w<4; w++) {
        t->p[P_E0]=(int16_t)w;
        t->p[P_E4]=0;
        miyako_note_on(t,&t->v[0]);
        for (loop=0;loop<40;loop++) {
            memset(b,0,sizeof(b));
            miyako_render(t,&t->v[0],b,32,&m);
            for (i=0;i<32;i++) {
                require(b[i] > -200000 && b[i] < 200000,"waveform bound");
                nonzero+=(b[i]!=0);
            }
        }
    }
    require(nonzero>3000,"all four basic oscillators produce signal");
    for (w=1;w<=2;w++) {
        t->p[P_E4]=(int16_t)w;
        t->p[P_E5]=110;
        for (loop=0;loop<900;loop++) {
            memset(b,0,sizeof(b));
            miyako_render(t,&t->v[0],b,32,&m);
            miyako_post(t,b,32);
            for (i=0;i<32;i++)
                require(b[i] > -200000 && b[i] < 200000,"FX sample bound");
        }
    }
    t->p[P_E4]=0;
    for (loop=0;loop<4;loop++) {
        memset(b,0,sizeof(b));
        miyako_render(t,&t->v[0],b,32,&m);
        miyako_post(t,b,32);
    }
    require(((miyako_slot_t *)host_arena)->mode == 0,"bypass finishes with mode off");
    {
        miyako_slot_t *st = (miyako_slot_t *)host_arena;
        /* A changed DEPTH must take effect at the NEXT record cycle, not
         * instantly (mid-grain) or one extra cycle late. */
        memset(st, 0, sizeof(*st));
        st->mode = 2;
        st->wet = 32766;
        st->segment = 256u;
        st->phase = 511u;
        t->p[P_E4] = 2;
        t->p[P_E5] = 110;
        t->p[P_E6] = 20;
        t->p[P_E7] = 0;
        b[0] = 1000;
        miyako_post(t, b, 1);
        require(st->segment == 256u && st->phase == 512u, "old segment intact to loop end");
        miyako_post(t, b, 1);
        require(st->segment == 736u && st->phase == 1u, "new segment starts next capture");
    }
    {
        miyako_slot_t *st = (miyako_slot_t *)host_arena;
        uint32_t q;
        memset(st, 0, sizeof(*st));
        t->p[P_E4] = 2; /* EUREKA */
        t->p[P_E5] = 127;
        t->p[P_E6] = 0; /* 256-sample slice */
        t->p[P_E7] = 0; /* one repeat */
        b[0] = 12000;
        miyako_post(t, b, 1);
        require(miyako_tail_active(t), "Eureka tail persists at note off");
        require(st->tail_left == 512u, "one full record+repeat cycle retained");
        for (q = 0; q < 511u; ++q) {
            b[0] = 0;
            miyako_post(t, b, 1);
        }
        require(st->tail_left == 1u && miyako_tail_active(t),
                "tail persists through final replay sample");
        b[0] = 0;
        miyako_post(t, b, 1);
        require(st->tail_left == 0u && miyako_tail_active(t),
                "last replay block must reach mixer even when countdown hits zero");
        b[0] = 0;
        miyako_post(t, b, 1);
        require(st->tail_left == 0u && !miyako_tail_active(t),
                "next silent block releases the mixer without indefinite CPU load");
        t->engine = 13u; /* Plaits is an independent engine on track 3. */
        require(!miyako_tail_active(t), "other engines never claim MIYAKO arena");
        t->engine = ENGI_ACID;
    }
    require(ENG_MIYAKO.npresets==6,"six factory presets; FM uses Plaits");
    require(ENG_MIYAKO.post==miyako_post,"real post callback");
    require(ENG_MIYAKO.desc==miyako_desc,"actual DSP-driven parameter descriptors");
    t->p[P_E0]=0;
    t->p[P_E4]=0;
    require(miyako_desc(t,1)==&MI_PW_STORED,"width is inactive on Sine/Saw/Square");
    t->p[P_E0]=3;
    require(miyako_desc(t,1)==0,"width enabled only for Pulse");
    t->p[P_E4]=1;
    require(miyako_desc(t,6)==&MI_WOW && miyako_desc(t,7)==&MI_FLTR,
            "Reel knobs map to WOW and Flutter amplitude, not rate");
    t->p[P_E4]=2;
    require(miyako_desc(t,6)==&MI_SIZE && miyako_desc(t,7)==&MI_REPS_DESC,
            "Eureka knobs map to record SIZE and REPEATS");
    require(MI_REPS_DESC.names[0][0]=='1' && MI_REPS_DESC.names[3][0]=='4',
            "Eureka repeats names cover DSP 1..4");
    t->p[P_E4]=0;
    require(miyako_desc(t,6)==0 && miyako_desc(t,7)==0,"Off-mode default stored controls");
    printf("PASS: MIYAKO 4 waveforms + Reel + Eureka + bypass, %lu nonzero samples\n",nonzero);
    return 0;
}
