/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO application contract for the MPL-2.0 fm1-base hardware platform.
 * This application contains no KARMA code.
 */
#pragma once
#include "plat.h"
#define APP_NAME "MIYAKO"
#define APP_TAGLINE "NATIVE TRIAL"
/* Version display is explicit for all development builds.
 * Release builds may override with -DAPP_VERSION from fm1-base.
 * This is distinct from the FWSC package identity FM-1_933. */
#ifndef APP_VERSION
#define APP_VERSION "0.3.0-DEV"
#endif
#define APP_SPLASH_RGB RGB(255, 209, 12)
#define APP_BLOCK 256u
#define APP_TOUCH
#define APP_HOST_EXPECT
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}
/* Standard fm1-base resource area, [0x97000,0xDC000), excludes the
 * 0xDC000 settings A/B pair and the existing 0xFC000 save region. */
#define APP_RES_BASE 0x97000u
#define APP_RES_SECTORS 69u
#define APP_BACKUP_OBJS {OBJ_SETTINGS}
#define APP_BACKUP_MAGIC 0x4D493134u
#define APP_BACKUP_VERSION 1u
#define APP_BACKUP_WHAT "restores your saved MIYAKO settings"
int app_backup_playing(void);
void app_backup_flush(void);
void app_backup_hold(int on);
void app_backup_restored(void);
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
#ifdef PLAT_HOST
int app_expect(const char *what, int *got);
#endif
