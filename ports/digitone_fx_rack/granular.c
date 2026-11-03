/* SPDX-License-Identifier: GPL-2.0-or-later
 * GRANULAR for Digitone mk1 / Keys OS 1.44; core-dn1 4.1.1.
 * Original Digitone FM, Delay and Reverb remain unchanged.
 *
 * Two pitch voices, two overlapping Hann-window grains per voice.
 * AUTO-channel CC33 PITCH A, CC34 PITCH B, CC36 SIZE,
 * CC37 POSITION and CC60 MIX.
 * Fixed-point 32 stereo frames/block, no allocation or feedback.
 *
 * Only NEW grains latch MIDI pitch and length changes: continuing grains
 * finish their old window before changing. No DSP reset on a CC event.
 */
#include <stdint.h>
#include "digitone-mk1/core3.h"
#include "rack.h"

#define HIST_FRAMES        131072u /* 2.731 seconds @ 48 kHz, stereo 512 KiB */
#define HIST_MASK          (HIST_FRAMES - 1u)
#define GRAIN_MIN_FRAMES     5760u /* 120 ms */
#define GRAIN_MAX_FRAMES    17280u /* 360 ms */
#define GRAIN_DEFAULT       11520u /* 240 ms */
#define GRAIN_ARM_FRAMES    90000u /* 1.875 s valid audio before wet begins */
#define GRAIN_POSITION_MAX  3000u /* existing CC37 range: 0..62.5 ms */
#define GRAIN_LAG_A         70000u
#define GRAIN_LAG_B         76000u
#define GRAIN_RAMP_FRAMES    3840u /* 80 ms fade on enable */

/* Worst-case read bounds: +24 consumes 4 input samples per output
 * sample; its max catch-up is 3 * 17280 = 51840, less than the
 * 70000-sample delay. At -12, the oldest sample is behind by
 * 76000 + 3000 + 17280/2 = 87640 < 90000 valid samples.
 * The 20.12 read phase cannot overflow u32 in a 360 ms grain.
 */
_Static_assert(HIST_FRAMES == 131072u, "power-of-two ring");
_Static_assert(GRAIN_LAG_A > 3u*GRAIN_MAX_FRAMES,
               "plus24 reads unwritten audio");
_Static_assert(GRAIN_LAG_B + GRAIN_POSITION_MAX +
               GRAIN_MAX_FRAMES / 2u < GRAIN_ARM_FRAMES,
               "minus12 reads stale history");
_Static_assert(GRAIN_MAX_FRAMES*16384u < 0xffffffffu,
               "Q20.12 read phase overflow");
_Static_assert((GRAIN_DEFAULT % 2u) == 0u, "half grains integral");

/* Grain pitch steps Q20.12. Maximum 4.0 for +24; no 64-bit
 * multiplication or division in the audio callback's sample loop. */
static const uint32_t pitch_steps[8] = {
    2048u,  /* -12 */
    2734u,  /*  -7 */
    3069u,  /*  -5 */
    4096u,  /*   0 */
    5468u,  /*  +5 */
    6137u,  /*  +7 */
    8192u,  /* +12 */
    16384u  /* +24 */
};

/* One 512 KiB history ring; never clear it from the UI thread.
 * Reset only hist_valid; old contents cannot play before re-arming.
 */
static int16_t history[HIST_FRAMES * 2u];
static uint32_t hist_write, hist_valid;
static uint8_t grain_armed;
static uint32_t grain_origin[4], grain_phase[4];
static uint32_t grain_frames[4], grain_speed[4], grain_window_step_q16[4];
static uint32_t grain_ramp;
/* 256-phase Hann window: complementary 50% phases always sum to 32767.
 * 512-byte immutable lookup, unlike a real-time cos() or sqrt(). */
static const uint16_t grain_hann_q15[256] = {
    0u, 5u, 20u, 44u, 79u, 123u, 177u, 241u, 315u, 398u, 491u, 593u, 
    705u, 827u, 958u, 1098u, 1247u, 1406u, 1573u, 1749u, 1935u, 2128u, 2331u, 2542u, 
    2761u, 2989u, 3224u, 3468u, 3719u, 3978u, 4244u, 4518u, 4799u, 5086u, 5381u, 5682u, 
    5990u, 6304u, 6624u, 6950u, 7281u, 7618u, 7961u, 8308u, 8660u, 9017u, 9379u, 9744u, 
    10114u, 10487u, 10864u, 11244u, 11628u, 12014u, 12403u, 12794u, 13187u, 13583u, 13980u, 14378u, 
    14778u, 15178u, 15580u, 15981u, 16383u, 16786u, 17187u, 17589u, 17989u, 18389u, 18787u, 19184u, 
    19580u, 19973u, 20364u, 20753u, 21139u, 21523u, 21903u, 22280u, 22653u, 23023u, 23388u, 23750u, 
    24107u, 24459u, 24806u, 25149u, 25486u, 25817u, 26143u, 26463u, 26777u, 27085u, 27386u, 27681u, 
    27968u, 28249u, 28523u, 28789u, 29048u, 29299u, 29543u, 29778u, 30006u, 30225u, 30436u, 30639u, 
    30832u, 31018u, 31194u, 31361u, 31520u, 31669u, 31809u, 31940u, 32062u, 32174u, 32276u, 32369u, 
    32452u, 32526u, 32590u, 32644u, 32688u, 32723u, 32747u, 32762u, 32767u, 32762u, 32747u, 32723u, 
    32688u, 32644u, 32590u, 32526u, 32452u, 32369u, 32276u, 32174u, 32062u, 31940u, 31809u, 31669u, 
    31520u, 31361u, 31194u, 31018u, 30832u, 30639u, 30436u, 30225u, 30006u, 29778u, 29543u, 29299u, 
    29048u, 28789u, 28523u, 28249u, 27968u, 27681u, 27386u, 27085u, 26777u, 26463u, 26143u, 25817u, 
    25486u, 25149u, 24806u, 24459u, 24107u, 23750u, 23388u, 23023u, 22653u, 22280u, 21903u, 21523u, 
    21139u, 20753u, 20364u, 19973u, 19580u, 19184u, 18787u, 18389u, 17989u, 17589u, 17187u, 16786u, 
    16384u, 15981u, 15580u, 15178u, 14778u, 14378u, 13980u, 13583u, 13187u, 12794u, 12403u, 12014u, 
    11628u, 11244u, 10864u, 10487u, 10114u, 9744u, 9379u, 9017u, 8660u, 8308u, 7961u, 7618u, 
    7281u, 6950u, 6624u, 6304u, 5990u, 5682u, 5381u, 5086u, 4799u, 4518u, 4244u, 3978u, 
    3719u, 3468u, 3224u, 2989u, 2761u, 2542u, 2331u, 2128u, 1935u, 1749u, 1573u, 1406u, 
    1247u, 1098u, 958u, 827u, 705u, 593u, 491u, 398u, 315u, 241u, 177u, 123u, 
    79u, 44u, 20u, 5u
};

