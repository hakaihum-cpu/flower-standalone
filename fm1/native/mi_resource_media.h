/* SPDX-License-Identifier: GPL-3.0-only
 * 628 original source images, 96x96, 16 RGB565 colours, 8x4 VQ,
 * 256 codewords every 27 frames; validated stock FM-1 res.bin.
 */
#ifndef MI_RESOURCE_MEDIA_H
#define MI_RESOURCE_MEDIA_H
#include <stdint.h>
#define MI_TILE_BANKS 8u
#define MI_TILE_COUNT 628u
#define MI_TILE_SIDE 96u
#define MI_TILE_VQ_CLIPS 24u
#define MI_TILE_VQ_FRAMES_PER_CLIP 27u
#define MI_TILE_VQ_COLS 12u
#define MI_TILE_VQ_ROWS 24u
#define MI_TILE_VQ_MAP_STRIDE 288u
#define MI_TILE_VQ_PALETTE_OFFSET 0u
#define MI_TILE_VQ_BOOK_OFFSET 768u
#define MI_TILE_VQ_MAP_OFFSET 99072u
#define MI_TILE_DATA_BYTES 279936u
/* Original fixed palette from the user's archival FM-1 96x96 VQ pack.
 * Match colour indices to the actual codebooks; never replace the 24
 * archive palettes with unrelated cool blue/purple colours. The legacy
 * recoloured res.bin uses the SAME index maps, so it renders correctly. */
/* Source-calibrated neutral RGB565 palettes for 628 user-supplied
 * MONOCHROME frames. 16 per-bank indices are the least-squares shade means
 * of EVERY 96x96 source frame against its exact decoded archival VQ indices
 * (SHA256 bb5345df9b9e4118052b2feac603e38f16db0fccd5af49223e52b08115a1cc40).
 * Colours are grayscale; codebooks, maps, frame order, DSP and res.bin stay
 * untouched. No per-frame palette flicker or new bitmap storage. */
static const uint16_t mi_photo_grayscale_palette[MI_TILE_BANKS][16] = {
    {0x0020u,0x10a2u,0x2124u,0x39e7u,0x4228u,0x5acbu,0x6b6du,0x8410u,0x94b2u,0x9cf3u,0xa534u,0xb5b6u,0xc618u,0xd6bau,0xef5du,0xffdfu},
    {0x0020u,0x1082u,0x18e3u,0x3186u,0x39e7u,0x4a69u,0x632cu,0x73aeu,0x7befu,0x8c71u,0x94b2u,0xad75u,0xad55u,0xbdf7u,0xce59u,0xdedbu},
    {0x0841u,0x10a2u,0x2124u,0x39c7u,0x4a49u,0x5acbu,0x738eu,0x8430u,0x94b2u,0xa514u,0xad75u,0xc638u,0xc618u,0xdefbu,0xef5du,0xffdfu},
    {0x0841u,0x1082u,0x2124u,0x39c7u,0x4228u,0x52aau,0x6b6du,0x8410u,0x8c71u,0x9cd3u,0xa534u,0xbdd7u,0xbdd7u,0xd69au,0xdedbu,0xef5du},
    {0x0000u,0x10a2u,0x2945u,0x39c7u,0x4228u,0x5acbu,0x6b6du,0x8430u,0x9cd3u,0x9cd3u,0xa534u,0xbdf7u,0xbdf7u,0xd6bau,0xef5du,0xffdfu},
    {0x0000u,0x10a2u,0x2945u,0x39c7u,0x4228u,0x52aau,0x6b6du,0x8410u,0x94b2u,0x9cd3u,0xa514u,0xb5b6u,0xbdf7u,0xdedbu,0xe73cu,0xffdfu},
    {0x0020u,0x10a2u,0x2945u,0x39e7u,0x4228u,0x52aau,0x6b6du,0x8410u,0x9492u,0x9cf3u,0xa534u,0xbdf7u,0xbdf7u,0xd6bau,0xe71cu,0xf7beu},
    {0x0020u,0x10a2u,0x2945u,0x39e7u,0x4228u,0x52aau,0x6b6du,0x8410u,0x8430u,0x9cf3u,0xa514u,0xbdf7u,0xbdf7u,0xdedbu,0xe71cu,0xf79eu}
};
static const uint16_t mi_tile_bank_first[8]={0u,81u,145u,245u,326u,407u,488u,558u};
static const uint8_t mi_tile_bank_count[8]={81u,64u,100u,81u,81u,81u,70u,70u};
#endif
