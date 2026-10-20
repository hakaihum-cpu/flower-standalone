/* Android bionic declares glob/globfree starting at API 28.
 * libpd's pinned x_file.c uses them at API 24. This is a local
 * build-only declaration, NOT a modification to upstream Pure Data.
 */
#pragma once
#if defined(__ANDROID__) && defined(__ANDROID_API__) && __ANDROID_API__ < 28
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#include <glob.h>
#ifdef __cplusplus
extern "C" {
#endif
int glob(const char *pattern, int flags,
         int (*errfunc)(const char *, int), glob_t *results);
void globfree(glob_t *results);
#ifdef __cplusplus
}
#endif
#endif
