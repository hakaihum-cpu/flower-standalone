/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO FM-1 native KARMA engine.
 * Plaits, Elements and Clouds remain KARMA's separate native DSP paths.
 * Native mono fixed-point engine, no Android/JUCE allocation.
 */
#define MIYAKO_RING 1024u
#define MIYAKO_MASK (MIYAKO_RING - 1u)
/* Preserve EUREKA's original 256..3304-sample timing while storing one
 * sample every four; replay linearly interpolates the captured points. */
#define MIYAKO_EUREKA_DECIM 4u
typedef struct {
    int16_t ring[MIYAKO_RING];
    uint32_t wr, valid, phase, segment, wow, flutter;
    uint32_t tail_left; /* remaining EUREKA replay samples after the last input */
    int32_t wet;                /* Q15, smoothed */
    uint8_t mode, tail_block; /* finish processing the last replay block */
} miyako_slot_t;
/* Fail compilation before the linker if a later change silently expands the
 * FM-1 EUREKA buffer or breaks the power-of-two ring-mask assumption.
 * The target linker independently checks the remaining .pool headroom. */
typedef char mi_ring_power_of_two[(MIYAKO_RING && !(MIYAKO_RING & (MIYAKO_RING - 1u))) ? 1 : -1];
typedef char mi_slot_max_bytes[(sizeof(miyako_slot_t) <= 2084u) ? 1 : -1];
typedef char mi_eureka_max_segment_fits[((256u + 127u * 24u + MIYAKO_EUREKA_DECIM - 1u)
                                          / MIYAKO_EUREKA_DECIM <= MIYAKO_RING) ? 1 : -1];

static miyako_slot_t *miyako_slot(track_t *t)
{
    if (t != &trk[0])
        return 0;
    return (miyako_slot_t *)eng_arena_need(t, ENGI_ACID, sizeof(miyako_slot_t));
}

static void miyako_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = v->ph[1] = 0u;
}

static void miyako_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t i, p = v->ph[0], subph = v->ph[1];
    uint32_t wave = (uint32_t)clamp(t->p[P_E0], 0, 3);
    uint32_t pw = (32u + ((uint32_t)clamp(t->p[P_E1], 0, 127) * 192u >> 7)) << 24;
    int32_t sub = clamp(t->p[P_E2], 0, 127);
    uint32_t crush = (uint32_t)clamp(t->p[P_E3], 0, 127) >> 3;
    for (i = 0; i < n; ++i) {
        int32_t y;
        switch (wave) {
        case 0u: y = sine_i(p); break;
        case 1u: y = osc_saw(p, m->inc); break;
        case 2u: y = osc_pulse(p, m->inc, 0x80000000u); break;
        default: y = osc_pulse(p, m->inc, pw); break;
        }
        y = (y * (128 - sub) + sine_i(subph) * sub) >> 7;
        y = (y >> crush) * (1 << crush); /* avoid negative signed left shift */
        /* Respect KARMA's VOICE_FS=24000 convention: doubling this value
         * can clip a single MIYAKO voice before the effect stage. */
        out[i] += voice_amp(y, m, i);
        p += m->inc;
        subph += m->inc >> 1;
    }
    v->ph[0] = p;
    v->ph[1] = subph;
}

