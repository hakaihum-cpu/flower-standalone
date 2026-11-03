#ifndef TEST_CORE3_H
#define TEST_CORE3_H
#include <stdint.h>
#define CORE_AUDIO_INSERT 2u
#define CORE_MIDI_CC_AUTO 1u
struct core_audio_owner {volatile int32_t on; void (*render)(int32_t*,const int32_t*); uint32_t flags;} __attribute__((aligned(4)));
struct core_menu_item {const char *name; void (*open)(void*,void*,int32_t);uint32_t tag;const uint16_t *icon;} __attribute__((aligned(4)));
extern volatile int32_t fw_tempo;
extern const char fw_font5[],fw_font_label[],fw_font_title[];
extern void fw_fillrect(void*,int32_t,int32_t,int32_t,int32_t,int32_t);
extern void fw_framerect(void*,int32_t,int32_t,int32_t,int32_t,int32_t);
extern int32_t fw_textf(void*,const void*,int32_t,int32_t,int32_t,const char*,...);
#endif
