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

    // Part 0: existing bowed-string violin. Keep its validated core untouched.
    violin_.prepare(sampleRate_);
    violin_.setBowPressure(0.58f);
    violin_.setBowSpeed(0.58f);
    violin_.setBowPosition(0.12f);
    violin_.setVibratoDepth(0.10f);

    // Parts 1..7 are permanently instantiated. Instrument selection now only
    // selects the foreground/UI part; it no longer destroys another part.
    for (int i=0; i<7; ++i) {
        modelParts_[i].prepare(sampleRate_);
        modelParts_[i].setType(i + 1);
        modelParts_[i].setControl(1, cc1_ / 127.0f);
        modelParts_[i].setControl(10, cc10_ / 127.0f);
        modelParts_[i].setControl(11, cc11_ / 127.0f);
        modelParts_[i].setControl(74, cc74_ / 127.0f);
    }

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

// Foreground/UI events use the currently selected instrument.
void AudioEngine::noteOn(int n,int v){ push({Event::NOTE_ON,n,v}); }
void AudioEngine::noteOff(int n,int v){ push({Event::NOTE_OFF,n,v}); }
void AudioEngine::polyPressure(int n,int p){ push({Event::POLY_AT,n,p}); }
void AudioEngine::channelPressure(int p){ push({Event::CH_AT,p,0}); }
void AudioEngine::controlChange(int c,int v){ push({Event::CC,c,v}); }
void AudioEngine::pitchBend(int v){ push({Event::PITCH,v,0}); }

// External MIDI events are already mapped to a part by MidiController.
void AudioEngine::noteOnPart(int part,int n,int v){ push({Event::PART_NOTE_ON,part,n,v,0}); }
void AudioEngine::noteOffPart(int part,int n,int v){ push({Event::PART_NOTE_OFF,part,n,v,0}); }
void AudioEngine::polyPressurePart(int part,int n,int p){ push({Event::PART_POLY_AT,part,n,p,0}); }
void AudioEngine::channelPressurePart(int part,int p){ push({Event::PART_CH_AT,part,p,0,0}); }
void AudioEngine::controlChangePart(int part,int c,int v){ push({Event::PART_CC,part,c,v,0}); }
void AudioEngine::pitchBendPart(int part,int v){ push({Event::PART_PITCH,part,v,0,0}); }

void AudioEngine::setDreamy(bool on){ push({Event::DREAMY,on?1:0,0,0}); }
void AudioEngine::setBoosterStep(int step){ push({Event::BOOST,step,0,0}); }
void AudioEngine::setBoostDb(int db){ push({Event::BOOST_DB,db,0,0}); }
void AudioEngine::setSpaceMode(int mode){ push({Event::SPACE_MODE,mode,0,0}); }
void AudioEngine::setSpaceParameters(int mix,int decay){ push({Event::SPACE_PARAMS,mix,decay,0}); }
void AudioEngine::setTape(bool on){ push({Event::TAPE,on?1:0,0,0}); }
void AudioEngine::setTapeParameters(int wow,int flutter,int drive){ push({Event::TAPE_PARAMS,wow,flutter,drive}); }
void AudioEngine::setDreamyParameters(int x,int y,int mix){ push({Event::DREAMY_PARAMS,x,y,mix,0}); }
void AudioEngine::setAdsr(int attackMs,int decayMs,int sustainPct,int releaseMs){
    push({Event::ADSR,attackMs,decayMs,sustainPct,releaseMs});
}
void AudioEngine::setInstrument(int instrument){
    push({Event::INSTRUMENT,instrument,0,0,0});
}

void AudioEngine::handlePartNoteOn(int part, int note, int velocity) {
    part = std::clamp(part, 0, 7);
    if (velocity <= 0) { handlePartNoteOff(part, note); return; }

    if (part == 0) violin_.noteOn(note, velocity);
    else modelParts_[part - 1].noteOn(note, velocity);
}

