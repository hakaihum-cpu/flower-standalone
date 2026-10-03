#include "AudioEngine.h"
#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <cstring>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"EPSampler",__VA_ARGS__)

AudioEngine& AudioEngine::instance() { static AudioEngine e; return e; }
AudioEngine::AudioEngine() { rrCounter_.fill(0); polyAT_.fill(0); }
AudioEngine::~AudioEngine() { stop(); }

bool AudioEngine::start() {
    if (stream_) return true;
    AAudioStreamBuilder* b=nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return false;
    AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setDataCallback(b, dataCallback, this);
    AAudioStreamBuilder_setErrorCallback(b, errorCallback, this);
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &stream_);
    if (r != AAUDIO_OK) {
        AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
        r = AAudioStreamBuilder_openStream(b, &stream_);
    }
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK || !stream_) { stream_=nullptr; return false; }
    sampleRate_ = AAudioStream_getSampleRate(stream_);
    dreamy_.prepare(sampleRate_);
    dreamy_.setXY(cc103_/127.f, cc104_/127.f);
    if (AAudioStream_requestStart(stream_) != AAUDIO_OK) { stop(); return false; }
    return true;
}

void AudioEngine::stop() {
    if (!stream_) return;
    AAudioStream_requestStop(stream_);
    AAudioStream_close(stream_);
    stream_=nullptr;
}

bool AudioEngine::loadBank(const std::string& path) {
    std::lock_guard<std::mutex> g(bankMutex_);
    for (auto& v: voices_) v.active=false;
    return bank_.load(path);
}

bool AudioEngine::loadBankFd(int fd) {
    std::lock_guard<std::mutex> g(bankMutex_);
    for (auto& v: voices_) v.active=false;
    return bank_.loadFd(fd);
}

void AudioEngine::push(Event e) {
    uint32_t w=write_.load(std::memory_order_relaxed);
    uint32_t n=(w+1)%QUEUE;
    if (n==read_.load(std::memory_order_acquire)) return;
    queue_[w]=e;
    write_.store(n,std::memory_order_release);
}
bool AudioEngine::pop(Event& e) {
    uint32_t r=read_.load(std::memory_order_relaxed);
    if (r==write_.load(std::memory_order_acquire)) return false;
    e=queue_[r];
    read_.store((r+1)%QUEUE,std::memory_order_release);
    return true;
}

void AudioEngine::noteOn(int n,int v){ push({Event::NOTE_ON,n,v}); }
void AudioEngine::noteOff(int n,int v){ push({Event::NOTE_OFF,n,v}); }
void AudioEngine::polyPressure(int n,int p){ push({Event::POLY_AT,n,p}); }
void AudioEngine::channelPressure(int p){ push({Event::CH_AT,p,0}); }
void AudioEngine::controlChange(int c,int v){ push({Event::CC,c,v}); }
void AudioEngine::pitchBend(int v){ push({Event::PITCH,v,0}); }
void AudioEngine::setDreamy(bool on){ push({Event::DREAMY,on?1:0,0}); }

void AudioEngine::handle(const Event& e) {
    switch(e.type) {
        case Event::NOTE_ON: beginVoice(e.a,e.b); break;
        case Event::NOTE_OFF: releaseVoice(e.a); break;
        case Event::POLY_AT:
            if(e.a>=0&&e.a<128){ polyAT_[e.a]=std::clamp(e.b,0,127); for(auto& v:voices_) if(v.active&&v.note==e.a) v.targetBodyVelocity=float(v.velocity)+(127.f-v.velocity)*(polyAT_[e.a]/127.f); }
            break;
        case Event::CH_AT:
            channelAT_=std::clamp(e.a,0,127);
            for(auto& v:voices_) if(v.active&&polyAT_[v.note]==0) v.targetBodyVelocity=float(v.velocity)+(127.f-v.velocity)*(channelAT_/127.f);
            break;
        case Event::CC:
            if(e.a==7) cc7_=std::clamp(e.b,0,127);
            else if(e.a==11) cc11_=std::clamp(e.b,0,127);
            else if(e.a==64){
                bool was=cc64_>=64; cc64_=std::clamp(e.b,0,127); bool now=cc64_>=64;
                if(was && !now) for(auto& v:voices_) if(v.active&&v.pendingRelease){v.pendingRelease=false;v.releasing=true;v.releaseFrame=0.0;}
            }
            else if(e.a==103){ cc103_=std::clamp(e.b,0,127); dreamy_.setXY(cc103_/127.f, cc104_/127.f); }
            else if(e.a==104){ cc104_=std::clamp(e.b,0,127); dreamy_.setXY(cc103_/127.f, cc104_/127.f); }
            break;
        case Event::PITCH: pitch_=std::clamp(e.a,0,16383); break;
        case Event::DREAMY: dreamy_.setEnabled(e.a!=0); break;
    }
}

