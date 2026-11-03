/* SPDX-License-Identifier: GPL-2.0-or-later
 * GRANULAR for Digitone mk1 / Keys OS 1.44; core-dn1 4.1.1.
 * Original Digitone FM, Delay and Reverb remain unchanged.
 *
 * Two pitch voices, two overlapping triangular-window grains per voice.
 * AUTO-channel CC33 PITCH A, CC34 PITCH B, CC36 SIZE, CC60 MIX.
 * Fixed-point 32 stereo frames/block, no allocation or feedback.
 *
 * Only NEW grains latch MIDI pitch and length changes: continuing grains
 * finish their old window before changing. No DSP reset on a CC event.
 */
#include <stdint.h>
#include "digitone-mk1/core3.h"

#define HIST_FRAMES         32768u
#define HIST_MASK           (HIST_FRAMES - 1u)
#define GRAIN_MIN_FRAMES    1920u  /* 40 ms at 48 kHz */
#define GRAIN_MAX_FRAMES    6720u  /* 140 ms, bounded for +24 semitones */
#define GRAIN_DEFAULT       4800u  /* 100 ms */
#define GRAIN_ARM_FRAMES   27000u
#define GRAIN_LAG_A        21000u
#define GRAIN_LAG_B        23000u

/* Worst case: +24 pitch consumes 4 frames per output frame, but the
 * write head advances by 1. At 140 ms, it can approach by at most
 * 3 * 6720 = 20160 frames, less than LAG_A. Conversely, at -12,
 * the oldest B sample is only LAG_B + 6720 / 2 = 26360 back,
 * strictly within the 27000-frame startup history.
 */
_Static_assert(GRAIN_LAG_A > GRAIN_MAX_FRAMES * 3u,
               "plus24 reads unwritten input");
_Static_assert(GRAIN_LAG_B + GRAIN_MAX_FRAMES / 2u < GRAIN_ARM_FRAMES,
               "minus12 reads stale input");
_Static_assert(GRAIN_MAX_FRAMES * 262144u < 0xffffffffu,
               "16.16 read position overflow");
_Static_assert((GRAIN_DEFAULT % 2u) == 0u, "half grains must be integral");

static const uint32_t pitch_steps[8] = {
    32768u,  /* -12 */
    43740u,  /*  -7 */
    49097u,  /*  -5 */
    65536u,  /*   0 */
    87480u,  /*  +5 */
    98193u,  /*  +7 */
    131072u, /* +12 */
    262144u  /* +24 */
};

/* One 128 KiB history ring; never clear it from the UI thread.
 * Reset only hist_valid, so old data cannot be read before re-arming.
 */
static int16_t history[HIST_FRAMES * 2u];
static uint32_t hist_write, hist_valid;
static uint8_t grain_armed;
static uint32_t grain_origin[4], grain_phase[4];
static uint32_t grain_frames[4], grain_speed[4], grain_window_q12[4];

/* MIDI-task writers; render task reads individual aligned scalars only. */
static volatile uint8_t mix_cc = 64u;
static volatile uint8_t pitch_choice[2] = {4u, 6u}; /* +5, +12 */
static volatile uint32_t grain_frames_target = GRAIN_DEFAULT;

static int32_t clamp15(int32_t x)
{
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return x;
}

static int32_t soft15(int32_t x)
{
    if (x > 20000) return clamp15(20000 + (x - 20000) / 4);
    if (x < -20000) return clamp15(-20000 + (x + 20000) / 4);
    return x;
}

/* Called from the render only on a grain boundary (or first arm).
 * The next grain latches the requested CC values, so no current
 * grain is cut off and no bulk memory operation runs in a callback.
 */
static void grain_begin(unsigned g, uint32_t start_halfway)
{
    uint32_t len = grain_frames_target;
    uint32_t choice = pitch_choice[g / 2u];

    if(len < GRAIN_MIN_FRAMES || len > GRAIN_MAX_FRAMES)
        len = GRAIN_DEFAULT;
    if(choice >= 8u)
        choice = 4u;

    grain_frames[g] = len;
    grain_phase[g] = start_halfway ? len / 2u : 0u;
    grain_origin[g] =
        (hist_write + HIST_FRAMES -
         (g < 2u ? GRAIN_LAG_A : GRAIN_LAG_B)) & HIST_MASK;
    grain_speed[g] = pitch_steps[choice];
    /* Triangle peaks near Q15=32768 regardless of grain length.
     * No division in grain_at's per-sample fast path.
     */
    grain_window_q12[g] = (32768u << 12) / (len / 2u);
}

