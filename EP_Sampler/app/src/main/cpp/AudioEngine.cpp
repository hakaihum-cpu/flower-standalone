#include "AudioEngine.h"
#include <android/log.h>
#include <algorithm>
#include <cmath>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"EPViolin",__VA_ARGS__)

AudioEngine& AudioEngine::instance() { static AudioEngine e; return e; }
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

    if (r != AAUDIO_OK || !stream_) {
        stream_ = nullptr;
        return false;
    }

    sampleRate_ = AAudioStream_getSampleRate(stream_);
    violin_.prepare(sampleRate_);
    violin_.setBowPressure(0.58f);
    violin_.setBowSpeed(0.58f);
    violin_.setBowPosition(0.12f);
    violin_.setVibratoDepth(0.10f);

    dreamy_.prepare(sampleRate_);
    dreamy_.setXY(cc103_/127.f, cc104_/127.f);
    space_.prepare(sampleRate_);
    tape_.prepare(sampleRate_);
    space_.setParameters(spaceMix_, spaceDecay_);
    tape_.setParameters(tapeWow_, tapeFlutter_, tapeDrive_);
    dreamy_.setParameters(cc103_/127.f, cc104_/127.f, dreamyMix_);

    if (AAudioStream_requestStart(stream_) != AAUDIO_OK) {
        stop();
        return false;
    }
    return true;
}

void AudioEngine::stop() {
    if (!stream_) return;
    AAudioStream_requestStop(stream_);
    AAudioStream_close(stream_);
    stream_ = nullptr;
}

void AudioEngine::push(Event e) {
    const uint32_t w = write_.load(std::memory_order_relaxed);
    const uint32_t n = (w + 1) % QUEUE;
    if (n == read_.load(std::memory_order_acquire)) return;
    queue_[w] = e;
    write_.store(n, std::memory_order_release);
}

bool AudioEngine::pop(Event& e) {
    const uint32_t r = read_.load(std::memory_order_relaxed);
    if (r == write_.load(std::memory_order_acquire)) return false;
    e = queue_[r];
    read_.store((r + 1) % QUEUE, std::memory_order_release);
    return true;
}

void AudioEngine::noteOn(int n,int v){ push({Event::NOTE_ON,n,v}); }
void AudioEngine::noteOff(int n,int v){ push({Event::NOTE_OFF,n,v}); }
void AudioEngine::polyPressure(int n,int p){ push({Event::POLY_AT,n,p}); }
void AudioEngine::channelPressure(int p){ push({Event::CH_AT,p,0}); }
void AudioEngine::controlChange(int c,int v){ push({Event::CC,c,v}); }
void AudioEngine::pitchBend(int v){ push({Event::PITCH,v,0}); }
void AudioEngine::setDreamy(bool on){ push({Event::DREAMY,on?1:0,0,0}); }
void AudioEngine::setBoosterStep(int step){ push({Event::BOOST,step,0,0}); }
void AudioEngine::setBoostDb(int db){ push({Event::BOOST_DB,db,0,0}); }
void AudioEngine::setSpaceMode(int mode){ push({Event::SPACE_MODE,mode,0,0}); }
void AudioEngine::setSpaceParameters(int mix,int decay){ push({Event::SPACE_PARAMS,mix,decay,0}); }
void AudioEngine::setTape(bool on){ push({Event::TAPE,on?1:0,0,0}); }
void AudioEngine::setTapeParameters(int wow,int flutter,int drive){ push({Event::TAPE_PARAMS,wow,flutter,drive}); }
void AudioEngine::setDreamyParameters(int x,int y,int mix){ push({Event::DREAMY_PARAMS,x,y,mix}); }

