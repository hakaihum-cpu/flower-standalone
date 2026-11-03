#ifndef EUREKA576_HOST_TEST_CORE3_H
#define EUREKA576_HOST_TEST_CORE3_H
#include <stdint.h>
#define CORE_MIDI_CC_AUTO 1u
extern volatile uint32_t fw_transport, fw_timeline;
extern volatile int32_t fw_tempo;
#define FW_EVENT_KIND(ev) (((volatile uint32_t *)(ev))[0])
#define FW_EVENT_FIRE 0u
#define FW_EVENT_FIRST 1u
#define FW_EVENT_TRACK(ev) (((volatile int32_t *)(ev))[2])
#define FW_EVENT_STEP(ev) (((volatile uint32_t *)(ev))[10])
#define FW_EVENT_NOTE_TRIG 1u
void fw_fillrect(void*,int32_t,int32_t,int32_t,int32_t,int32_t);
#endif