/* MIDI-task writers; render task reads individual aligned scalars only. */
static volatile uint8_t mix_cc = 64u;
static volatile uint8_t pitch_choice[2] = {4u, 6u}; /* +5, +12 */
static volatile uint32_t grain_frames_target = GRAIN_DEFAULT;
static volatile uint32_t grain_position_target = 0u;

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
    uint32_t position = grain_position_target;

    if(len < GRAIN_MIN_FRAMES || len > GRAIN_MAX_FRAMES)
        len = GRAIN_DEFAULT;
    if(choice >= 8u)
        choice = 4u;
    if(position > GRAIN_POSITION_MAX)
        position = 0u;

    grain_frames[g] = len;
    grain_phase[g] = start_halfway ? len / 2u : 0u;
    grain_origin[g] =
        (hist_write + HIST_FRAMES -
         (g < 2u ? GRAIN_LAG_A : GRAIN_LAG_B) -
         position) & HIST_MASK;
    grain_speed[g] = pitch_steps[choice];
    /* No division, floating point or trigonometry in the sample loop. */
    grain_window_step_q16[g] = (256u << 16) / len;
}

/* 20.12 interpolated read head + Q15 Hann; at +24 pitch and 360 ms,
 * multiplying the 20.12 phase stays inside u32, unlike old 16.16. */
static int32_t grain_at(unsigned g, unsigned ch)
{
    uint32_t pos = grain_phase[g];
    uint32_t fraction = pos * grain_speed[g];
    uint32_t index = (grain_origin[g] + (fraction >> 12)) & HIST_MASK;
    int32_t a = history[index * 2u + ch];
    int32_t b = history[((index + 1u) & HIST_MASK) * 2u + ch];
    int32_t interpolated =
        a + (((b - a) * (int32_t)((fraction >> 4) & 255u)) >> 8);
    uint32_t phase = (pos * grain_window_step_q16[g]) >> 16;
    int32_t window = grain_hann_q15[phase < 256u ? phase : 255u];
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
            /* Each Hann overlapped pair sums to unity, then average the
             * two pitch voices: no 6 dB attenuation of the wet mix. */
            wet[0] >>= 1;
            wet[1] >>= 1;
        }

        history[hist_write * 2u] = (int16_t)dry[0];
        history[hist_write * 2u + 1u] = (int16_t)dry[1];
        hist_write = (hist_write + 1u) & HIST_MASK;
        if(hist_valid < HIST_FRAMES) ++hist_valid;

        /* Opening from the menu is click-free and preserves full dry
         * until history has filled. Fade wet in over 80 ms. */
        {
            int32_t effective_mix = mix;
            if(grain_armed) {
                if(grain_ramp < GRAIN_RAMP_FRAMES) ++grain_ramp;
                effective_mix = (mix * (int32_t)grain_ramp) /
                                (int32_t)GRAIN_RAMP_FRAMES;
            } else effective_mix = 0;
            for(ch = 0u; ch < 2u; ++ch) {
                int32_t blended =
                    (dry[ch] * (127 - effective_mix) +
                     wet[ch] * effective_mix) / 127;
                out[sample * 2u + ch] = soft15(blended) * 65536;
            }
        }
     }
}

void rack_granular_render(int32_t *out,const int32_t *in)
{
    granular_render(out,in);
}
void rack_granular_reset(void)
{
    hist_write=hist_valid=0u;
    grain_armed=0u;
    grain_ramp=0u;
}
int rack_granular_cc(int track, int cc, int value, int flags)
{
    (void)track;
    if((((uint32_t)flags & CORE_MIDI_CC_AUTO) == 0u) ||
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
        uint32_t steps = ((uint32_t)value * 48u + 63u) / 127u;
        grain_frames_target = GRAIN_MIN_FRAMES + 240u * steps;
        return 1;
    }
    if(cc == 37) {
        grain_position_target =
            ((uint32_t)value * GRAIN_POSITION_MAX + 63u) / 127u;
        return 1;
    }
    return 0;
}