void AudioEngine::beginVoice(int note,int velocity) {
    if(!bank_.loaded() || note<21 || note>108) return;
    Voice* pick=nullptr;
    for(auto& v:voices_) if(!v.active){pick=&v;break;}
    if(!pick) {
        pick=&voices_[0];
        for(auto& v:voices_) if(v.ageFrames > pick->ageFrames) pick=&v;
    }
    uint8_t rr = uint8_t((rrCounter_[note]++ % 3)+1);
    *pick = Voice{};
    pick->active=true; pick->note=note; pick->velocity=std::clamp(velocity,1,127); pick->rr=rr;
    pick->bodyVelocity=float(pick->velocity); pick->targetBodyVelocity=float(pick->velocity);
    int at = polyAT_[note] ? polyAT_[note] : channelAT_;
    pick->targetBodyVelocity=float(pick->velocity)+(127.f-pick->velocity)*(at/127.f);
    for(int i=0;i<8;i++){
        pick->sus[i]=bank_.find(uint8_t(note),uint8_t(VELS[i]),rr,SampleBank::SUSTAIN);
        pick->rel[i]=bank_.find(uint8_t(note),uint8_t(VELS[i]),rr,SampleBank::RELEASE);
    }
}

void AudioEngine::releaseVoice(int note) {
    for(auto& v:voices_) if(v.active&&v.note==note&&!v.releasing){
        if(cc64_>=64) v.pendingRelease=true;
        else {v.releasing=true;v.releaseFrame=0.0;}
    }
}

void AudioEngine::bracket(float v,int& lo,int& hi,float& mix) {
    if(v<=VELS[0]){lo=hi=0;mix=0;return;} if(v>=VELS[7]){lo=hi=7;mix=0;return;}
    for(int i=0;i<7;i++) if(v>=VELS[i]&&v<=VELS[i+1]){lo=i;hi=i+1;mix=(v-VELS[i])/float(VELS[i+1]-VELS[i]);return;}
    lo=hi=7;mix=0;
}

float AudioEngine::layerSample(const Voice& v,bool release,int layer,double frame,int ch) const {
    const auto* e = release ? v.rel[layer] : v.sus[layer];
    return bank_.read(e,frame,ch);
}

void AudioEngine::renderVoice(Voice& v,float& l,float& r) {
    if(!v.active) return;
    const int attackLock = int(0.250 * sampleRate_);
    if(v.ageFrames > attackLock) v.bodyVelocity += (v.targetBodyVelocity-v.bodyVelocity)*0.0025f;
    float requested = v.ageFrames <= attackLock ? float(v.velocity) : v.bodyVelocity;
    int lo,hi; float mix; bracket(requested,lo,hi,mix);
    double bendSemis = (double(pitch_)-8192.0)/8192.0*2.0;
    double ratio = std::pow(2.0,bendSemis/12.0) * (double(bank_.sampleRate())/double(sampleRate_));

    float sl=0,sr=0;
    if(!v.releasing){
        sl=layerSample(v,false,lo,v.frame,0)*(1-mix)+layerSample(v,false,hi,v.frame,0)*mix;
        sr=layerSample(v,false,lo,v.frame,1)*(1-mix)+layerSample(v,false,hi,v.frame,1)*mix;
        v.frame += ratio;
        const auto* endRef=v.sus[hi]?v.sus[hi]:v.sus[lo];
        if(!endRef || v.frame>=endRef->frames) v.active=false;
    } else {
        const int xf=std::max(1,int(0.020*sampleRate_));
        float x=std::clamp(float(v.releaseFrame)/float(xf),0.f,1.f);
        float bodyL=layerSample(v,false,lo,v.frame,0)*(1-mix)+layerSample(v,false,hi,v.frame,0)*mix;
        float bodyR=layerSample(v,false,lo,v.frame,1)*(1-mix)+layerSample(v,false,hi,v.frame,1)*mix;
        float relL=layerSample(v,true,lo,v.releaseFrame,0)*(1-mix)+layerSample(v,true,hi,v.releaseFrame,0)*mix;
        float relR=layerSample(v,true,lo,v.releaseFrame,1)*(1-mix)+layerSample(v,true,hi,v.releaseFrame,1)*mix;
        sl=bodyL*(1-x)+relL*x; sr=bodyR*(1-x)+relR*x;
        v.frame += ratio; v.releaseFrame += ratio;
        const auto* endRef=v.rel[hi]?v.rel[hi]:v.rel[lo];
        if(!endRef || v.releaseFrame>=endRef->frames) v.active=false;
    }
    float gain=(cc7_/127.f)*(cc11_/127.f);
    l += sl*gain; r += sr*gain;
    v.ageFrames++;
}

void AudioEngine::render(float* out,int32_t frames) {
    Event e; while(pop(e)) handle(e);
    std::lock_guard<std::mutex> g(bankMutex_);
    for(int32_t i=0;i<frames;i++){
        float l=0.f,r=0.f;
        for(auto& v:voices_) renderVoice(v,l,r);
        dreamy_.process(l,r);
        out[i*2]=std::tanh(l*0.82f);
        out[i*2+1]=std::tanh(r*0.82f);
    }
}

aaudio_data_callback_result_t AudioEngine::dataCallback(AAudioStream*,void* user,void* audio,int32_t n){
    auto* self=reinterpret_cast<AudioEngine*>(user);
    self->render(reinterpret_cast<float*>(audio),n);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
void AudioEngine::errorCallback(AAudioStream*,void*,aaudio_result_t error){ LOGE("AAudio error %d",error); }
