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
    /* Second REC adds a new layer AT THE CURRENT PLAYHEAD, not at zero.
     * Pause the transport in the middle, then confirm both cursors stay. */
    edits[0]=0u;edits[1]=127u;edits[2]=3u;edits[3]=100u;
    r.playback_pos=1234.25f;
    {
        int16_t old10=mi_rec_sample(10),old1234=mi_rec_sample(1234);
        int16_t old1280=mi_rec_sample(1280);
        mi_rec_toggle_record(&r);
        assert(r.recording&&r.overdubbing&&r.count==MI_REC_CAPACITY&&r.playing);
        assert(r.playback_pos==1234.25f&&r.write_pos==1234u);
        for(i=0;i<40u;i++){
            float monitored=mi_rec_tick(&r,0.1f,edits);
            assert(monitored>=-1.f&&monitored<=1.f);
        }
        assert(r.count==MI_REC_CAPACITY);
        assert(mi_rec_sample(1234)!=old1234); /* new layer at current cursor */
        assert(mi_rec_sample(1280)==old1280); /* untouched future */
        assert(mi_rec_sample(10)==old10); /* never overwrite from zero */
        {
            float pos=r.playback_pos;
            mi_rec_toggle_record(&r); /* manual finish must not rewind */
            assert(!r.recording&&!r.overdubbing&&r.playing);
            assert(r.playback_pos==pos);
        }
    }
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
    /* MIDI-clock arming: an existing clip advances while waiting for the
     * next bar. Do not jump to zero when overdub actually begins. */
    mi_rec_init(&r);
    r.count=MI_REC_CAPACITY;r.playing=1u;r.play_step=0.5f;
    r.playback_pos=2345.75f;
    mi_rec_external_tick(&r,1000u);
    mi_rec_toggle_record(&r);
    assert(r.armed&&!r.recording&&r.playback_pos==2345.75f);
    mi_rec_external_tick(&r,2000u); /* next 96-tick boundary */
    assert(!r.recording&&r.armed);
    r.clock_ticks=96u; /* next incoming pulse starts the following bar */
    mi_rec_external_tick(&r,3000u);
    assert(r.recording&&r.overdubbing&&r.playing&&!r.armed);
    assert(r.write_pos==2345u&&r.playback_pos==2345.75f);
    {
        float pos=r.playback_pos;
        mi_rec_clock_stop(&r);
        assert(!r.recording&&!r.overdubbing&&r.playing);
        assert(r.playback_pos==pos);
    }
    /* Lost external clock while armed must also preserve the live head. */
    r.clock_active=1u;r.clock_last_sample=1u;
    r.playback_pos=3765.5f;r.playing=1u;
    mi_rec_toggle_record(&r);
    assert(r.armed);
    mi_rec_configure(&r,120u,2u,44100.f,50000u);
    assert(!r.clock_active&&!r.armed&&r.recording&&r.overdubbing);
    assert(r.write_pos==3765u&&r.playback_pos==3765.5f);
    /* MI20 reverse: read current PCM in reverse without allocating another
     * clip; overdub follows the reverse playhead, not the forward write head. */
    mi_rec_init(&r);
    mi_rec_configure(&r,120u,2u,44100.f,0u);
    mi_rec_toggle_record(&r);
    for(i=0u;i<8000u;i++)mi_rec_tick(&r,(i&256u)?0.3f:-0.3f,edits);
    mi_rec_toggle_record(&r);
    assert(r.playing&&r.count>1000u);
    mi_rec_set_transport(&r,0u,1u,0u);
    r.playback_pos=1234.25f;
    mi_rec_tick(&r,0.f,edits);
    assert(r.playback_pos<1234.25f&&r.tracks[0].reverse==1u);
    mi_rec_toggle_record(&r);
    assert(r.overdubbing&&r.write_pos==1234u); /* 0.25-point/sample at 120 BPM */
    for(i=0u;i<20u;i++)mi_rec_tick(&r,0.1f,edits);
    assert(r.write_pos<1233u);
    mi_rec_toggle_record(&r);
    assert(!r.recording&&r.playing);
    mi_rec_toggle_play(&r);
    assert(!r.playing);
    mi_rec_toggle_play(&r);
    assert(r.playing && r.playback_pos==(float)(r.count-1u));
    /* Internal clock: a pending beat STOP fires after 22,050 output samples
     * at 120 BPM, with the same sample counter used for playback. */
    mi_rec_set_transport(&r,0u,1u,1u);
    r.beat_phase=0.f;r.beat_index=0u;
    mi_rec_toggle_play(&r);
    assert(r.playing && r.tracks[0].pending_play==2u);
    for(i=0u;i<22049u;i++)mi_rec_tick(&r,0.f,edits);
    assert(r.playing&&r.tracks[0].pending_play==2u);
    mi_rec_tick(&r,0.f,edits);
    assert(!r.playing&&!r.tracks[0].pending_play);
    mi_rec_toggle_play(&r);
    assert(!r.playing&&r.tracks[0].pending_play==1u);
    /* A second PLAY press cancels the pending quantized start. */
    mi_rec_toggle_play(&r);
    assert(!r.tracks[0].pending_play);
    /* Starting the internal step sequencer defines beat 0 for all loops,
     * without waiting for a free-running grid from firmware boot. */
    mi_rec_toggle_play(&r);
    assert(r.tracks[0].pending_play==1u);
    mi_rec_internal_start(&r);
    assert(r.playing&&!r.tracks[0].pending_play);
    assert(r.beat_index==1u&&r.beat_phase==0.f);
    /* On external MIDI, BAR mode aligns to tick 1/97 (24 PPQN*4). */
    mi_rec_init(&r);
    r.count=8000u;
    mi_rec_set_transport(&r,0u,0u,2u);
    mi_rec_toggle_play(&r);
    assert(!r.playing && r.tracks[0].pending_play==1u);
    mi_rec_external_tick(&r,1000u);
    assert(r.playing&&!r.tracks[0].pending_play);
    mi_rec_toggle_play(&r);
    assert(r.playing&&r.tracks[0].pending_play==2u);
    for(i=2u;i<=96u;i++)mi_rec_external_tick(&r,i*1000u);
    assert(r.playing);
    mi_rec_external_tick(&r,97000u);
    assert(!r.playing&&!r.tracks[0].pending_play);
    mi_rec_set_transport(&r,0u,0u,1u);
    mi_rec_toggle_play(&r);
    assert(r.tracks[0].pending_play==1u);
    mi_rec_set_transport(&r,0u,0u,0u);
    assert(r.playing&&!r.tracks[0].pending_play);
    /* User-requested 1-bar first take: a manual STOP during bar 1
     * retains the original recorded sound and fills the remaining interval
     * with PCM7 zero (no time stretching). Existing overdubs keep length. */
    mi_rec_init(&r);
    mi_rec_configure(&r,120u,2u,44100.f,0u);
    mi_rec_toggle_record(&r);
    for(i=0u;i<44100u;i++)mi_rec_tick(&r,0.6f,edits); /* half bar */
    assert(r.count>10900u&&r.count<11200u);
    mi_rec_toggle_record(&r);
    assert(!r.recording&&r.playing&&r.count==22050u);
    assert(r.tracks[0].one_bar_loop==1u);
    assert(mi_rec_sample(100u)>10000);
    assert(mi_rec_sample(17000u)==0); /* the 1-bar silent tail */
    assert(r.play_step>0.24f&&r.play_step<0.26f);
    for(i=0u;i<88200u;i++)mi_rec_tick(&r,0.f,edits);
    assert(r.playback_pos<0.26f); /* exactly 1 bar at 120 BPM */
    /* Overdub into previously silent tail must work and never extend loop. */
    r.playback_pos=16500.f;
    mi_rec_toggle_record(&r);
    assert(r.overdubbing&&r.write_pos==16500u);
    for(i=0u;i<24u;i++)mi_rec_tick(&r,0.4f,edits);
    mi_rec_toggle_record(&r);
    assert(!r.recording&&r.count==22050u&&r.tracks[0].one_bar_loop==1u);
    assert(mi_rec_sample(16500u)!=0);
    assert(mi_rec_sample(17000u)==0);
    /* Following external MIDI tempo must be ONE bar (96 F8), not 192. */
    mi_rec_external_tick(&r,1000u);
    mi_rec_external_tick(&r,2000u);
    mi_rec_configure(&r,120u,2u,44100.f,2000u);
    assert(r.play_step>0.22f&&r.play_step<0.24f);
    /* 4-BAR capacity setting still gives the same one-bar duration. */
    mi_rec_init(&r);
    mi_rec_configure(&r,120u,4u,44100.f,0u);
    mi_rec_toggle_record(&r);
    for(i=0u;i<10000u;i++)mi_rec_tick(&r,0.4f,edits);
    mi_rec_toggle_record(&r);
    assert(r.count==11025u&&r.tracks[0].one_bar_loop==1u);
    assert(mi_rec_sample(9000u)==0);
    mi_rec_clear(&r);
    assert(r.count==0u&&!r.tracks[0].one_bar_loop);
    /* STOP exactly on the first bar boundary is also a one-bar loop. */
    mi_rec_init(&r);
    mi_rec_configure(&r,120u,2u,44100.f,0u);
    mi_rec_toggle_record(&r);
    for(i=0u;i<88200u;i++)mi_rec_tick(&r,0.3f,edits);
    assert(r.count==22050u);
    mi_rec_toggle_record(&r);
    assert(r.count==22050u&&r.tracks[0].one_bar_loop==1u);
    assert(mi_rec_sample(21900u)>0);
    /* A take longer than the first bar retains its exact manual length. */
    mi_rec_init(&r);
    mi_rec_configure(&r,120u,2u,44100.f,0u);
    mi_rec_toggle_record(&r);
    for(i=0u;i<120000u;i++)mi_rec_tick(&r,0.2f,edits);
    assert(r.count>22050u&&r.count<MI_REC_CAPACITY);
    {
        uint32_t original=r.count;
        mi_rec_toggle_record(&r);
        assert(!r.recording&&r.count==original&&!r.tracks[0].one_bar_loop);
    }
    puts("PASS: REC manual stop, adaptive 2/4 bars, 192 MIDI ticks, overdub, clear, pitch/trim, current-playhead overdub");
    return 0;
}
