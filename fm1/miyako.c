/* SPDX-License-Identifier: MIT
 * MIYAKO FM-1: first portable engine for Felucca ABI 1.
 * No JUCE, heap allocation, floating point or 64-bit arithmetic.
 * This is not a port of Mutable Instruments Plaits/Elements/Clouds.
 */
#include "fe_tables.h"
#include "felucca_plugin.h"
#include "dsp.c"

#define MY_LINE 4096u
#define MY_MASK (MY_LINE - 1u)
typedef struct {
    int16_t line[MY_LINE];       /* reused by Reel and Eureka, 8 KiB per part */
    uint32_t wr, valid, wow, flutter;
    uint32_t phase, length;
    int32_t mix;               /* Q15 smoothed wet fraction */
    uint8_t mode;
} miyako_mem_t;
typedef char my_mem_within_felucca_limit[(sizeof(miyako_mem_t) <= FE_MEM_MAX) ? 1 : -1];

static inline int32_t my_limit(int32_t x)
{
    return x > 32767 ? 32767 : x < -32768 ? -32768 : x;
}

static void my_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = 0u;
    v->ph[1] = 0u;
}
static void my_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t i, p = v->ph[0], subp = v->ph[1];
    uint32_t wave = (uint32_t)t->p[P_E0] & 3u;
    uint32_t duty = 16u + ((uint32_t)clamp(t->p[P_E1], 0, 127) * 224u >> 7);
    uint32_t cutoff = duty << 24; /* about 6..93 % pulse width */
    int32_t sub = clamp(t->p[P_E2], 0, 127);
    uint32_t sh = (uint32_t)clamp(t->p[P_E3], 0, 127) * 16u >> 7;
    for (i = 0; i < n; i++) {
        int32_t y;
        if (wave == 0u) y = sine_i(p);
        else if (wave == 1u) y = osc_saw(p, m->inc);
        else if (wave == 2u) y = osc_pulse(p, m->inc, 0x80000000u);
        else y = osc_pulse(p, m->inc, cutoff);
        if (sub)
            y = (y * (128 - sub) + sine_i(subp) * sub) >> 7;
        y = (y >> sh) * (1 << sh); /* 16-bit to 1-bit without signed left-shift UB */
        out[i] += voice_amp(y, m, i);
        p += m->inc;
        subp += m->inc >> 1;
    }
    v->ph[0] = p;
    v->ph[1] = subp;
}

/* Per-part processor: REEL variable-delay colour, EUREKA short stutter.
 * P_E4 = OFF / REEL / EUREKA. P_E5 = wet mix, P_E6/7 = motion.
 * The original Android 10.2 s Eureka buffer cannot fit in FM-1 RAM.
 */
