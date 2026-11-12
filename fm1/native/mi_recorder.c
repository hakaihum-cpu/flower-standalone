/* SPDX-License-Identifier: GPL-3.0-only
 * Three-track FM-1 REC, 44,100 sample points per track, 7-bit log-PCM.
 * No allocation, PCM clearing, block re-encoding or flash writes on each
 * audio sample. The three loop heads and overdub heads are independent.
 */
#include "mi_recorder.h"
#include "mi_memory.h"
#define MI_REC_BLOCK_MASK (MI_REC_PCM7_SAMPLES-1u)
/* The FM-1 linker has separate 96 KiB RAM and 344 KiB POOL.
 * Keep one loop in RAM, two in POOL: all 3 clips remain full 44,100 points,
 * without reducing FX history, SLICER, or recorder bit depth. */
static uint8_t clip_ram[MI_REC_PCM7_BLOCKS][MI_REC_PCM7_BLOCK_BYTES];
static uint8_t clip_pool[MI_REC_TRACKS-1u][MI_REC_PCM7_BLOCKS]
                        [MI_REC_PCM7_BLOCK_BYTES] __attribute__((section(".pool")));
static uint8_t *clip_block(unsigned tr,unsigned block)
{
    return tr==0u?clip_ram[block]:clip_pool[tr-1u][block];
}
static int8_t wave_min[MI_REC_TRACKS][224],wave_max[MI_REC_TRACKS][224];
static mi_recorder_t *latest;
static const float speed[7]={0.5f,0.667419927f,0.749153538f,
                             1.f,1.334839854f,1.498307077f,2.f};
