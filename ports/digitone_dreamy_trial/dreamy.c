/* SPDX-License-Identifier: GPL-2.0-or-later
 * GRANULAR trial for Digitone mk1 / Keys OS 1.44; core-dn1 >= 4.0.
 * Original stock FM, Delay and Reverb remain unchanged.
 *
 * ONLY two pitched grain voices: +5 semitones and +12 semitones.
 * Each voice uses two overlapping triangular-window grains.
 * NO independent Delay, Reverb or feedback network.
 *
 * Core's CORE_AUDIO_INSERT follows the Digitone stock master/FX stage.
 * Reverb and Delay therefore process the source BEFORE this granular insert.
 * Everything here is fixed-point; the render callback allocates no memory.
 */
#include <stdint.h>
#include "digitone-mk1/core3.h"

#define HIST_FRAMES 32768u
#define HIST_MASK (HIST_FRAMES - 1u)
#define GRAIN_LEN 8192u
#define GRAIN_HALF (GRAIN_LEN / 2u)
#define GRAIN_ARM_FRAMES 24000u

/* 128 KiB of ring buffer instead of the previous ~430 KiB.
 * No bulk zeroing (BSS may contain past audio after toggling): only read
 * from it after enough new frames have actually overwritten the ring.
 */
static int16_t history[HIST_FRAMES * 2u];
static uint32_t hist_write, hist_valid;
static uint8_t grain_armed;
static uint32_t grain_origin[4], grain_phase[4];
static volatile uint8_t mix_cc = 64u;

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

/* 16.16 read index, 8-bit interpolation; no float, no allocator. */
static int32_t grain_at(unsigned g, unsigned ch)
{
    uint32_t step = g < 2u ? 87480u : 131072u; /* 5 and 12 semitones */
    uint32_t pos = grain_phase[g];
    uint32_t fraction = pos * step;
    uint32_t index = (grain_origin[g] + (fraction >> 16)) & HIST_MASK;
    int32_t a = history[index * 2u + ch];
    int32_t b = history[((index + 1u) & HIST_MASK) * 2u + ch];
    int32_t interpolated = a + (((b - a) * (int32_t)((fraction >> 8) & 255u)) >> 8);
    uint32_t edge = pos < GRAIN_HALF ? pos : GRAIN_LEN - pos;
    int32_t window = (int32_t)(edge * 8u);
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
                for(g = 0u; g < 4u; ++g) {
                    grain_phase[g] = (g & 1u) ? GRAIN_HALF : 0u;
                    grain_origin[g] =
                        (hist_write + HIST_FRAMES - (g < 2u ? 21000u : 23000u))
                        & HIST_MASK;
                }
                grain_armed = 1u;
            }

            /* Two grains per pitch, two channels, no delay/reverb loops. */
            for(g = 0u; g < 4u; ++g) {
                for(ch = 0u; ch < 2u; ++ch)
                    wet[ch] += grain_at(g, ch);
                ++grain_phase[g];
                if(grain_phase[g] >= GRAIN_LEN) {
                    grain_phase[g] = 0u;
                    grain_origin[g] =
                        (hist_write + HIST_FRAMES - (g < 2u ? 21000u : 23000u))
                        & HIST_MASK;
                }
            }
            wet[0] >>= 2;
            wet[1] >>= 2;
        }

        /* Capture input each frame; cold-start reads only valid new data. */
        history[hist_write * 2u] = (int16_t)dry[0];
        history[hist_write * 2u + 1u] = (int16_t)dry[1];
        hist_write = (hist_write + 1u) & HIST_MASK;
        if(hist_valid < HIST_FRAMES) ++hist_valid;

        for(ch = 0u; ch < 2u; ++ch) {
            int32_t blended = (dry[ch] * (127 - mix) + wet[ch] * mix) / 127;
            out[sample * 2u + ch] = soft15(blended) * 65536;
        }
    }
}

/* Audio insert, not an exclusive synth: FM voices and stock FX stay active. */
struct core_audio_owner dreamy_owner = {0, granular_render, CORE_AUDIO_INSERT};

/* UI callback: O(1) reset, no array clear, no audio DSP work.
 * Turn on only after fresh-history counters are invalidated. */
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
const struct core_menu_item dreamy_menu = {"GRANULAR", dreamy_open, 0, 0};

/* AUTO-channel MIDI CC 60 dry/wet MIX; no CC 61/62 interception. */
int dreamy_cc(int track, int cc, int value, int flags)
{
    (void)track;
    if(!dreamy_owner.on || (((uint32_t)flags & CORE_MIDI_CC_AUTO) == 0u) ||
       value < 0 || value > 127)
        return 0;
    if(cc == 60) {
        mix_cc = (uint8_t)value;
        return 1;
    }
    return 0;
}
