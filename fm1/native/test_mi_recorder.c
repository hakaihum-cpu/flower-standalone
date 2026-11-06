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
    assert(!r.recording&&r.count==MI_REC_CAPACITY);
    assert(mi_rec_sample(100)>20000);
    assert(mi_rec_sample(150)<-20000);
    {
        int8_t lo,hi;
        mi_rec_peak(0,&lo,&hi);
        assert(lo<0&&hi>0);
    }
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
    mi_rec_toggle_record(&r);
    assert(r.recording&&r.count==0u&&!r.playing);
    mi_rec_stop(&r);
    puts("PASS: 2-sec 22.05k mono REC/PLAY waveform IN/OUT and 7 semitone rates");
    return 0;
}
