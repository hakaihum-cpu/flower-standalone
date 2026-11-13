/* SPDX-License-Identifier: GPL-3.0-only
 * Experimental source-derived SEL correction overlay. Does NOT alter FM-1
 * firmware unless deliberately linked after physical/quality validation.
 */
#ifndef MI_MHP2_PATCH_POC_H
#define MI_MHP2_PATCH_POC_H
#include <stdint.h>
#include "mi_mhp2_codec_poc.h"
/* Packed 629-entry uint32 frame directory, followed by 18-byte records:
 * uint16 tile_id (0..287), 16 bytes exact source palette indices (8x4 4bpp).
 * Caller must provide a CRC-protected resource and bounds-checked length.
 */
int mi_mhp2_apply_patch(const uint8_t *overlay, uint32_t overlay_bytes,
                        uint16_t frame, uint8_t out_pixels[MI_MHP2_FRAME_BYTES]);
#endif