/* 16.16 interpolated source position, fixed-point envelope. */
static int32_t grain_at(unsigned g, unsigned ch)
{
    uint32_t pos = grain_phase[g];
    uint32_t fraction = pos * grain_speed[g];
    uint32_t index = (grain_origin[g] + (fraction >> 16)) & HIST_MASK;
    int32_t a = history[index * 2u + ch];
    int32_t b = history[((index + 1u) & HIST_MASK) * 2u + ch];
    int32_t interpolated =
        a + (((b - a) * (int32_t)((fraction >> 8) & 255u)) >> 8);
    uint32_t len = grain_frames[g];
    uint32_t edge = pos < len / 2u ? pos : len - pos;
    int32_t window =
        (int32_t)((edge * grain_window_q12[g]) >> 12);
    return (interpolated * window) >> 15;
}

static void granular_render(int32_t *out, const int32_t *in)
{
    unsigned sample, ch, g;
    int32_t mix = (int32_t)mix_cc;

    for(sample = 0u; sample < 32u; ++sample) {
        int32_t dry[2], wet[2] = {0, 0};
        for(ch = 0u; ch < 2u; ++ch)
            dry[ch] = in[sample * 2u + ch] >> 16;

        if(hist_valid >= GRAIN_ARM_FRAMES) {
            if(!grain_armed) {
                for(g = 0u; g < 4u; ++g)
                    grain_begin(g, (g & 1u) ? 1u : 0u);
                grain_armed = 1u;
            }
            for(g = 0u; g < 4u; ++g) {
                for(ch = 0u; ch < 2u; ++ch)
                    wet[ch] += grain_at(g, ch);
                if(++grain_phase[g] >= grain_frames[g])
                    grain_begin(g, 0u);
            }
            wet[0] >>= 2;
            wet[1] >>= 2;
        }

        history[hist_write * 2u] = (int16_t)dry[0];
        history[hist_write * 2u + 1u] = (int16_t)dry[1];
        hist_write = (hist_write + 1u) & HIST_MASK;
        if(hist_valid < HIST_FRAMES) ++hist_valid;

        for(ch = 0u; ch < 2u; ++ch) {
            int32_t blended =
                (dry[ch] * (127 - mix) + wet[ch] * mix) / 127;
            out[sample * 2u + ch] = soft15(blended) * 65536;
        }
    }
}

/* Insert after Digitone's built-in FX. Existing synth and stock FX stay on. */
struct core_audio_owner dreamy_owner = {
    0, granular_render, CORE_AUDIO_INSERT
};

/* O(1) enable/disable; no 128 KiB buffer clear. */
static void dreamy_open(void *brain, void *event, int32_t track)
{
    (void)brain; (void)event; (void)track;
    if(dreamy_owner.on) {
        dreamy_owner.on = 0;
        return;
    }
    dreamy_owner.on = 0;
    hist_write = 0u;
    hist_valid = 0u;
    grain_armed = 0u;
    dreamy_owner.on = 1;
}
const struct core_menu_item dreamy_menu = {
    "GRANULAR", dreamy_open, 0, 0
};

/* CC 33/34: eight exact 16-value bands, from -12 to +24 semitones.
 * CC 36: 40..140 ms, in 2 ms increments. Value 76 gives 100 ms.
 * CC 60: dry/wet 0..127. No other MIDI CC is consumed.
 */
int dreamy_cc(int track, int cc, int value, int flags)
{
    (void)track;
    if(!dreamy_owner.on ||
       (((uint32_t)flags & CORE_MIDI_CC_AUTO) == 0u) ||
       value < 0 || value > 127)
        return 0;

    if(cc == 60) {
        mix_cc = (uint8_t)value;
        return 1;
    }
    if(cc == 33 || cc == 34) {
        pitch_choice[(unsigned)(cc - 33)] = (uint8_t)((unsigned)value >> 4);
        return 1;
    }
    if(cc == 36) {
        uint32_t steps = ((uint32_t)value * 50u + 63u) / 127u;
        grain_frames_target = GRAIN_MIN_FRAMES + 96u * steps;
        return 1;
    }
    return 0;
}