void AudioEngine::handlePartNoteOff(int part, int note) {
    part = std::clamp(part, 0, 7);
    const bool sustain = partSustain_[part] >= 64;
    if (part == 0) violin_.noteOff(note, sustain);
    else modelParts_[part - 1].noteOff(note, sustain);
}

void AudioEngine::handlePartPolyPressure(int part, int note, int pressure) {
    part = std::clamp(part, 0, 7);
    if (part == 0) violin_.polyPressure(note, pressure);
    else modelParts_[part - 1].polyPressure(note, pressure);
}

void AudioEngine::handlePartChannelPressure(int part, int pressure) {
    part = std::clamp(part, 0, 7);
    if (part == 0) violin_.channelPressure(pressure);
    else modelParts_[part - 1].channelPressure(pressure);
}

void AudioEngine::allNotesOffPart(int part) {
    part = std::clamp(part, 0, 7);
    if (part == 0) violin_.allNotesOff();
    else modelParts_[part - 1].allNotesOff();
}

void AudioEngine::handlePartPitchBend(int part, int value14) {
    part = std::clamp(part, 0, 7);
    value14 = std::clamp(value14, 0, 16383);
    partPitch_[part] = value14;

    if (part == 0) violin_.pitchBend(value14);
    else modelParts_[part - 1].pitchBend(value14);

    if (part == selectedInstrument_) pitch_ = value14;
}

void AudioEngine::handlePartControlChange(int part, int cc, int value) {
    part = std::clamp(part, 0, 7);
    value = std::clamp(value, 0, 127);

    // Per-part performance controls.
    if (cc == 7) {
        partVolume_[part] = value;
        if (part == selectedInstrument_) cc7_ = value;
    } else if (cc == 1) {
        if (part == 0) violin_.setVibratoDepth(value / 127.0f);
        else modelParts_[part - 1].setControl(1, value / 127.0f);
        if (part == selectedInstrument_) cc1_ = value;
    } else if (cc == 10) {
        if (part == 0) violin_.setBowPressure(value / 127.0f);
        else modelParts_[part - 1].setControl(10, value / 127.0f);
        if (part == selectedInstrument_) cc10_ = value;
    } else if (cc == 11) {
        if (part == 0) violin_.setBowSpeed(value / 127.0f);
        else modelParts_[part - 1].setControl(11, value / 127.0f);
        if (part == selectedInstrument_) cc11_ = value;
    } else if (cc == 64) {
        partSustain_[part] = value;
        const bool down = value >= 64;
        if (part == 0) violin_.sustainChanged(down);
        else modelParts_[part - 1].sustainChanged(down);
        if (part == selectedInstrument_) cc64_ = value;
    } else if (cc == 74) {
        if (part == 0) violin_.setBowPosition(value / 127.0f);
        else modelParts_[part - 1].setControl(74, value / 127.0f);
        if (part == selectedInstrument_) cc74_ = value;
    } else if (cc == 120 || cc == 123) {
        allNotesOffPart(part);
    }

    // Effects remain a shared mix bus, so these CCs are intentionally global
    // regardless of which assigned MIDI part sends them.
    if (cc == 20) {
        boostDb_=std::clamp(int(std::lround(value*6.0/127.0)),0,6);
    } else if (cc == 21) {
        space_.setMode(std::clamp(int(std::lround(value*3.0/127.0)),0,3));
    } else if (cc == 22) {
        spaceMix_=value/127.f; space_.setParameters(spaceMix_,spaceDecay_);
    } else if (cc == 23) {
        spaceDecay_=value/127.f; space_.setParameters(spaceMix_,spaceDecay_);
    } else if (cc == 24) {
        tape_.setEnabled(value>=64);
    } else if (cc == 25) {
        tapeWow_=value/127.f; tape_.setParameters(tapeWow_,tapeFlutter_,tapeDrive_);
    } else if (cc == 26) {
        tapeFlutter_=value/127.f; tape_.setParameters(tapeWow_,tapeFlutter_,tapeDrive_);
    } else if (cc == 27) {
        tapeDrive_=value/127.f; tape_.setParameters(tapeWow_,tapeFlutter_,tapeDrive_);
    } else if (cc == 28) {
        dreamy_.setEnabled(value>=64);
    } else if (cc == 103) {
        cc103_=value; dreamy_.setXY(cc103_/127.f, cc104_/127.f);
    } else if (cc == 104) {
        cc104_=value; dreamy_.setXY(cc103_/127.f, cc104_/127.f);
    } else if (cc == 105) {
        dreamyMix_=value/127.f;
        dreamy_.setParameters(cc103_/127.f, cc104_/127.f, dreamyMix_);
    }
}

