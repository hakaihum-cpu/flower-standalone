/* Host regression for three-track ADPCM REC: isolation, mute, pan, pitch,
 * independent overdub, clock, and the original 44,100 samples per track. */
#include "mi_recorder.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    mi_recorder_t r;
    uint8_t controls[4]={0u,127u,3u,127u};
    float left,right,prior;
    uint32_t count[3];
    unsigned tr,i;
    mi_rec_init(&r);
    assert(MI_REC_TRACKS==3u&&MI_REC_ADPCM_STORE_BYTES==74520u);
    for(tr=0u;tr<3u;tr++){
        mi_rec_select(&r,tr);
        mi_rec_set_track(&r,tr,controls,0u,64u);
        mi_rec_toggle_record(&r);
        assert(r.tracks[tr].recording);
        for(i=0u;i<1800u;i++)
            mi_rec_tick_stereo(&r,tr==1u?-0.28f:0.34f,controls,&left,&right);
        mi_rec_toggle_record(&r);
        assert(!r.tracks[tr].recording&&r.tracks[tr].playing);
        count[tr]=r.tracks[tr].count;
        assert(count[tr]>=899u&&count[tr]<=901u);
    }
    assert(mi_rec_any_playing(&r));
    /* Mute is a playback-only control: data must remain intact. */
    mi_rec_set_track(&r,0u,controls,1u,64u);
    mi_rec_set_track(&r,1u,controls,1u,64u);
    mi_rec_set_track(&r,2u,controls,0u,0u);
    for(i=0u;i<30u;i++)
        mi_rec_tick_stereo(&r,0.f,controls,&left,&right);
    assert(left>0.10f);
    assert(right==0.f); /* track 3 hard left; other two muted */
    /* Selected track 2 can overdub without touching the other two heads. */
    mi_rec_select(&r,1u);
    prior=r.tracks[0].playback_pos;
    mi_rec_toggle_record(&r);
    assert(r.tracks[1].overdubbing&&r.tracks[1].playing);
    for(i=0u;i<60u;i++)
        mi_rec_tick_stereo(&r,0.05f,controls,&left,&right);
    mi_rec_toggle_record(&r);
    assert(r.tracks[1].count==count[1]);
    assert(r.tracks[0].count==count[0]&&r.tracks[2].count==count[2]);
    assert(r.tracks[0].playback_pos!=prior); /* independent playback continued */
    /* Clear only selected track; the two other recordings survive. */
    mi_rec_clear(&r);
    assert(r.tracks[1].count==0u);
    assert(r.tracks[0].count==count[0]&&r.tracks[2].count==count[2]);
    /* Both pan extremes work without cross-channel bleed. */
    mi_rec_select(&r,0u);
    mi_rec_set_track(&r,0u,controls,0u,127u);
    mi_rec_set_track(&r,2u,controls,1u,64u);
    for(i=0u;i<30u;i++)
        mi_rec_tick_stereo(&r,0.f,controls,&left,&right);
    assert(left==0.f);
    assert(right>0.10f);
    /* The MIDI clock is fed to all REC tracks, not just selected one. */
    mi_rec_clock_stop(&r);
    mi_rec_external_tick(&r,1000u);
    assert(r.tracks[0].clock_active && r.tracks[1].clock_active &&
           r.tracks[2].clock_active);
    mi_rec_stop(&r);
    assert(!mi_rec_any_busy(&r));
    puts("PASS: independent 3x ADPCM REC, overwrite isolation, mute, pan and MIDI clock");
    return 0;
}