void AudioEngine::handle(const Event& e) {
    switch(e.type) {
        case Event::NOTE_ON:
            if (e.b <= 0) violin_.noteOff(e.a, cc64_ >= 64);
            else violin_.noteOn(e.a, e.b);
            break;
        case Event::NOTE_OFF:
            violin_.noteOff(e.a, cc64_ >= 64);
            break;
        case Event::POLY_AT:
            violin_.polyPressure(e.a, e.b);
            break;
        case Event::CH_AT:
            violin_.channelPressure(e.a);
            break;
        case Event::CC:
            if (e.a == 7) {
                cc7_ = std::clamp(e.b, 0, 127);
            } else if (e.a == 1) {
                violin_.setVibratoDepth(std::clamp(e.b,0,127) / 127.0f);
            } else if (e.a == 10) {
                violin_.setBowPressure(std::clamp(e.b,0,127) / 127.0f);
            } else if (e.a == 11) {
                violin_.setBowSpeed(std::clamp(e.b,0,127) / 127.0f);
            } else if (e.a == 64) {
                cc64_ = std::clamp(e.b,0,127);
                violin_.sustainChanged(cc64_ >= 64);
            } else if (e.a == 74) {
                violin_.setBowPosition(std::clamp(e.b,0,127) / 127.0f);
            } else if (e.a == 120 || e.a == 123) {
                violin_.allNotesOff();
            } else if (e.a==20) {
                boostDb_=std::clamp(int(std::lround(e.b*6.0/127.0)),0,6);
            } else if (e.a==21) {
                space_.setMode(std::clamp(int(std::lround(e.b*3.0/127.0)),0,3));
            } else if (e.a==22) {
                spaceMix_=e.b/127.f; space_.setParameters(spaceMix_,spaceDecay_);
            } else if (e.a==23) {
                spaceDecay_=e.b/127.f; space_.setParameters(spaceMix_,spaceDecay_);
            } else if (e.a==24) {
                tape_.setEnabled(e.b>=64);
            } else if (e.a==25) {
                tapeWow_=e.b/127.f; tape_.setParameters(tapeWow_,tapeFlutter_,tapeDrive_);
            } else if (e.a==26) {
                tapeFlutter_=e.b/127.f; tape_.setParameters(tapeWow_,tapeFlutter_,tapeDrive_);
            } else if (e.a==27) {
                tapeDrive_=e.b/127.f; tape_.setParameters(tapeWow_,tapeFlutter_,tapeDrive_);
            } else if (e.a==28) {
                dreamy_.setEnabled(e.b>=64);
            } else if (e.a==103) {
                cc103_=std::clamp(e.b,0,127);
                dreamy_.setXY(cc103_/127.f, cc104_/127.f);
            } else if (e.a==104) {
                cc104_=std::clamp(e.b,0,127);
                dreamy_.setXY(cc103_/127.f, cc104_/127.f);
            } else if (e.a==105) {
                dreamyMix_=e.b/127.f;
                dreamy_.setParameters(cc103_/127.f, cc104_/127.f, dreamyMix_);
            }
            break;
        case Event::PITCH:
            pitch_=std::clamp(e.a,0,16383);
            violin_.pitchBend(pitch_);
            break;
        case Event::DREAMY:
            dreamy_.setEnabled(e.a!=0);
            break;
        case Event::BOOST:
            boosterStep_=std::clamp(e.a,0,3);
            boostDb_=boosterStep_*2;
            break;
        case Event::BOOST_DB:
            boostDb_=std::clamp(e.a,0,6);
            break;
        case Event::SPACE_MODE:
            space_.setMode(e.a);
            break;
        case Event::SPACE_PARAMS:
            spaceMix_=std::clamp(e.a,0,100)/100.f;
            spaceDecay_=std::clamp(e.b,0,100)/100.f;
            space_.setParameters(spaceMix_,spaceDecay_);
            break;
        case Event::TAPE:
            tape_.setEnabled(e.a!=0);
            break;
        case Event::TAPE_PARAMS:
            tapeWow_=std::clamp(e.a,0,100)/100.f;
            tapeFlutter_=std::clamp(e.b,0,100)/100.f;
            tapeDrive_=std::clamp(e.c,0,100)/100.f;
            tape_.setParameters(tapeWow_,tapeFlutter_,tapeDrive_);
            break;
        case Event::DREAMY_PARAMS:
            cc103_=std::clamp(int(std::lround(std::clamp(e.a,0,100)*1.27f)),0,127);
            cc104_=std::clamp(int(std::lround(std::clamp(e.b,0,100)*1.27f)),0,127);
            dreamyMix_=std::clamp(e.c,0,100)/100.f;
            dreamy_.setParameters(cc103_/127.f,cc104_/127.f,dreamyMix_);
            break;
    }
}

void AudioEngine::render(float* out,int32_t frames) {
    Event e;
    while (pop(e)) handle(e);

    for (int32_t i=0; i<frames; ++i) {
        const float mono = violin_.process();
        float l = mono;
        float r = mono;

        dreamy_.process(l,r);
        tape_.process(l,r);
        space_.process(l,r);

        const float volume = cc7_ / 127.0f;
        const float boostGain = std::pow(10.0f, float(std::clamp(boostDb_,0,6)) / 20.0f);
        const float outGain = 1.55f * volume * boostGain;

        l = std::isfinite(l) ? std::tanh(l * outGain) : 0.0f;
        r = std::isfinite(r) ? std::tanh(r * outGain) : 0.0f;
        out[i*2] = l;
        out[i*2+1] = r;
    }
}

aaudio_data_callback_result_t AudioEngine::dataCallback(
        AAudioStream*, void* user, void* audio, int32_t n) {
    auto* self = reinterpret_cast<AudioEngine*>(user);
    self->render(reinterpret_cast<float*>(audio), n);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AudioEngine::errorCallback(AAudioStream*,void*,aaudio_result_t error) {
    LOGE("AAudio error %d", error);
}