float AudioEngine::processPart(int part) {
    part = std::clamp(part, 0, 7);
    const float raw = (part == 0) ? violin_.process() : modelParts_[part - 1].process();
    return raw * (partVolume_[part] / 127.0f);
}

int AudioEngine::activeVoicesPart(int part) const {
    part = std::clamp(part, 0, 7);
    return part == 0 ? violin_.activeVoices() : modelParts_[part - 1].activeVoices();
}

void AudioEngine::handle(const Event& e) {
    switch(e.type) {
        case Event::NOTE_ON:
            handlePartNoteOn(selectedInstrument_, e.a, e.b);
            break;
        case Event::NOTE_OFF:
            handlePartNoteOff(selectedInstrument_, e.a);
            break;
        case Event::POLY_AT:
            handlePartPolyPressure(selectedInstrument_, e.a, e.b);
            break;
        case Event::CH_AT:
            handlePartChannelPressure(selectedInstrument_, e.a);
            break;
        case Event::CC:
            handlePartControlChange(selectedInstrument_, e.a, e.b);
            break;
        case Event::PITCH:
            handlePartPitchBend(selectedInstrument_, e.a);
            break;

        case Event::PART_NOTE_ON:
            handlePartNoteOn(e.a, e.b, e.c);
            break;
        case Event::PART_NOTE_OFF:
            handlePartNoteOff(e.a, e.b);
            break;
        case Event::PART_POLY_AT:
            handlePartPolyPressure(e.a, e.b, e.c);
            break;
        case Event::PART_CH_AT:
            handlePartChannelPressure(e.a, e.b);
            break;
        case Event::PART_CC:
            handlePartControlChange(e.a, e.b, e.c);
            break;
        case Event::PART_PITCH:
            handlePartPitchBend(e.a, e.b);
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
        case Event::ADSR: {
            const float a = static_cast<float>(std::clamp(e.a,0,5000));
            const float d = static_cast<float>(std::clamp(e.b,0,5000));
            const float sus = std::clamp(e.c,0,100)/100.0f;
            const float r = static_cast<float>(std::clamp(e.d,0,5000));
            if (selectedInstrument_ == 0) violin_.setAdsr(a,d,sus,r);
            else modelParts_[selectedInstrument_ - 1].setAdsr(a,d,sus,r);
            break;
        }
        case Event::INSTRUMENT:
            selectedInstrument_ = std::clamp(e.a, 0, 7);
            cc7_ = partVolume_[selectedInstrument_];
            cc64_ = partSustain_[selectedInstrument_];
            pitch_ = partPitch_[selectedInstrument_];
            break;
    }
}

void AudioEngine::render(float* out,int32_t frames) {
    Event e;
    while (pop(e)) handle(e);

    for (int32_t i=0; i<frames; ++i) {
        float mono = 0.0f;
        int sounding = 0;
        for (int part=0; part<8; ++part) {
            mono += processPart(part);
            sounding += activeVoicesPart(part);
        }

        if (sounding > 1) mono *= 1.0f / std::sqrt(float(sounding));

        float l = mono;
        float r = mono;

        dreamy_.process(l,r);
        tape_.process(l,r);
        space_.process(l,r);

        const float boostGain = std::pow(10.0f, float(std::clamp(boostDb_,0,6)) / 20.0f);
        const float outGain = 1.55f * boostGain;

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
