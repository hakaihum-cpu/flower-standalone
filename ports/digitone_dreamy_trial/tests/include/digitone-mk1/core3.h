#ifndef DREAMY_HOST_CORE3_H
#define DREAMY_HOST_CORE3_H
#include <stdint.h>
#define CORE_AUDIO_INSERT 2u
#define CORE_MIDI_CC_AUTO 1u
struct core_audio_owner { volatile int32_t on; void (*render)(int32_t *, const int32_t *); uint32_t flags; };
struct core_menu_item { const char *name; void (*open)(void *,void *,int32_t); uint32_t tag; const uint16_t *icon; };
#endif
