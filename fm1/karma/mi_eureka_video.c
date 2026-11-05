/* SPDX-License-Identifier: GPL-3.0-only
 * Original user-supplied MIYAKO EUREKA video, all 16 x 36 frames.
 * Offline quantization/packing preserves every frame, with no replacement art.
 * This runs in the KARMA UI thread, NOT in the audio render callback.
 *
 * Each source clip uses 256 8x4 tile prototypes + 36 x 320 index maps,
 * with a fixed 16-colour RGB565 palette. Intentional lossy compression
 * is measured during packing; no omitted source frames or replacement art.
 * Bounded 4bpp playback RAM: 128 x 78 / 2 = 4992 bytes.
 */
#include "mi_eureka_vq_decoder.c"
typedef char mi_ev_count_guard[(MI_EUREKA_ASSET_COUNT == 576u) ? 1 : -1];
typedef char mi_ev_dims_guard[(MI_EUREKA_ASSET_W == 128u &&
                               MI_EUREKA_ASSET_H == 78u &&
                               MI_EUREKA_ASSET_BYTES == 4992u &&
                               MI_EUREKA_ASSET_COLORS == 16u &&
                               MI_EUREKA_ASSET_BPP == 4u &&
                               MI_EUREKA_VQ_TILE_COUNT == 320u) ? 1 : -1];
#define mi_ev_pixels mi_vq_pixels
static uint32_t mi_ev_visible = 0u;
static uint32_t mi_ev_prior = 0u;
static uint32_t mi_ev_rng = 0xA18CE431u;
static uint32_t mi_ev_playhead = 0u;
static int32_t mi_ev_clip = -1;
static uint8_t mi_ev_enabled = 0u;

/* Advance the generator for EVERY attack decision; reusing the unchanged
 * state would make all attacks in a clip either always switch or never switch. */
static uint32_t mi_ev_rand(void)
{
    mi_ev_rng ^= mi_ev_rng << 13;
    mi_ev_rng ^= mi_ev_rng >> 17;
    mi_ev_rng ^= mi_ev_rng << 5;
    return mi_ev_rng;
}

static void mi_ev_choose_clip(void)
{
    int32_t clip;
    clip = (int32_t)(mi_ev_rand() % 16u);
    if (clip == mi_ev_clip) clip = (clip + 1) % 16;
    mi_ev_clip = clip;
    mi_ev_playhead = 0u;
    mi_ev_visible = (uint32_t)clip * 36u;
}

/* Direct O(320 tiles) random access: no previous frames are required.
 * All 576 original slots are decoded from the VQ tile dictionaries. */
static int mi_ev_seek(uint32_t frame)
{
    return mi_vq_seek(frame);
}

/* Match the original MIYAKO VisualizerComponent::setAudioActivity:
 * first real frame at rest, no autoplay on held level / silence, advance
 * on meaningful audio dynamics, 1..4 frames per event, occasional random
 * clip changes on attacks, and a new random clip at each end.
 * The 4.5 fps source sampling rate describes media extraction; it is NOT
 * a license to replace signal-reactive playback with a fixed-rate loop. */
static void mi_ev_update(const track_t *t)
{
    uint32_t peak, diff, onset, audio_present;
    const uint32_t previous = mi_ev_prior;
    peak = (uint32_t)clamp(t->peak, 0, 32767);
    /* KARMA peak is an accumulated maximum; consume it once on this FX page
     * so changes in the live signal can actually drive the video. */
    ((track_t *)t)->peak = 0;
    diff = peak > previous ? peak - previous : previous - peak;
    audio_present = peak > 197u;
    onset = audio_present && peak > 262u && peak > previous * 145u / 100u + 98u;
    mi_ev_prior = peak;
    if (!mi_ev_enabled) {
        mi_ev_enabled = 1u;
        mi_ev_prior = peak;
        if (mi_ev_clip < 0) mi_ev_choose_clip();
    }
    if (audio_present && (onset || diff > 81u)) {
        if (onset && mi_ev_rand() % 5u == 0u) {
            mi_ev_choose_clip();
        } else {
            uint32_t step = 1u + diff * 65u / 32768u + peak * 2u / 32768u;
            if (step > 4u) step = 4u;
            mi_ev_playhead += step;
            if (mi_ev_playhead >= 36u)
                mi_ev_choose_clip();
            else
                mi_ev_visible = (uint32_t)mi_ev_clip * 36u + mi_ev_playhead;
        }
    }
    if (!mi_ev_seek(mi_ev_visible))
        mi_ev_enabled = 0u; /* invalid media: no substitute image */
}
static void mi_ev_render(void)
{
    uint32_t x, y;
    if (!mi_ev_enabled || mi_ev_clip < 0) {
        cv_rect(0, 0, 240, 124, C_BLACK);
        return;
    }
    /* Isotropic 1.5x nearest-neighbour display. The earlier 224x78 mapping
     * stretched square source portraits horizontally; this does not. */
    cv_rect(0, 0, 240, 124, C_BLACK);
    /* RGB565 colour output, run-merged per palette index. All 576 source
     * frames retained; 16 colours per 36-frame clip, not 1-bit yellow art.
     * Palette[0] is always black, including original letterbox strips. */
    {
        const uint16_t *palette = mi_eureka_vq_palette[mi_ev_visible / 36u];
        for (y = 0; y < MI_EUREKA_ASSET_H; ++y) {
            uint32_t y0 = 3u + y * 3u / 2u;
            uint32_t y1 = 3u + (y + 1u) * 3u / 2u;
            x = 0u;
            while (x < MI_EUREKA_ASSET_W) {
                uint32_t b = y * MI_EUREKA_ASSET_W + x;
                uint32_t shade = (x & 1u) ? (mi_ev_pixels[b >> 1] & 15u)
                                           : (mi_ev_pixels[b >> 1] >> 4);
                uint32_t first, x0, x1;
                if (!shade) {
                    ++x;
                    continue;
                }
                first = x++;
                while (x < MI_EUREKA_ASSET_W) {
                    uint32_t bit = y * MI_EUREKA_ASSET_W + x;
                    uint32_t q = (x & 1u) ? (mi_ev_pixels[bit >> 1] & 15u)
                                          : (mi_ev_pixels[bit >> 1] >> 4);
                    if (q != shade)
                        break;
                    ++x;
                }
                x0 = 24u + first * 3u / 2u;
                x1 = 24u + x * 3u / 2u;
                cv_rect((int32_t)x0, (int32_t)y0,
                        (int32_t)(x1 - x0), (int32_t)(y1 - y0),
                        palette[shade]);
            }
        }
    }
}

/* Compatibility for the existing VQ host regression and older DOT preview.
 * The new MIYAKO-only full-height view calls update ONCE, render TWICE
 * (two 120-pixel LCD passes) to avoid consuming the same audio peak twice. */
static void mi_ev_draw(const track_t *t)
{
    mi_ev_update(t);
    mi_ev_render();
}
