/* SPDX-License-Identifier: GPL-2.0-or-later
 * DREAMY trial, Digitone mk1 / Keys OS 1.44, core-dn1 >= 4.0.
 * Insert AFTER the stock master; the original FM voices and stock effects stay audible.
 * Experimental fixed-point port of the EFFECTS DREAMY concept:
 * two windowed pitch grains (+5 semitones / +12 semitones), stereo filtered
 * cross-feedback delay (0.16-0.52s), three damped stereo ambience lines.
 * No DSP work happens in a UI, tick or MIDI callback.
 */
#include <stdint.h>
#include "digitone-mk1/core3.h"

#define HIST_FRAMES 65536u
#define HIST_MASK   (HIST_FRAMES-1u)
#define DEL_FRAMES  32768u
#define DEL_MASK    (DEL_FRAMES-1u)
#define GRAIN_LEN   8192u
#define GRAIN_HALF  (GRAIN_LEN/2u)
#define VERB_ALLOC  3072u
#define MEM_COUNT (HIST_FRAMES*2u + DEL_FRAMES*2u + 2u*3u*VERB_ALLOC)
#define CLEAR_PER_TICK 16384u

/* All samples use Q15, stereo interleaved, BSS. About 0.43 MB total.
 * The existing RG DOT artwork is in .run; combined RAM budget checked at link. */
static int16_t history[HIST_FRAMES*2u];
static int16_t delay_line[DEL_FRAMES*2u];
static int16_t room[2][3][VERB_ALLOC];
static const uint16_t room_len[3] = { 1451u, 2137u, 2861u };
static uint16_t room_pos[3];
static int32_t room_damp[2][3];
static int32_t delay_lowpass[2];
static uint32_t hist_write, hist_valid, delay_write;
static uint32_t grain_origin[4], grain_phase[4];
static volatile uint32_t clear_pos, requested_on;
static volatile uint8_t mix_cc=64u, time_cc=75u, room_cc=72u;

/* The three component amounts are independent, but full-scale output
 * is bounded by soft saturation so sudden transients cannot overflow. */
static int32_t clip15(int32_t x)
{
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return x;
}

static int32_t soft15(int32_t x)
{
    /* Fast, monotone, symmetric soft knee after +/-20000 Q15. */
    if (x > 20000) {
        x = 20000 + (x - 20000) / 4;
        return clip15(x);
    }
    if (x < -20000) {
        x = -20000 + (x + 20000) / 4;
        return clip15(x);
    }
    return x;
}

static int32_t grain_at(unsigned g, unsigned ch)
{
    unsigned step = g < 2u ? 87480u : 131072u; /* 2^(5/12), octave, Q16 */
    unsigned pos = grain_phase[g];
    unsigned frac_index = pos * step;
    unsigned index = (grain_origin[g] + (frac_index >> 16)) & HIST_MASK;
    int32_t a = history[index * 2u + ch];
    int32_t b = history[((index+1u)&HIST_MASK)*2u+ch];
    int32_t interp = a + ((b-a)*(int32_t)((frac_index>>8)&255u) >> 8);
    unsigned edge = pos < GRAIN_HALF ? pos : GRAIN_LEN-pos;
    int32_t envelope = (int32_t)(edge * 8u); /* triangular 0..32768 */
    return (interp * envelope) >> 15;
}