static uint32_t my_post(track_t *t, int32_t *out, uint32_t n)
{
    miyako_mem_t *s = (miyako_mem_t *)t->mem;
    uint32_t i, requested = (uint32_t)clamp(t->p[P_E4], 0, 2), mode;
    int32_t target = clamp(t->p[P_E5], 0, 127) * 258;
    if (!s) return 0;
    if (requested && s->mode != requested) {
        s->mode = (uint8_t)requested;
        s->wr = s->valid = s->phase = s->length = 0u;
        s->mix = 0;
    }
    mode = s->mode;
    if (!mode) return 0;
    if (!requested) target = 0;
    for (i = 0; i < n; ++i) {
        int32_t dry = my_limit(out[i]), fx = dry;
        if (mode == 1u) {
            /* Tape-speed fluctuation from two independent low-rate phases.
             * With an initially empty delay line, the unprocessed sound passes.
             */
            int32_t wow, fl, tap;
            s->wow += 58435u;       /* 0.6 Hz at 44.1 kHz */
            s->flutter += 779000u;  /* ~8 Hz at 44.1 kHz */
            wow = (sine_i(s->wow) * clamp(t->p[P_E6], 0, 127)) >> 17;
            fl  = (sine_i(s->flutter) * clamp(t->p[P_E7], 0, 127)) >> 19;
            tap = 256 + wow + fl;
            s->line[s->wr & MY_MASK] = (int16_t)dry;
            if (s->valid > (uint32_t)tap) {
                fx = s->line[(s->wr - (uint32_t)tap) & MY_MASK];
                /* Gentle tape saturation; bounded to the mix headroom. */
                fx = softclip(fx + (fx >> 2));
            }
            ++s->wr;
            if (s->valid < MY_LINE) ++s->valid;
        } else {
            /* Record a short segment, then repeat it 1..4 times.
             * Segment-edge linear ramps suppress click transients.
             */
            uint32_t len, reps, period, at, edge;
            if (!s->length || s->phase == 0u)
                s->length = 256u + (uint32_t)clamp(t->p[P_E6], 0, 127) * 12u;
            len = s->length;
            reps = 1u + ((uint32_t)clamp(t->p[P_E7], 0, 127) >> 5);
            period = len * (1u + reps);
            if (s->phase >= period) s->phase = 0u;
            if (s->phase < len) {
                s->line[s->phase] = (int16_t)dry;
                fx = dry;
            } else {
                at = (s->phase - len) % len;
                fx = s->line[at];
            }
            at = s->phase % len;
            edge = at < len - 1u - at ? at : len - 1u - at;
            if (edge < 32u) fx = (fx * (int32_t)edge) >> 5;
            ++s->phase;
        }
        s->mix += clamp(target - s->mix, -512, 512); /* 64-sample fade at 44.1 kHz */
        out[i] = dry + (((fx - dry) * s->mix) >> 15);
    }
    if (!requested && s->mix == 0) {
        s->mode = 0;
        s->valid = s->phase = 0u;
    }
    return mode == 2u && s->phase >= s->length && s->mix ? 1u : 0u;
}

static const char *const MY_WAVES[] = {"SINE", "SAW", "SQR", "PULSE"};
static const char *const MY_FX[] = {"OFF", "REEL", "EUREKA"};
static const preset_t MY_PRESETS[] = {
    {"SINE", {0, 64, 0, 0, 0, 0, 64, 64}, {4, 60, 110, 50}, 0, 0, FX(0, 10, 0, 10), PAT(0)},
    {"SAW", {1, 64, 0, 0, 0, 0, 64, 64}, {4, 60, 110, 50}, 0, 0, FX(0, 10, 0, 10), PAT(0)},
    {"REEL", {0, 64, 0, 0, 1, 85, 45, 30}, {4, 60, 110, 50}, 0, 0, FX(0, 0, 0, 5), PAT(0)},
    {"EUREKA", {1, 64, 0, 0, 2, 100, 48, 60}, {0, 60, 110, 40}, 0, 0, FX(0, 0, 0, 5), PAT(0)},
};
static const engine_t ENG_MIYAKO = {
    .name = "MIYAKO",
    .page_title = {"OSC", "FX"},
    .edit = {
        {"WAVE", F_ENUM, 0, 3, 0, MY_WAVES, 0},
        {"WIDTH", F_PCT, 0, 127, 64, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"CRUSH", F_PCT, 0, 127, 0, 0, 0},
        {"FX", F_ENUM, 0, 2, 0, MY_FX, 0},
        {"WET", F_PCT, 0, 127, 0, 0, 0},
        {"DEPTH", F_PCT, 0, 127, 64, 0, 0},
        {"RATE", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = MY_PRESETS,
    .npresets = sizeof MY_PRESETS / sizeof MY_PRESETS[0],
    .knob = {P_E0, P_E4, P_E5, P_E6},
    .note_on = my_note_on,
    .render = my_render,
    .post = my_post,
    .mem = sizeof(miyako_mem_t),
};
FE_PLUGIN(ENG_MIYAKO, FE_ID('m', 'Y', 'K', '1'), 1, FE_NEW, 0)
