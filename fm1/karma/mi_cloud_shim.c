/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO FM-1: no Mutable Clouds DSP.
 * Retains the original punch-in shared 16-bit recording ring, mu-law
 * lookup and encoder used in punch.c. Copyright upstream KARMA authors,
 * standard mu-law implementation licensed under GPL-3.0-only.
 * No Clouds granular models, grains or audio processing are linked.
 */
#define CL_NG 16
#define CL_RING_T int16_t
#define CL_RD(ring, i) CL_ULAW[~(uint32_t)(ring)[i] & 0xFFu]
#define CLD_N 32768u                                  /* the recording, samples: the punch-in ring's words (PUNCH_N) */
#define CLD_DOWN 2u                                   /* .. one for this many of the output's */
static const int16_t CL_ULAW[256] = {                 /* mu-law -> 16 bits (MuLaw2Lin of clouds/dsp/mu_law.h) */
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956, -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412, -11900, -11388, -10876, -10364, -9852, -9340, -8828, -8316,
    -7932, -7676, -7420, -7164, -6908, -6652, -6396, -6140, -5884, -5628, -5372, -5116, -4860, -4604, -4348, -4092,
    -3900, -3772, -3644, -3516, -3388, -3260, -3132, -3004, -2876, -2748, -2620, -2492, -2364, -2236, -2108, -1980,
    -1884, -1820, -1756, -1692, -1628, -1564, -1500, -1436, -1372, -1308, -1244, -1180, -1116, -1052, -988, -924,
    -876, -844, -812, -780, -748, -716, -684, -652, -620, -588, -556, -524, -492, -460, -428, -396,
    -372, -356, -340, -324, -308, -292, -276, -260, -244, -228, -212, -196, -180, -164, -148, -132,
    -120, -112, -104, -96, -88, -80, -72, -64, -56, -48, -40, -32, -24, -16, -8, 0,
    32124, 31100, 30076, 29052, 28028, 27004, 25980, 24956, 23932, 22908, 21884, 20860, 19836, 18812, 17788, 16764,
    15996, 15484, 14972, 14460, 13948, 13436, 12924, 12412, 11900, 11388, 10876, 10364, 9852, 9340, 8828, 8316,
    7932, 7676, 7420, 7164, 6908, 6652, 6396, 6140, 5884, 5628, 5372, 5116, 4860, 4604, 4348, 4092,
    3900, 3772, 3644, 3516, 3388, 3260, 3132, 3004, 2876, 2748, 2620, 2492, 2364, 2236, 2108, 1980,
    1884, 1820, 1756, 1692, 1628, 1564, 1500, 1436, 1372, 1308, 1244, 1180, 1116, 1052, 988, 924,
    876, 844, 812, 780, 748, 716, 684, 652, 620, 588, 556, 524, 492, 460, 428, 396,
    372, 356, 340, 324, 308, 292, 276, 260, 244, 228, 212, 196, 180, 164, 148, 132,
    120, 112, 104, 96, 88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8, 0,
};
/* Mutable Clouds DSP and grain playback intentionally removed. */

static int16_t punch_ring[CLD_N] __attribute__((section(".pool")));   /* (punch.c: its ring, shared) */

static uint8_t cl_mulaw(int32_t pcm)                  /* Lin2MuLaw of clouds/dsp/mu_law.h */
{
    int32_t mask, seg;
    pcm >>= 2;
    if (pcm < 0) {
        pcm = -pcm;
        mask = 0x7f;
    } else {
        mask = 0xff;
    }
    if (pcm > 8159)
        pcm = 8159;
    pcm += 0x84 >> 2;
    for (seg = 0; seg < 8 && pcm > (0x3f << seg | ((1 << seg) - 1)); seg++)
        ;
    if (seg >= 8)
        return (uint8_t)(0x7f ^ mask);
    return (uint8_t)(((seg << 4) | ((pcm >> (seg + 1)) & 0x0f)) ^ mask);
}

static void cloud_process(int32_t *in, int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i;
    (void)l;
    (void)r;
    for (i = 0u; i < n; ++i) in[i] = 0;
}