static void miyako_post(track_t *t, int32_t *out, uint32_t n)
{
    miyako_slot_t *s = miyako_slot(t);
    uint32_t request = (uint32_t)clamp(t->p[P_E4], 0, 2), mode, i;
    int32_t target = clamp(t->p[P_E5], 0, 127) * 258; /* 0..32766 */
    uint32_t heard = 0u;
    if (!s)
        return;
    /* Switching between REEL and EUREKA must fade the old effect OUT
     * before changing buffers, then fade the new one IN. */
    if (s->mode != request && s->wet == 0) {
        s->mode = (uint8_t)request;
        s->wr = s->valid = s->phase = s->segment = s->tail_left = 0u;
        s->tail_block = 0u;
    }
    if (s->mode != request)
        target = 0;
    mode = s->mode;
    if (!mode)
        return;
    for (i = 0; i < n; ++i) {
        int32_t original = out[i];
        int32_t dry = clamp(original, -32767, 32767), effected = dry;
        if (dry) heard = 1u;
        if (mode == 1u) { /* REEL: interpolated moving tape head + gentle drive */
            uint32_t delay, frac;
            int32_t a, b, delay_q8;
            s->wow += 58435u;
            s->flutter += 779000u;
            /* Keep eight fractional bits: an integer tap jumps as the head moves. */
            a = (sine_i(s->wow) * clamp(t->p[P_E6], 0, 127)) >> 9;
            b = (sine_i(s->flutter) * clamp(t->p[P_E7], 0, 127)) >> 11;
            delay_q8 = clamp((256 << 8) + a + b, 64 << 8, 960 << 8);
            delay = (uint32_t)delay_q8 >> 8;
            frac = (uint32_t)delay_q8 & 255u;
            if (s->valid > delay + 1u) {
                int32_t v0 = s->ring[(s->wr - delay) & MIYAKO_MASK];
                int32_t v1 = s->ring[(s->wr - delay - 1u) & MIYAKO_MASK];
                int32_t interpolated = v0 + (((v1 - v0) * (int32_t)frac) >> 8);
                int32_t delayed = softclip(interpolated + (interpolated >> 2));
                int32_t ramp = (int32_t)(s->valid - delay - 1u);
                if (ramp > 64) ramp = 64;
                effected = dry + (((delayed - dry) * ramp) >> 6);
            }
            s->ring[s->wr & MIYAKO_MASK] = (int16_t)dry;
            ++s->wr;
            if (s->valid < MIYAKO_RING)
                ++s->valid;
        } else { /* EUREKA: short capture / repeat, windowed to avoid clicks */
            uint32_t segment, repeats, cycle, at, edge;
            repeats = 1u + ((uint32_t)clamp(t->p[P_E7], 0, 127) >> 5);
            if (s->segment == 0u)
                s->segment = 256u + (uint32_t)clamp(t->p[P_E6], 0, 127) * 24u;
            segment = s->segment;
            cycle = segment * (repeats + 1u);
            if (s->phase >= cycle) {
                /* Latch a new slice length exactly when the next recording
                 * begins, not halfway through a playback repeat. */
                s->phase = 0u;
                s->segment = 256u + (uint32_t)clamp(t->p[P_E6], 0, 127) * 24u;
                segment = s->segment;
            }
            at = s->phase % segment;
            if (s->phase < segment) {
                if ((at & (MIYAKO_EUREKA_DECIM - 1u)) == 0u)
                    s->ring[at / MIYAKO_EUREKA_DECIM] = (int16_t)dry;
            } else {
                uint32_t idx = at / MIYAKO_EUREKA_DECIM;
                uint32_t frac = at & (MIYAKO_EUREKA_DECIM - 1u);
                int32_t s0 = s->ring[idx];
                int32_t s1 = ((idx + 1u) * MIYAKO_EUREKA_DECIM < segment)
                           ? s->ring[idx + 1u] : s0;
                effected = s0 + (((s1 - s0) * (int32_t)frac) >> 2);
            }
            edge = at < segment - at - 1u ? at : segment - at - 1u;
            if (s->phase >= segment && edge < 32u)
                effected = dry + (((effected - dry) * (int32_t)edge) >> 5);
            ++s->phase;
        }
        s->wet += clamp(target - s->wet, -512, 512);
        /* With both inputs in [-32767,32767], signed 32-bit multiplication fits. */
        out[i] = original + (((effected - dry) * s->wet) >> 15);
    }
    if (mode == 2u && request == 2u && s->wet) {
        /* At most one complete capture/repeat cycle survives note-off.
         * The mixer consults this countdown before skipping an idle part. */
        uint32_t repeats = 1u + ((uint32_t)clamp(t->p[P_E7], 0, 127) >> 5);
        uint32_t cycle = s->segment * (repeats + 1u);
        s->tail_block = (uint8_t)(s->tail_left != 0u);
        if (heard) s->tail_left = cycle;
        else s->tail_left = s->tail_left > n ? s->tail_left - n : 0u;
        if (s->tail_left) s->tail_block = 1u;
    } else {
        s->tail_left = 0u;
        s->tail_block = 0u;
    }
    if (s->wet == 0 && s->mode != request) {
        /* Next block initializes the requested mode at zero wet. */
        s->mode = (uint8_t)request;
        s->wr = s->valid = s->phase = s->segment = s->tail_left = 0u;
        s->tail_block = 0u;
    }
}

