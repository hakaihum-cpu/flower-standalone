#include "mi_recorder.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    mi_recorder_t r;
    uint8_t edits[4]={0,127,3,100};
    uint32_t i;
    mi_rec_init(&r);
    assert(r.count==0u&&!r.playing&&!r.recording);
    mi_rec_toggle_play(&r);
    assert(!r.playing);
    mi_rec_toggle_record(&r);
    assert(r.recording);
    for(i=0;i<MI_REC_CAPACITY*2u;i++){
        float signal=(i/100u)&1u?-0.75f:0.75f;
        float out=mi_rec_tick(&r,signal,edits);
        assert(out==0.f);
    }
    assert(!r.recording&&r.playing&&r.count==MI_REC_CAPACITY);
    assert(mi_rec_sample(100)>20000);
    assert(mi_rec_sample(150)<-20000);
    {
        int8_t lo,hi;
        mi_rec_peak(0,&lo,&hi);
        assert(lo<0&&hi>0);
    }
    /* Full buffer automatically loops; PLAY still toggles stop/restart. */
    mi_rec_toggle_play(&r);
    assert(!r.playing);
    mi_rec_toggle_play(&r);
    assert(r.playing);
    for(i=0;i<128u;i++){
        float out=mi_rec_tick(&r,0.f,edits);
        assert(out>=-1.f&&out<=1.f);
    }
    /* Seven rate indices: -12/-7/-5/0/+5/+7/+12, 0 is half-rate
       22.05 kHz clip -> 44.1 kHz output; +12 is exactly 2x that. */
    float before=r.playback_pos;
    edits[2]=0;
    mi_rec_tick(&r,0.f,edits);
    assert(r.playback_pos-before>0.24f&&r.playback_pos-before<0.26f);
    before=r.playback_pos;
    edits[2]=6;
    mi_rec_tick(&r,0.f,edits);
    assert(r.playback_pos-before>0.99f&&r.playback_pos-before<1.01f);
    edits[0]=20;edits[1]=30;edits[2]=3;
    for(i=0;i<MI_REC_CAPACITY*3u;i++)
        mi_rec_tick(&r,0.f,edits);
    assert(r.playing);
    assert(r.playback_pos>=(float)((r.count-1u)*20u/127u));
    assert(r.playback_pos<(float)((r.count-1u)*30u/127u+1u));
    mi_rec_toggle_play(&r);
    assert(!r.playing);
    /* Second REC adds a new layer; it must NEVER clear the first clip. */
    edits[0]=0u;edits[1]=127u;edits[2]=3u;edits[3]=100u;
    int16_t old10=mi_rec_sample(10),old40=mi_rec_sample(40);
    mi_rec_toggle_record(&r);
    assert(r.recording&&r.overdubbing&&r.count==MI_REC_CAPACITY&&r.playing);
    for(i=0;i<40u;i++){
        float monitored=mi_rec_tick(&r,0.1f,edits);
        assert(monitored>=-1.f&&monitored<=1.f);
    }
    assert(r.count==MI_REC_CAPACITY);
    assert(mi_rec_sample(10)!=old10); /* new layer actually added */
    assert(mi_rec_sample(40)==old40); /* untouched future samples preserved */
    mi_rec_toggle_record(&r);
    assert(!r.recording&&!r.overdubbing&&r.playing);
    mi_rec_clear(&r);
    assert(r.count==0u&&!r.recording&&!r.overdubbing&&!r.playing);
    mi_rec_toggle_record(&r);
    assert(r.recording&&!r.overdubbing&&r.count==0u);
    mi_rec_stop(&r);
    assert(!r.recording&&!r.playing);
    mi_rec_clear(&r);
    mi_rec_toggle_record(&r);
    for(i=0;i<1000u;i++)mi_rec_tick(&r,0.25f,edits);
    mi_rec_toggle_record(&r);
    assert(!r.recording&&r.playing&&r.count==500u);
    assert(r.playback_pos==0.f);
    /* No external MIDI clock: 2 or 4 bars fit the existing PCM capacity. */
    mi_rec_init(&r);
    mi_rec_configure(&r,120u,2u,44100.f,0u);
    assert(r.bars==2u && r.capture_step>0.24f &&
           r.capture_step<0.26f);
    mi_rec_configure(&r,120u,4u,44100.f,0u);
    assert(r.bars==4u && r.capture_step>0.12f &&
           r.capture_step<0.13f);
    mi_rec_toggle_record(&r);
    for(i=0u;i<44100u;i++)mi_rec_tick(&r,0.6f,edits);
    assert(r.recording && r.count>5400u && r.count<5600u);
    mi_rec_toggle_record(&r);
    assert(!r.recording && r.playing && r.count<MI_REC_CAPACITY);
    /* Clock: two bars = 2 * 4 * 24 = 192 ticks; even when REC is set
     * to 4 bars it is ignored under live external clock. */
    mi_rec_init(&r);
    mi_rec_external_tick(&r,0u);
    mi_rec_configure(&r,120u,4u,44100.f,0u);
    assert(r.clock_active && r.bars==2u);
    mi_rec_toggle_record(&r);
    assert(r.armed&&!r.recording);
    for(i=1u;i<=290u;i++){
        uint32_t j;
        mi_rec_external_tick(&r,i*919u);
        mi_rec_configure(&r,120u,4u,44100.f,i*919u);
        for(j=0u;j<919u;j++)mi_rec_tick(&r,0.3f,edits);
        if(i==100u)assert(r.recording&&!r.armed);
    }
    assert(r.count>40000u && r.count<=MI_REC_CAPACITY);
    assert(!r.recording && r.playing && r.bars==2u);
    mi_rec_clock_stop(&r);
    assert(!r.clock_active);
    puts("PASS: REC manual stop, adaptive 2/4 bars, 192 MIDI ticks, overdub, clear, pitch/trim");
    return 0;
}
