/* SPDX-License-Identifier: GPL-3.0-only
 * Freestanding-only MIYAKO memory and string helpers for JieLi pi32v2.
 *
 * The firmware compiler supplies <stdint.h>, but does NOT ship libc
 * <string.h>. Never include or depend on hosted string.h in target code.
 * Volatile byte stores keep zeroing independent of a compiler-emitted
 * memset() symbol even under optimisations; only used at init/reset.
 */
#ifndef MI_NATIVE_MEMORY_H
#define MI_NATIVE_MEMORY_H

static inline void mi_memzero(void *destination, unsigned count)
{
    volatile unsigned char *p = (volatile unsigned char *)destination;
    while (count--) *p++ = 0u;
}

static inline int mi_streq(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
#endif