/* Use the established Plaits FM / 6OP A/B/C engines on track 3.
 * No duplicate homebrew FM algorithm lives in MIYAKO track 1. */
/* Audio mixer must not cut EUREKA off as soon as the last key releases.
 * Only the active MIYAKO engine on physical track 1 may claim its arena. */
static int miyako_tail_active(track_t *t)
{
    miyako_slot_t *s;
    if (t != &trk[0] || t->engine != ENGI_ACID) return 0;
    s = miyako_slot(t);
    return s && s->mode == 2u && s->wet && (s->tail_left || s->tail_block);
}

static const char *const MY_WAVES[] = {"SINE", "SAW", "SQR", "PULSE"};
static const char *const MY_FX[] = {"OFF", "REEL", "EUREKA"};
/* Labels come from actual fixed-point DSP paths, not from generic
 * DEPTH/RATE placeholders. KARMA's parameter band fits <=5 glyphs. */
static const char *const MI_REPS[] = {"1", "2", "3", "4", 0};
static const param_desc_t MI_PW_STORED = {"PW*", F_PCT, 0, 127, 64, 0, 0};
static const param_desc_t MI_WOW = {"WOW", F_PCT, 0, 127, 64, 0, 0};
static const param_desc_t MI_FLTR = {"FLTR", F_PCT, 0, 127, 64, 0, 0};
static const param_desc_t MI_SIZE = {"SIZE", F_PCT, 0, 127, 64, 0, 0};
static const param_desc_t MI_REPS_DESC = {"REPS", F_INT, 0, 127, 64, MI_REPS, 0};
static const param_desc_t *miyako_desc(const track_t *t, uint32_t k)
{
    uint32_t mode = (uint32_t)clamp(t->p[P_E4], 0, 2);
    if (k == 1u && t->p[P_E0] != 3) return &MI_PW_STORED;
    if (mode == 1u) {
        if (k == 6u) return &MI_WOW;
        if (k == 7u) return &MI_FLTR;
    }
    if (mode == 2u) {
        if (k == 6u) return &MI_SIZE;
        if (k == 7u) return &MI_REPS_DESC;
    }
    return 0;
}
static const preset_t MIYAKO_PRESETS[] = {
    {"SINE",   {0, 64,  0,  0, 0,  0, 64, 64}, { 4, 65, 115, 45}, 0, 0, FX(0, 0, 0, 8)},
    {"SAW",    {1, 64,  0,  0, 0,  0, 64, 64}, { 4, 65, 115, 45}, 0, 0, FX(0, 0, 0, 8)},
    {"SQUARE", {2, 64,  0,  0, 0,  0, 64, 64}, { 3, 55, 110, 40}, 0, 0, FX(0, 0, 0, 8)},
    {"PULSE",  {3, 36, 32,  0, 0,  0, 64, 64}, { 3, 55, 110, 40}, 0, 0, FX(0, 0, 0, 8)},
    {"REEL",   {0, 64,  0,  0, 1, 80, 48, 45}, { 4, 60, 115, 45}, 0, 0, FX(0, 0, 0, 8)},
    {"EUREKA", {1, 64,  0,  0, 2, 95, 64, 80}, { 1, 55, 115, 45}, 0, 0, FX(0, 0, 0, 8)},
};
static const engine_t ENG_MIYAKO = {
    .name = "MIYAKO",
    .page_title = {"OSC", "FX"},
    .edit = {
        {"WAVE", F_ENUM, 0, 3, 0, MY_WAVES, 0},
        {"WIDTH", F_PCT, 0, 127, 64, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"CRUSH", F_PCT, 0, 127, 0, 0, 0},
        {"MODE", F_ENUM, 0, 2, 0, MY_FX, 0},
        {"WET", F_PCT, 0, 127, 0, 0, 0},
        {"DEPTH", F_PCT, 0, 127, 64, 0, 0},
        {"RATE", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = MIYAKO_PRESETS,
    .npresets = NELEM(MIYAKO_PRESETS),
    .note_on = miyako_note_on,
    .render = miyako_render,
    .post = miyako_post,
    .desc = miyako_desc,
    .fil_page = -1,
    .color = 0xFFE0, /* MIYAKO yellow RGB565 */
    .macro = {P_E0, P_E4, P_E5, P_E6},
    .poly = 4,
};
