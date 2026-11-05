/* SPDX-License-Identifier: GPL-3.0-only
 * Native-host regression: prove the FM-1 C decoder reconstructs every one
 * of the original 576 converted frames, not merely that the header exists.
 * This runs only in CI host tests; it is not included in target firmware.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define C_BLACK 0u
#define MI_DOT_Y 0xFFE0u /* other DOT UI only; photo uses RGB565 */

typedef struct { int32_t peak; } track_t;

static int32_t clamp(int32_t x, int32_t lo, int32_t hi)
{
    return x < lo ? lo : x > hi ? hi : x;
}

static uint32_t rect_count;
static uint16_t lcd[124][240];
static void cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);

#include "mi_eureka_video.c"

static void cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
    int32_t px, py;
    /* The decoder is only allowed the 240 x 124 graph strip. */
    assert(x >= 0 && y >= 0 && w > 0 && h > 0);
    assert(x + w <= 240 && y + h <= 124);
    /* Every colour must come from the matching original 36-frame clip palette. */
    {
        uint32_t i, ok = color == C_BLACK;
        for (i = 0; i < MI_EUREKA_ASSET_COLORS; ++i)
            if (mi_eureka_vq_palette[mi_ev_visible / 36u][i] == color) ok = 1u;
        assert(ok);
    }
    for (py = y; py < y + h; ++py)
        for (px = x; px < x + w; ++px)
            lcd[py][px] = color;
    rect_count++;
}


static uint32_t crc_feed(uint32_t crc, const uint8_t *data, uint32_t n)
{
    uint32_t i, b;
    for (i = 0; i < n; ++i) {
        crc ^= (uint32_t)data[i];
        for (b = 0; b < 8u; ++b)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc;
}

/* The run-merged RGB565 renderer must produce the EXACT nearest-neighbour
 * 4bpp palette image; not visually-similar synthetic replacement art. */
static void assert_display_matches_frame(void)
{
    uint32_t x, y, xx, yy;
    for (y = 0; y < MI_EUREKA_ASSET_H; ++y)
        for (x = 0; x < MI_EUREKA_ASSET_W; ++x) {
            uint32_t b = y * MI_EUREKA_ASSET_W + x;
            uint8_t index = (uint8_t)((x & 1u) ? (mi_ev_pixels[b >> 1] & 15u)
                                                  : (mi_ev_pixels[b >> 1] >> 4));
            uint16_t color = mi_eureka_vq_palette[mi_ev_visible / 36u][index];
            for (yy = 3u + y * 3u / 2u; yy < 3u + (y + 1u) * 3u / 2u; ++yy)
                for (xx = 24u + x * 3u / 2u; xx < 24u + (x + 1u) * 3u / 2u; ++xx)
                    assert(lcd[yy][xx] == color);
        }
    assert(lcd[0][0] == 0u && lcd[123][239] == 0u);
}

int main(void)
{
    uint32_t frame, crc = 0xFFFFFFFFu, at_rest, previous_rng;
    track_t track = {0};
    assert(MI_EUREKA_ASSET_COUNT == 576u);
    assert(MI_EUREKA_ASSET_BYTES == 4992u);
    assert(MI_EUREKA_ASSET_COLORS == 16u && MI_EUREKA_ASSET_BPP == 4u);
    for (frame = 0; frame < 16u; ++frame)
        assert(mi_eureka_vq_palette[frame][0] == C_BLACK);
    assert(sizeof(mi_eureka_vq_tilebook) == 16u * 256u * 16u);
    assert(sizeof(mi_eureka_vq_maps) == 576u * 320u);
    assert(sizeof(mi_eureka_vq_palette) == 16u * 16u * 2u);

    /* Independent C CRC32 over the decoded chronological video.
     * The Python VQ packer CRC is computed from the 576 reconstructed
     * 4bpp frames. This detects corrupt codebooks or maps, but is NOT a
     * claim that lossy VQ matches JPEG pixels. */
    for (frame = 0; frame < MI_EUREKA_ASSET_COUNT; ++frame) {
        assert(mi_ev_seek(frame));
        crc = crc_feed(crc, mi_ev_pixels, MI_EUREKA_ASSET_BYTES);
    }
    assert((~crc) == MI_EUREKA_DECODED_CRC32);
    assert(!mi_ev_seek(MI_EUREKA_ASSET_COUNT));
    assert(mi_ev_seek(0u));
    assert(mi_ev_seek(35u));
    assert(mi_ev_seek(36u));
    assert(mi_ev_seek(575u));

    /* Opening EUREKA when silent displays original art, never autoplay.
     * An audio onset advances the random decision and video playhead;
     * silence then freezes the resulting original frame. */
    mi_ev_enabled = 0u;
    mi_ev_clip = -1;
    mi_ev_prior = 0u;
    mi_vq_last_frame = -1;
    mi_ev_draw(&track);
    at_rest = mi_ev_visible;
    assert(mi_ev_visible < 576u && rect_count > 0u);
    assert_display_matches_frame();
    mi_ev_draw(&track);
    assert(mi_ev_visible == at_rest);
    previous_rng = mi_ev_rng;
    track.peak = 3200;
    mi_ev_draw(&track);
    assert(mi_ev_rng != previous_rng);
    assert(mi_ev_visible != at_rest);
    assert_display_matches_frame();
    at_rest = mi_ev_visible;
    track.peak = 0;
    mi_ev_draw(&track);
    assert(mi_ev_visible == at_rest);

    printf("PASS EUREKA RGB565 16-colour VQ: 576 original slots decoded by C; CRC32 %08X; "
           "silent freeze/onset/random and graph bounds validated\n",
           (unsigned)(~crc));
    return 0;
}
