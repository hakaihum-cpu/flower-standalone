/* SPDX-License-Identifier: GPL-3.0-only
 * MIYAKO application contract for the MPL-2.0 fm1-base hardware platform.
 * This application contains no KARMA code.
 */
#pragma once
#include "plat.h"
#define APP_NAME "MIYAKO"
#define APP_SPLASH_RGB RGB(255, 209, 12)
#define APP_BLOCK 256u
#define APP_TOUCH
#define APP_HOST_EXPECT
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
