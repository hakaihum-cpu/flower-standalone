#include "mi_recorder.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    mi_recorder_t r;
    const uint8_t controls[4]={0,127,3,127};
    uint32_t i;
    mi_rec_init(&r);
    mi_rec_config(&r,44100.f,40u);
    mi_rec_toggle_record(&r);
    assert(r.recording&&!r.clock_mode&&r.limit_count==MI_REC_CAPACITY);
    for(i=0u;i<44100u;i++)mi_rec_tick(&r,(i/4410u)&1u? -0.75f:0.75f,controls);
    mi_rec_toggle_record(&r);
    assert(!r.recording&&r.playing);
    assert(r.count>=7999u&&r.count<=8001u);
    assert(mi_rec_sample(100)>18000);
    mi_rec_clear(&r);
    assert(!r.count);
    mi_rec_config(&r,44100.f,120u);
    mi_rec_midi_realtime(&r,0xfau);
    for(i=0u;i<10u;i++)mi_rec_midi_realtime(&r,0xf8u);
    mi_rec_toggle_record(&r);
    assert(r.clock_mode&&r.clock_wait&&!r.recording);
    /* Align to the next bar after START: tick #96. */
    for(i=0u;i<86u;i++){
        unsigned j;
        mi_rec_midi_realtime(&r,0xf8u);
        for(j=0u;j<919u;j++)mi_rec_tick(&r,0.f,controls);
    }
    assert(!r.clock_wait&&r.recording);
    for(i=0u;i<192u;i++){
        unsigned j;
        mi_rec_midi_realtime(&r,0xf8u);
        for(j=0u;j<919u;j++)mi_rec_tick(&r,0.25f,controls);
        if(i==37u)mi_rec_toggle_record(&r);
    }
    assert(!r.clock_mode&&!r.recording&&!r.clock_pad&&r.playing);
    assert(r.count>30000u&&r.count<34000u);
    assert(mi_rec_sample(5000)>0);
    assert(mi_rec_sample(r.count-1u)==0);
    mi_rec_stop(&r);
    puts("PASS: free record end, 8kHz uLaw, MIDI two-bar endpoint, silent tail");
    return 0;
}