static void dreamy_render(int32_t *out, const int32_t *in)
{
    unsigned sample, ch, j, g;
    /* UI/MIDI-task writes are 8-bit and affect only next-block knobs. */
    int32_t mix = (int32_t)mix_cc, room_amt=(int32_t)room_cc;
    unsigned delay_frames = 7680u + ((unsigned)time_cc*17280u)/127u;
    for (sample=0u; sample<32u; ++sample) {
        int32_t dry[2], pitch[2]={0,0}, delayed[2], ambience[2]={0,0};
        unsigned readpos = (delay_write + DEL_FRAMES-delay_frames) & DEL_MASK;
        for (ch=0u;ch<2u;++ch) dry[ch] = in[sample*2u+ch] >> 16;

        if (hist_valid > 24000u) {
            for (g=0u;g<4u;++g) {
                for (ch=0u;ch<2u;++ch)
                    pitch[ch] += grain_at(g,ch);
                ++grain_phase[g];
                if (grain_phase[g]>=GRAIN_LEN) {
                    grain_phase[g] = 0u;
                    grain_origin[g]=(hist_write+HIST_FRAMES-(g<2u?21000u:23000u))&HIST_MASK;
                }
            }
            for (ch=0u;ch<2u;++ch) pitch[ch] >>= 2;
        }

        /* Input always captured, including while the first grains fill. */
        for(ch=0u;ch<2u;++ch)
            history[hist_write*2u+ch]=(int16_t)dry[ch];
        hist_write=(hist_write+1u)&HIST_MASK;
        if(hist_valid<HIST_FRAMES) ++hist_valid;

        for(ch=0u;ch<2u;++ch) {
            int32_t raw=delay_line[readpos*2u+ch];
            delay_lowpass[ch]+=(raw-delay_lowpass[ch])>>2;
            delayed[ch]=delay_lowpass[ch];
        }
        for(ch=0u;ch<2u;++ch) {
            int32_t sent = (dry[ch]*3+pitch[ch])/4;
            /* Cross-feedback, with damping and bounded write. */
            int32_t feedback = (delayed[1u-ch]*16384)>>15;
            delay_line[delay_write*2u+ch]=(int16_t)soft15(sent+feedback);
        }
        delay_write=(delay_write+1u)&DEL_MASK;

        for(j=0u;j<3u;++j) {
            unsigned n=room_pos[j];
            for(ch=0u;ch<2u;++ch) {
                int32_t stored=room[ch][j][n];
                room_damp[ch][j]+=(stored-room_damp[ch][j])>>3;
                int32_t sent=(pitch[ch]+delayed[ch])>>2;
                int32_t feedback=(room_damp[ch][j]*21135)>>15;
                room[ch][j][n]=(int16_t)soft15(sent+feedback);
                ambience[ch]+=stored;
            }
            if (++room_pos[j]>=room_len[j]) room_pos[j]=0u;
        }

        for(ch=0u;ch<2u;++ch) {
            int32_t verb=ambience[ch]/3;
            int32_t wet=(pitch[ch]*13107
                         +delayed[ch]*9830
                         +((verb*room_amt)/127)*6554)>>15;
            /* The dry path stays present; output level cannot wrap. */
            int32_t shaped=soft15((dry[ch]*(127-mix)+wet*mix)/127);
            out[sample*2u+ch]=shaped*65536;
        }
    }
}

/* One active insert at a time, as specified by core's core_audio table.
 * This does not replace the original Digitone master or mute its voices. */
struct core_audio_owner dreamy_owner={0, dreamy_render, CORE_AUDIO_INSERT};

static void dreamy_open(void *brain, void *event, int32_t track)
{
    (void)brain; (void)event; (void)track;
    if (dreamy_owner.on || requested_on) {
        dreamy_owner.on=0;
        requested_on=0;
    } else {
        /* Clear a bounded amount at each UI tick, never on the audio IRQ. */
        dreamy_owner.on=0;
        clear_pos=0u;
        requested_on=1u;
    }
}
const struct core_menu_item dreamy_menu={"DREAMY",dreamy_open,0,0};

void dreamy_tick(void *ctrl)
{
    unsigned i, limit;
    int16_t *base;
    (void)ctrl;
    if(!requested_on || dreamy_owner.on) return;
    limit=clear_pos+CLEAR_PER_TICK;
    if(limit>MEM_COUNT) limit=MEM_COUNT;
    for(i=clear_pos;i<limit;++i) {
        if (i < HIST_FRAMES*2u) history[i]=0;
        else if (i < HIST_FRAMES*2u+DEL_FRAMES*2u)
            delay_line[i-HIST_FRAMES*2u]=0;
        else {
            unsigned n=i-HIST_FRAMES*2u-DEL_FRAMES*2u;
            room[n/(3u*VERB_ALLOC)][(n/VERB_ALLOC)%3u][n%VERB_ALLOC]=0;
        }
    }
    clear_pos=limit;
    if(clear_pos!=MEM_COUNT) return;
    hist_write=hist_valid=delay_write=0u;
    for(i=0u;i<3u;++i) {
        room_pos[i]=0u;
        room_damp[0][i]=room_damp[1][i]=0;
    }
    delay_lowpass[0]=delay_lowpass[1]=0;
    for(i=0u;i<4u;++i) {
        grain_phase[i]=(i&1u)?GRAIN_HALF:0u;
        grain_origin[i]=(HIST_FRAMES-(i<2u?21000u:23000u))&HIST_MASK;
    }
    dreamy_owner.on=1;
}

/* Reuse EFFECTS DREAMY's MIDI CC 60/61/62 on Digitone AUTO channel only.
 * We consume them only when the insert is active; other MIDI is untouched. */
int dreamy_cc(int track, int cc, int value, int flags)
{
    (void)track;
    if(!dreamy_owner.on || !(flags & CORE_MIDI_CC_AUTO) ||
       value<0 || value>127) return 0;
    if(cc==60) { mix_cc=(uint8_t)value; return 1; }
    if(cc==61) { time_cc=(uint8_t)value; return 1; }
    if(cc==62) { room_cc=(uint8_t)value; return 1; }
    return 0;
}