static float boundf(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static unsigned track_id(const mi_recorder_t *r)
{
    return r->selected<MI_REC_TRACKS?r->selected:0u;
}
static void commit(mi_rec_track_t *t,unsigned tr)
{
    if(t->write_valid&&t->write_dirty){
        mi_pcm7_encode_64(clip_block(tr,t->write_block),t->write_pcm);
        t->read_valid=0u;
        t->write_dirty=0u;
    }
}
static int16_t get_pcm(mi_rec_track_t *t,unsigned tr,uint32_t pos)
{
    unsigned block;
    if(pos>=t->count||pos>=MI_REC_CAPACITY)return 0;
    block=(unsigned)pos/MI_REC_PCM7_SAMPLES;
    if(t->write_valid&&t->write_dirty&&t->write_block==block)
        return t->write_pcm[pos&MI_REC_BLOCK_MASK];
    if(!t->read_valid||t->read_block!=block){
        mi_pcm7_decode_64(t->read_pcm,clip_block(tr,block));
        t->read_block=(uint16_t)block;t->read_valid=1u;
    }
    return t->read_pcm[pos&MI_REC_BLOCK_MASK];
}
static void put_pcm(mi_rec_track_t *t,unsigned tr,uint32_t pos,int16_t value)
{
    unsigned block=(unsigned)pos/MI_REC_PCM7_SAMPLES;
    if(pos>=MI_REC_CAPACITY)return;
    if(!t->write_valid||t->write_block!=block){
        commit(t,tr);
        if(t->overdubbing&&t->count>=4u)
            mi_pcm7_decode_64(t->write_pcm,clip_block(tr,block));
        else mi_memzero(t->write_pcm,sizeof t->write_pcm);
        t->write_block=(uint16_t)block;
        t->write_valid=1u;t->write_dirty=0u;
    }
    t->write_pcm[pos&MI_REC_BLOCK_MASK]=value;
    t->write_dirty=1u;
    t->read_valid=0u;
}
static void clear_track(mi_rec_track_t *t,unsigned tr)
{
    uint8_t clock=t->clock_active,bars=t->bars;
    uint8_t reverse=t->reverse,quant=t->quant,tempo_ready=t->tempo_ready;
    uint32_t clock_ticks=t->clock_ticks;
    uint32_t clock_last_sample=t->clock_last_sample;
    float clock_period=t->clock_period;
    float capture_step=t->capture_step,play_step=t->play_step;
    mi_memzero(t,sizeof *t);
    t->clock_active=clock;t->clock_ticks=clock_ticks;
    t->clock_last_sample=clock_last_sample;t->clock_period=clock_period;
    t->bars=bars?bars:2u;
    t->reverse=reverse;t->quant=quant;t->tempo_ready=tempo_ready;
    t->capture_step=capture_step>0.f?capture_step:0.5f;
    t->play_step=play_step>0.f?play_step:0.5f;
    mi_memzero(wave_min[tr],sizeof wave_min[tr]);
    mi_memzero(wave_max[tr],sizeof wave_max[tr]);
}
int16_t mi_rec_sample(uint32_t sample)
{
    return latest?get_pcm(&latest->tracks[0],0u,sample):0;
}
void mi_rec_peak_track(unsigned tr,unsigned column,int8_t *lo,int8_t *hi)
{
    if(lo)*lo=tr<MI_REC_TRACKS&&column<224u?wave_min[tr][column]:0;
    if(hi)*hi=tr<MI_REC_TRACKS&&column<224u?wave_max[tr][column]:0;
}
void mi_rec_peak(unsigned column,int8_t *lo,int8_t *hi)
{
    mi_rec_peak_track(latest?track_id(latest):0u,column,lo,hi);
}
void mi_rec_init(mi_recorder_t *r)
{
    unsigned i;
    if(!r)return;
    mi_memzero(r,sizeof *r);
    for(i=0u;i<MI_REC_TRACKS;i++){
        r->tracks[i].bars=2u;
        r->tracks[i].capture_step=0.5f;
        r->tracks[i].play_step=0.5f;
        r->pan[i]=64u;r->level[i]=100u;
        mi_memzero(wave_min[i],sizeof wave_min[i]);
        mi_memzero(wave_max[i],sizeof wave_max[i]);
    }
    for(i=0u;i<2u;i++){
        r->params[i][0]=0u;r->params[i][1]=127u;
        r->params[i][2]=3u;r->params[i][3]=100u;
    }
    latest=r;
}
void mi_rec_select(mi_recorder_t *r,unsigned track)
{
    if(r&&track<MI_REC_TRACKS)r->selected=(uint8_t)track;
}
/* Pending requests belong to the audio thread and are consumed at a real
 * quarter-note boundary (4 quarters for bar mode). No extra PCM storage. */
static void quant_commit(mi_rec_track_t *t)
{
    if(t->pending_play==1u&&t->count>=4u){
        t->playback_pos=t->reverse?(float)(t->count-1u):0.f;
        t->playing=1u;
    }else if(t->pending_play==2u)t->playing=0u;
    t->pending_play=0u;
}
static void quant_boundary(mi_recorder_t *r,unsigned beat_index)
{
    unsigned tr;
    for(tr=0u;tr<MI_REC_TRACKS;tr++){
        mi_rec_track_t *t=&r->tracks[tr];
        if(t->pending_play &&
           (t->quant==1u||(t->quant==2u&&beat_index%4u==0u)))
            quant_commit(t);
    }
}
void mi_rec_internal_start(mi_recorder_t *r)
{
    if(!r || r->tracks[0].clock_active)return;
    r->beat_phase=0.f;
    r->beat_index=1u;
    quant_boundary(r,0u); /* first 16th step is also the bar downbeat */
}
void mi_rec_set_transport(mi_recorder_t *r,unsigned track,
                          unsigned reverse,unsigned quant)
{
    mi_rec_track_t *t;
    if(!r||track>=MI_REC_TRACKS)return;
    t=&r->tracks[track];
    t->reverse=(uint8_t)(reverse!=0u);
    t->quant=(uint8_t)(quant<=2u?quant:0u);
    if(t->pending_play&&t->quant==0u)quant_commit(t);
}
void mi_rec_set_track(mi_recorder_t *r,unsigned track,const uint8_t controls[4],
                      unsigned mute,unsigned pan)
{
    unsigned i;
    if(!r||track>=MI_REC_TRACKS||!controls)return;
    r->mute[track]=(uint8_t)(mute!=0u);
    r->pan[track]=(uint8_t)(pan>127u?127u:pan);
    r->level[track]=controls[3];
    if(track>0u)for(i=0u;i<4u;i++)r->params[track-1u][i]=controls[i];
}
int mi_rec_any_playing(const mi_recorder_t *r)
{
    unsigned i;
    if(!r)return 0;
    for(i=0u;i<MI_REC_TRACKS;i++)if(r->tracks[i].playing)return 1;
    return 0;
}
int mi_rec_any_audible(const mi_recorder_t *r)
{
    unsigned i;
    if(!r)return 0;
    for(i=0u;i<MI_REC_TRACKS;i++)
        if(r->tracks[i].playing&&r->tracks[i].count>=4u&&
           !r->mute[i]&&r->level[i]>0u)return 1;
    return 0;
}
int mi_rec_any_busy(const mi_recorder_t *r)
{
    unsigned i;
    if(!r)return 0;
    for(i=0u;i<MI_REC_TRACKS;i++)
        if(r->tracks[i].playing||r->tracks[i].recording||
           r->tracks[i].armed||r->tracks[i].pending_play)return 1;
    return 0;
}
static void finish(mi_rec_track_t *t,unsigned tr)
{
    uint8_t was_overdubbing=t->overdubbing;
    commit(t,tr);
    /* A manually stopped, tempo-configured first take shorter than one
     * measure becomes exactly one measure; its unused tail is SILENCE.
     * PCM7 silence is encoded as all-zero bytes. Only the final partly
     * written block is already zero-filled by put_pcm(), and all following
     * blocks can be cleared directly without re-encoding audio samples.
     * This adds no PCM allocation and never changes an overdubbed length. */
    if(!was_overdubbing && t->tempo_ready && t->count>=4u){
        uint32_t one_bar=MI_REC_CAPACITY/(t->bars==4u?4u:2u);
        if(t->count<one_bar){
            unsigned block=(unsigned)((t->count+MI_REC_PCM7_SAMPLES-1u)/
                                      MI_REC_PCM7_SAMPLES);
            unsigned final=(unsigned)((one_bar+MI_REC_PCM7_SAMPLES-1u)/
                                      MI_REC_PCM7_SAMPLES);
            for(;block<final;block++)
                mi_memzero(clip_block(tr,block),MI_REC_PCM7_BLOCK_BYTES);
            t->read_valid=0u;
            t->write_valid=0u;
            t->write_dirty=0u;
            t->count=one_bar;
            t->one_bar_loop=1u;
        }
    }
    t->recording=0u;t->overdubbing=0u;t->armed=0u;
    if(t->count>=4u){
        t->playing=1u;
        if(!was_overdubbing)
            t->playback_pos=t->reverse?(float)(t->count-1u):0.f;
    }
}
static void begin_overdub(mi_rec_track_t *t)
{
    if(t->playback_pos<0.f||t->playback_pos>=(float)t->count)
        t->playback_pos=0.f;
    t->write_pos=(uint32_t)t->playback_pos;
    t->write_valid=0u;t->write_dirty=0u;
    t->overdubbing=1u;t->recording=1u;t->playing=1u;
    t->capture_step=t->play_step;
    t->capture_sum=0.f;t->capture_samples=0u;t->accumulator=0.f;
}
void mi_rec_stop(mi_recorder_t *r)
{
    unsigned i;
    if(!r)return;
    for(i=0u;i<MI_REC_TRACKS;i++){
        mi_rec_track_t *t=&r->tracks[i];
        commit(t,i);
        t->armed=0u;t->recording=0u;t->playing=0u;
        t->overdubbing=0u;t->pending_play=0u;
    }
}
void mi_rec_clear(mi_recorder_t *r)
{
    unsigned tr;
    if(!r)return;
    tr=track_id(r);
    clear_track(&r->tracks[tr],tr);
}
void mi_rec_configure(mi_recorder_t *r,unsigned bpm,unsigned bars,
                      float sample_rate,uint32_t sample_clock)
{
    unsigned tr;
    if(!r)return;
    if(sample_rate<8000.f)sample_rate=44100.f;
    if(bpm<40u)bpm=40u;
    if(bpm>240u)bpm=240u;
    r->beat_samples=sample_rate*60.f/(float)bpm;
    for(tr=0u;tr<MI_REC_TRACKS;tr++){
        mi_rec_track_t *t=&r->tracks[tr];
        float step;
        if(t->clock_active && (uint32_t)(sample_clock-t->clock_last_sample)>
           (uint32_t)sample_rate){t->clock_active=0u;t->clock_period=0.f;}
        if(t->clock_active)t->bars=2u;
        else if(!t->recording&&!t->playing&&!t->armed)
            t->bars=(uint8_t)(bars==4u?4u:2u);
        step=(float)MI_REC_CAPACITY*(float)bpm/
             (t->bars*4.f*60.f*sample_rate);
        if(t->clock_active&&t->clock_period>1.f)
            step=(float)MI_REC_CAPACITY/(192.f*t->clock_period);
        step=boundf(step,0.001f,0.95f);
        t->tempo_ready=1u;
        if((!t->recording && !t->playing) ||
           (t->recording && !t->overdubbing))t->capture_step=step;
        if(!t->playing&&!t->recording&&!t->armed)t->play_step=step;
        if(t->clock_active && t->clock_period>1.f &&
           t->playing&&!t->recording&&t->count>=4u)
            t->play_step=(float)t->count/
                ((t->one_bar_loop?96.f:192.f)*t->clock_period);
        if(t->armed&&!t->clock_active){
            t->armed=0u;
            if(t->count>=4u)begin_overdub(t);
            else{clear_track(t,tr);t->recording=1u;}
        }
    }
}
void mi_rec_external_tick(mi_recorder_t *r,uint32_t sample_clock)
{
    unsigned tr;
    if(!r)return;
    for(tr=0u;tr<MI_REC_TRACKS;tr++){
        mi_rec_track_t *t=&r->tracks[tr];
        if(t->clock_active){
            uint32_t interval=(uint32_t)(sample_clock-t->clock_last_sample);
            if(interval>100u&&interval<22050u){
                if(t->clock_period<=1.f)t->clock_period=(float)interval;
                else t->clock_period=t->clock_period*0.8f+0.2f*(float)interval;
            }
        }
        t->clock_last_sample=sample_clock;
        t->clock_active=1u;
        ++t->clock_ticks;
        if(t->armed && ((t->clock_ticks-1u)%96u)==0u){
            uint8_t overdub=t->count>=4u;
            if(!overdub)clear_track(t,tr);
            t->armed=0u;
            if(overdub)begin_overdub(t);
            else{
                t->recording=1u;t->overdubbing=0u;t->playing=0u;
                t->write_pos=0u;t->playback_pos=0.f;
                t->capture_sum=0.f;t->capture_samples=0u;t->accumulator=0.f;
            }
            t->clock_start_tick=t->clock_ticks;
            if(!overdub&&t->clock_period>1.f)
                t->capture_step=boundf((float)MI_REC_CAPACITY/
                                     (192.f*t->clock_period),0.001f,0.95f);
        }else if(t->recording &&
                 (uint32_t)(t->clock_ticks-t->clock_start_tick)>=192u){
            finish(t,tr);
        }
    }
    /* Clock count 1 is MIDI Start's first downbeat; every 24 F8 pulses
     * form one quarter note, 96 pulses one bar. */
    if(((r->tracks[0].clock_ticks-1u)%24u)==0u)
        quant_boundary(r,(r->tracks[0].clock_ticks-1u)/24u);
}
void mi_rec_clock_stop(mi_recorder_t *r)
{
    unsigned tr;
    if(!r)return;
    for(tr=0u;tr<MI_REC_TRACKS;tr++){
        mi_rec_track_t *t=&r->tracks[tr];
        t->clock_active=0u;t->clock_period=0.f;t->armed=0u;
        if(t->recording)finish(t,tr);
    }
}
void mi_rec_toggle_record(mi_recorder_t *r)
{
    mi_rec_track_t *t;unsigned tr;
    if(!r)return;
    tr=track_id(r);t=&r->tracks[tr];
    /* REC is an explicit operation: cancel an earlier queued PLAY/STOP
     * so the quantizer cannot halt a newly started overdub. */
    t->pending_play=0u;
    if(t->armed){t->armed=0u;return;}
    if(t->recording){finish(t,tr);return;}
    if(t->clock_active){t->armed=1u;return;}
    if(t->count>=4u){begin_overdub(t);return;}
    clear_track(t,tr);
    t->recording=1u;
}
void mi_rec_toggle_play(mi_recorder_t *r)
{
    mi_rec_track_t *t;unsigned tr;
    if(!r)return;
    tr=track_id(r);t=&r->tracks[tr];
    if(t->recording)finish(t,tr);
    t->armed=0u;
    if(t->count<4u){
        t->playing=0u;t->pending_play=0u;return;
    }
    /* A second press cancels a pending action; it cannot reverse a queue. */
    if(t->pending_play){t->pending_play=0u;return;}
    t->pending_play=t->playing?2u:1u;
    if(!t->quant)quant_commit(t);
}
static float tick_track(mi_rec_track_t *t,unsigned tr,float input,
                        const uint8_t controls[4])
{
    uint32_t first,last,i,next,fade;
    float pos,fraction,out=0.f,ratio,level;
    if(t->playing&&t->count>=4u){
        first=(t->count-1u)*controls[0]/127u;
        last=(t->count-1u)*controls[1]/127u+1u;
        if(last>t->count)last=t->count;
        if(first+2u>last)t->playing=0u;
        else{
            if(t->playback_pos<(float)first||t->playback_pos>=(float)last)
                t->playback_pos=t->reverse?(float)(last-1u):(float)first;
            pos=t->playback_pos;i=(uint32_t)pos;
            next=i+1u<last?i+1u:first;
            fraction=pos-(float)i;
            out=((float)get_pcm(t,tr,i)+
                 ((float)get_pcm(t,tr,next)-(float)get_pcm(t,tr,i))*fraction)*
                (1.f/32767.f);
            fade=(last-first)/8u;
            if(fade>128u)fade=128u;
            if(t->reverse&&fade>=2u&&pos<(float)(first+fade)){
                float tail=(float)(last-fade)+pos-(float)first;
                uint32_t j=(uint32_t)tail;
                float part=tail-(float)j;
                uint32_t after=j+1u<last?j+1u:first;
                float end=((float)get_pcm(t,tr,j)+
                           ((float)get_pcm(t,tr,after)-
                            (float)get_pcm(t,tr,j))*part)*(1.f/32767.f);
                float mix=((float)(first+fade)-pos)/(float)fade;
                out=out*(1.f-mix)+end*mix;
            }else if(!t->reverse&&fade>=2u&&pos>=(float)(last-fade)){
                float headpos=(float)first+pos-(float)(last-fade);
                uint32_t j=(uint32_t)headpos;
                float part=headpos-(float)j;
                float head=((float)get_pcm(t,tr,j)+
                           ((float)get_pcm(t,tr,j+1u)-(float)get_pcm(t,tr,j))*part)*
                           (1.f/32767.f);
                float mix=(pos-(float)(last-fade))/(float)fade;
                out=out*(1.f-mix)+head*mix;
            }
            ratio=speed[controls[2]<=6u?controls[2]:3u];
            if(t->reverse){
                t->playback_pos-=ratio*t->play_step;
                if(t->playback_pos<(float)first){
                    t->playback_pos=(float)(last-1u)-
                        ((float)first-t->playback_pos);
                    if(t->playback_pos<(float)first)
                        t->playback_pos=(float)(last-1u);
                }
            }else{
                t->playback_pos+=ratio*t->play_step;
                if(t->playback_pos>=(float)last){
                    t->playback_pos=(float)(first+fade)+(t->playback_pos-(float)last);
                    if(t->playback_pos>=(float)last)
                        t->playback_pos=(float)(first+fade);
                }
            }
        }
    }
    if(t->recording){
        t->capture_sum+=input;
        if(t->capture_samples<65535u)++t->capture_samples;
        t->accumulator+=t->capture_step;
        if(t->accumulator>=1.f){
            float val=boundf(t->capture_sum/(float)t->capture_samples,-1.f,1.f);
            t->capture_sum=0.f;t->capture_samples=0u;
            t->accumulator-=1.f;
            if(t->write_pos<MI_REC_CAPACITY){
                uint32_t col=t->write_pos*224u/MI_REC_CAPACITY;
                int32_t amp;
                if(t->overdubbing&&t->count>=4u)
                    val=boundf((float)get_pcm(t,tr,t->write_pos)*
                               (1.f/32767.f)+val,-1.f,1.f);
                amp=(int32_t)(val*110.f);
                if(amp>110)amp=110;
                if(amp< -110)amp=-110;
                if(amp<wave_min[tr][col])wave_min[tr][col]=(int8_t)amp;
                if(amp>wave_max[tr][col])wave_max[tr][col]=(int8_t)amp;
                put_pcm(t,tr,t->write_pos,(int16_t)(val*32767.f));
                if(t->overdubbing&&t->reverse){
                    t->write_pos=t->write_pos? t->write_pos-1u:t->count-1u;
                }else ++t->write_pos;
                if(!t->overdubbing)t->count=t->write_pos;
            }
            if(t->overdubbing){
                if(t->write_pos>=t->count)t->write_pos=0u;
            }else if(t->write_pos>=MI_REC_CAPACITY){
                t->play_step=t->capture_step;
                if(!t->clock_active)finish(t,tr);
            }
        }
    }
    if(t->recording&&!t->overdubbing)t->play_step=t->capture_step;
    level=boundf((float)controls[3]/127.f,0.f,1.f);
    return boundf(out*level,-1.f,1.f);
}
void mi_rec_tick_stereo(mi_recorder_t *r,float input,
                        const uint8_t selected_controls[4],float *left,float *right)
{
    float l=0.f,rr=0.f;unsigned tr;
    if(left)*left=0.f;
    if(right)*right=0.f;
    if(!r||!selected_controls)return;
    if(!r->tracks[0].clock_active && r->beat_samples>=16.f){
        r->beat_phase+=1.f;
        if(r->beat_phase>=r->beat_samples){
            r->beat_phase-=r->beat_samples;
            quant_boundary(r,r->beat_index);
            r->beat_index=(uint8_t)((r->beat_index+1u)%4u);
        }
    }
    for(tr=0u;tr<MI_REC_TRACKS;tr++){
        const uint8_t *controls=tr==0u?selected_controls:r->params[tr-1u];
        float x=tick_track(&r->tracks[tr],tr,input,controls);
        unsigned pan=r->pan[tr];
        if(r->mute[tr])continue;
        /* Balance pan: centre retains the previous mono playback level. */
        l+=x*(pan<=64u?1.f:(float)(127u-pan)/63.f);
        rr+=x*(pan>=64u?1.f:(float)pan/64.f);
    }
    /* Do not hard-clip the sum of three loop tracks. The stereo output
     * mixer applies its smooth limiter after combining OSC and FMDrum. */
    if(left)*left=l;
    if(right)*right=rr;
}
float mi_rec_tick(mi_recorder_t *r,float input,const uint8_t controls[4])
{
    float l=0.f,rr=0.f;
    mi_rec_tick_stereo(r,input,controls,&l,&rr);
    return (l+rr)*0.5f;
}
