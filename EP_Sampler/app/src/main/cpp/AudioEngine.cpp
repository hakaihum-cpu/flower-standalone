#include "AudioEngine.h"
#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"EPViolin",__VA_ARGS__)

AudioEngine& AudioEngine::instance() { static AudioEngine e; return e; }

AudioEngine::AudioEngine() {
    for (auto& active : epActiveBank_) active.store(0, std::memory_order_relaxed);
    for (auto& pending : epPendingBank_) pending.store(-1, std::memory_order_relaxed);
}

AudioEngine::~AudioEngine() { stop(); }

bool AudioEngine::loadBank(const std::string& path) {
    std::lock_guard<std::mutex> lock(epBankLoadMutex_[0]);
    const int active = epActiveBank_[0].load(std::memory_order_acquire);
    const int inactive = 1 - active;
    if (!epBanks_[0][inactive].load(path)) return false;
    epPendingBank_[0].store(inactive, std::memory_order_release);
    return true;
}

bool AudioEngine::loadBankFd(int fd) {
    return loadBankSlotFd(0, fd);
}

bool AudioEngine::loadBankSlotFd(int slot, int fd) {
    slot = std::clamp(slot, 0, SAMPLE_BANK_COUNT - 1);
    std::lock_guard<std::mutex> lock(epBankLoadMutex_[slot]);

    const int active = epActiveBank_[slot].load(std::memory_order_acquire);
    const int inactive = 1 - active;

    // File I/O + mmap + EPBANK1 validation happen only on the caller's
    // background thread. The audio callback never waits on this mutex.
    if (!epBanks_[slot][inactive].loadFd(fd)) return false;

    // The actual bank flip happens at the next audio-buffer boundary.
    epPendingBank_[slot].store(inactive, std::memory_order_release);
    return true;
}

bool AudioEngine::bankLoaded(int slot) const {
    slot = std::clamp(slot, 0, SAMPLE_BANK_COUNT - 1);
    const int index = epActiveBank_[slot].load(std::memory_order_acquire);
    return epBanks_[slot][index].loaded();
}

std::string AudioEngine::bankStatus(int slot) const {
    slot = std::clamp(slot, 0, SAMPLE_BANK_COUNT - 1);
    const int index = epActiveBank_[slot].load(std::memory_order_acquire);
    return epBanks_[slot][index].status();
}


namespace {
uint16_t readLe16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
uint32_t readLe32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
bool fourcc(const uint8_t* p, const char* s) {
    return p[0] == uint8_t(s[0]) && p[1] == uint8_t(s[1]) &&
           p[2] == uint8_t(s[2]) && p[3] == uint8_t(s[3]);
}
}

bool AudioEngine::loadDrumSample(int slot, const uint8_t* data, size_t size) {
    if (slot < 0 || slot >= DRUM_SAMPLE_COUNT || data == nullptr || size < 44) return false;
    if (!fourcc(data, "RIFF") || !fourcc(data + 8, "WAVE")) return false;

    uint16_t format = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t blockAlign = 0;
    uint16_t bits = 0;
    const uint8_t* pcm = nullptr;
    size_t pcmBytes = 0;

    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint8_t* chunk = data + pos;
        const uint32_t chunkSize = readLe32(chunk + 4);
        const size_t payload = pos + 8;
        if (payload + chunkSize > size) break;

        if (fourcc(chunk, "fmt ") && chunkSize >= 16) {
            const uint8_t* p = data + payload;
            format = readLe16(p + 0);
            channels = readLe16(p + 2);
            sampleRate = readLe32(p + 4);
            blockAlign = readLe16(p + 12);
            bits = readLe16(p + 14);
        } else if (fourcc(chunk, "data")) {
            pcm = data + payload;
            pcmBytes = chunkSize;
        }

        pos = payload + chunkSize + (chunkSize & 1u);
    }

    if (!pcm || channels < 1 || channels > 2 || sampleRate == 0 || blockAlign == 0) return false;
    if (!((format == 1 && (bits == 16 || bits == 24 || bits == 32)) ||
          (format == 3 && bits == 32))) return false;

    const size_t frames = pcmBytes / blockAlign;
    if (frames == 0) return false;

    DrumSample decoded;
    decoded.sampleRate = int(sampleRate);
    decoded.left.resize(frames);
    decoded.right.resize(frames);

    const int bytesPerSample = bits / 8;
    for (size_t i = 0; i < frames; ++i) {
        const uint8_t* frame = pcm + i * blockAlign;

        auto decode = [&](int ch) -> float {
            const uint8_t* p = frame + ch * bytesPerSample;
            if (format == 3 && bits == 32) {
                float v = 0.0f;
                std::memcpy(&v, p, sizeof(float));
                return std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f;
            }
            if (bits == 16) {
                int16_t v = int16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
                return float(v) / 32768.0f;
            }
            if (bits == 24) {
                int32_t v = int32_t(p[0]) | (int32_t(p[1]) << 8) | (int32_t(p[2]) << 16);
                if (v & 0x00800000) v |= ~0x00FFFFFF;
                return float(v) / 8388608.0f;
            }
            int32_t v = int32_t(readLe32(p));
            return float(double(v) / 2147483648.0);
        };

        const float l = decode(0);
        const float r = channels == 2 ? decode(1) : l;
        decoded.left[i] = l;
        decoded.right[i] = r;
    }

    decoded.loaded = true;
    drumSamples_[slot] = std::move(decoded);
    return true;
}

void AudioEngine::triggerDrumSample(int slot, int velocity) {
    if (slot < 0 || slot >= DRUM_SAMPLE_COUNT) return;
    if (!drumSamples_[slot].loaded) return;

    int voice = -1;
    for (int i = 0; i < DRUM_SAMPLE_VOICES; ++i) {
        if (!drumSampleVoices_[i].active) {
            voice = i;
            break;
        }
    }
    if (voice < 0) {
        voice = drumSampleSteal_;
        drumSampleSteal_ = (drumSampleSteal_ + 1) % DRUM_SAMPLE_VOICES;
    }

    auto& v = drumSampleVoices_[voice];
    v.active = true;
    v.slot = slot;
    v.position = 0.0;
    v.gain = 0.25f + 0.75f * (std::clamp(velocity, 1, 127) / 127.0f);
}

void AudioEngine::stopDrumSamples() {
    for (auto& v : drumSampleVoices_) v = DrumSampleVoice{};
}

int AudioEngine::activeDrumSampleVoices() const {
    int n = 0;
    for (const auto& v : drumSampleVoices_) if (v.active) ++n;
    return n;
}

void AudioEngine::processDrumSamples(float& left, float& right) {
    const double outputRate = std::max(1, sampleRate_);

    for (auto& v : drumSampleVoices_) {
        if (!v.active || v.slot < 0 || v.slot >= DRUM_SAMPLE_COUNT) continue;
        const auto& sample = drumSamples_[v.slot];
        if (!sample.loaded || sample.left.empty()) {
            v.active = false;
            continue;
        }

        const size_t i0 = size_t(v.position);
        if (i0 >= sample.left.size()) {
            v.active = false;
            continue;
        }

        const size_t i1 = std::min(i0 + 1, sample.left.size() - 1);
        const float frac = float(v.position - double(i0));
        const float l = sample.left[i0] + (sample.left[i1] - sample.left[i0]) * frac;
        const float r = sample.right[i0] + (sample.right[i1] - sample.right[i0]) * frac;

        const float partGain = partVolume_[7] / 127.0f;
        left += l * v.gain * partGain;
        right += r * v.gain * partGain;

        v.position += double(sample.sampleRate) / outputRate;
        if (v.position >= double(sample.left.size())) v.active = false;
    }
}

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
    defaultBufferSizeFrames_ = AAudioStream_getBufferSizeInFrames(stream_);
    requestedBufferBursts_ = 0.0f;

    const int delayBufferSize = std::max(4096, sampleRate_ * 2);
    performanceDelayL_.assign(delayBufferSize, 0.0f);
    performanceDelayR_.assign(delayBufferSize, 0.0f);
    performanceDelayWrite_ = 0;

    const int stutterBufferSize = std::max(4096, sampleRate_);
    stutterHistoryL_.assign(stutterBufferSize, 0.0f);
    stutterHistoryR_.assign(stutterBufferSize, 0.0f);
    stutterWrite_ = 0;
    stutterCaptureEnd_ = 0;
    stutterPhase_ = 0.0;

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
        if (i == 6) {
            // DRUMS: CC10 Kick Tune / CC11 Hi-hat Tune /
            // CC74 Snare Tune / CC1 Decay. Centre the three tunings.
            modelParts_[i].setControl(1, 64.0f / 127.0f);
            modelParts_[i].setControl(10, 64.0f / 127.0f);
            modelParts_[i].setControl(11, 64.0f / 127.0f);
            modelParts_[i].setControl(74, 64.0f / 127.0f);
        } else {
            modelParts_[i].setControl(1, cc1_ / 127.0f);
            modelParts_[i].setControl(10, cc10_ / 127.0f);
            modelParts_[i].setControl(11, cc11_ / 127.0f);
            modelParts_[i].setControl(74, cc74_ / 127.0f);
        }
    }

    dreamy_.prepare(sampleRate_);
    dreamy_.setMode(dreamyMode_);
    dreamy_.setXY(cc103_/127.f, cc104_/127.f);
    dreamy_.setExtraParameters(cc106_/127.f, cc107_/127.f);
    space_.prepare(sampleRate_);
    feltPianoReverb_.prepare(sampleRate_);
    feltPianoReverb_.setMode(SpaceEffect::HALL);
    feltPianoReverb_.setParameters(feltReverbMix_ / 100.0f, feltReverbDecay_ / 100.0f);
    tape_.prepare(sampleRate_);
    space_.setParameters(spaceMix_, spaceDecay_);
    tape_.setParameters(tapeWow_, tapeFlutter_, tapeDrive_);
    dreamy_.setParameters(cc103_/127.f, cc104_/127.f, dreamyMix_);
    recorder_.prepare(sampleRate_);

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
void AudioEngine::setDreamyMode(int mode){ push({Event::DREAMY_MODE,mode,0,0,0}); }
void AudioEngine::setDreamyExtraParameters(int p3,int p4){ push({Event::DREAMY_EXTRA,p3,p4,0,0}); }

void AudioEngine::recorderToggleRecording(){ recorder_.toggleRecording(); }
void AudioEngine::recorderToggleRandom(){ recorder_.toggleRandom(); }
void AudioEngine::recorderClear(){ recorder_.requestClear(); }
void AudioEngine::recorderPlaySlot(int slot){ recorder_.requestPlaySlot(slot); }
void AudioEngine::recorderRecordSlot(int slot){ recorder_.requestRecordSlot(slot); }
void AudioEngine::recorderToggleClock(){ recorder_.toggleMidiClockMode(); }
void AudioEngine::recorderSetBpm(int bpm){ recorder_.setInternalBpm(bpm); }
void AudioEngine::recorderMidiRealtime(int status){ recorder_.handleMidiRealtime(status); }
void AudioEngine::setAdsr(int attackMs,int decayMs,int sustainPct,int releaseMs){
    push({Event::ADSR,attackMs,decayMs,sustainPct,releaseMs});
}
void AudioEngine::setInstrument(int instrument){
    push({Event::INSTRUMENT,instrument,0,0,0});
}
void AudioEngine::setDrumParameter(int parameter,int value){
    push({Event::DRUM_PARAM,parameter,value,0,0});
}
void AudioEngine::setDrumFx(int boostDb,int distortion){
    push({Event::DRUM_FX,boostDb,distortion,0,0});
}
void AudioEngine::setPartFx(int part,int boostDb,int distortion){
    push({Event::PART_FX,part,boostDb,distortion,0});
}
void AudioEngine::setPartMixer(int part,int volume,int pan,bool muted){
    push({Event::PART_MIXER,part,volume,pan,muted?1:0});
}
void AudioEngine::setFeltReverb(int mix,int decay){
    push({Event::FELT_REVERB,mix,decay,0,0});
}
void AudioEngine::setPerformanceXY(bool active,int part,int x,int y){
    push({Event::PERFORMANCE_XY,active?1:0,part,x,y});
}

int AudioEngine::setAudioBufferBursts(float bursts) {
    if (!stream_) return AAUDIO_ERROR_INVALID_STATE;
    requestedBufferBursts_ = std::max(0.0f, bursts);

    int target = defaultBufferSizeFrames_;
    if (requestedBufferBursts_ > 0.0f) {
        const int fpb = AAudioStream_getFramesPerBurst(stream_);
        if (fpb <= 0) return AAUDIO_ERROR_INVALID_STATE;
        target = std::max(1, int(std::lround(float(fpb) * requestedBufferBursts_)));
    }
    if (target <= 0) return AAUDIO_ERROR_INVALID_STATE;
    return AAudioStream_setBufferSizeInFrames(stream_, target);
}

int AudioEngine::audioFramesPerBurst() const {
    return stream_ ? AAudioStream_getFramesPerBurst(stream_) : 0;
}
int AudioEngine::audioBufferSizeFrames() const {
    return stream_ ? AAudioStream_getBufferSizeInFrames(stream_) : 0;
}
int AudioEngine::audioBufferCapacityFrames() const {
    return stream_ ? AAudioStream_getBufferCapacityInFrames(stream_) : 0;
}
int AudioEngine::audioXRunCount() const {
    return stream_ ? AAudioStream_getXRunCount(stream_) : 0;
}


void AudioEngine::epBeginVoice(int note, int velocity) {
    if (!epBank_.loaded() || note < 21 || note > 108) return;

    EpVoice* pick = nullptr;
    for (auto& v : epVoices_) {
        if (!v.active) { pick = &v; break; }
    }
    if (!pick) {
        pick = &epVoices_[0];
        for (auto& v : epVoices_) {
            if (v.ageFrames > pick->ageFrames) pick = &v;
        }
    }

    const uint8_t rr = uint8_t((epRrCounter_[note]++ % 3) + 1);
    *pick = EpVoice{};
    pick->active = true;
    pick->keyDown = true;
    pick->note = note;
    pick->velocity = std::clamp(velocity, 1, 127);
    pick->rr = rr;
    pick->bodyVelocity = float(pick->velocity);
    pick->targetBodyVelocity = float(pick->velocity);

    const int at = epPolyAT_[note] ? epPolyAT_[note] : epChannelAT_;
    pick->targetBodyVelocity = float(pick->velocity)
            + (127.0f - pick->velocity) * (at / 127.0f);

    for (int i = 0; i < 8; ++i) {
        pick->sus[i] = epBank_.find(uint8_t(note), uint8_t(EP_VELS[i]), rr, SampleBank::SUSTAIN);
        pick->rel[i] = epBank_.find(uint8_t(note), uint8_t(EP_VELS[i]), rr, SampleBank::RELEASE);
    }
}

void AudioEngine::epReleaseVoice(int note) {
    for (auto& v : epVoices_) {
        if (!v.active || v.note != note || v.releasing || !v.keyDown) continue;
        v.keyDown = false;
        if (partSustain_[8] >= 64) {
            v.pendingRelease = true;
        } else {
            v.releasing = true;
            v.releaseFrame = 0.0;
        }
    }
}

void AudioEngine::epPolyPressure(int note, int pressure) {
    if (note < 0 || note >= 128) return;
    epPolyAT_[note] = std::clamp(pressure, 0, 127);
    for (auto& v : epVoices_) {
        if (!v.active || v.note != note) continue;
        v.targetBodyVelocity = float(v.velocity)
                + (127.0f - v.velocity) * (epPolyAT_[note] / 127.0f);
    }
}

void AudioEngine::epChannelPressure(int pressure) {
    epChannelAT_ = std::clamp(pressure, 0, 127);
    for (auto& v : epVoices_) {
        if (!v.active || epPolyAT_[v.note] != 0) continue;
        v.targetBodyVelocity = float(v.velocity)
                + (127.0f - v.velocity) * (epChannelAT_ / 127.0f);
    }
}

void AudioEngine::epSustainChanged(bool down) {
    if (down) return;
    for (auto& v : epVoices_) {
        if (v.active && v.pendingRelease && !v.keyDown) {
            v.pendingRelease = false;
            v.releasing = true;
            v.releaseFrame = 0.0;
        }
    }
}

void AudioEngine::epAllNotesOff() {
    for (auto& v : epVoices_) v = EpVoice{};
}

int AudioEngine::epActiveVoices() const {
    int count = 0;
    for (const auto& v : epVoices_) if (v.active) ++count;
    return count;
}

void AudioEngine::epBracket(float velocity, int& lo, int& hi, float& mix) {
    if (velocity <= EP_VELS[0]) { lo = hi = 0; mix = 0.0f; return; }
    if (velocity >= EP_VELS[7]) { lo = hi = 7; mix = 0.0f; return; }
    for (int i = 0; i < 7; ++i) {
        if (velocity >= EP_VELS[i] && velocity <= EP_VELS[i + 1]) {
            lo = i;
            hi = i + 1;
            mix = (velocity - EP_VELS[i]) / float(EP_VELS[i + 1] - EP_VELS[i]);
            return;
        }
    }
    lo = hi = 7;
    mix = 0.0f;
}

float AudioEngine::epLayerSample(
        const EpVoice& v, bool release, int layer, double frame, int channel) const {
    const auto* entry = release ? v.rel[layer] : v.sus[layer];
    return epBank_.read(entry, frame, channel);
}

void AudioEngine::epRenderVoice(EpVoice& v, float& l, float& r) {
    if (!v.active || !epBank_.loaded()) return;

    const int attackLock = int(0.250 * sampleRate_);
    if (v.ageFrames > attackLock) {
        v.bodyVelocity += (v.targetBodyVelocity - v.bodyVelocity) * 0.0025f;
    }
    const float requested = v.ageFrames <= attackLock ? float(v.velocity) : v.bodyVelocity;

    int lo = 0, hi = 0;
    float mix = 0.0f;
    epBracket(requested, lo, hi, mix);

    const double bendSemis = (double(partPitch_[8]) - 8192.0) / 8192.0 * 2.0;
    const double ratio = std::pow(2.0, bendSemis / 12.0)
            * (double(epBank_.sampleRate()) / double(sampleRate_));

    float sl = 0.0f, sr = 0.0f;
    if (!v.releasing) {
        sl = epLayerSample(v, false, lo, v.frame, 0) * (1.0f - mix)
           + epLayerSample(v, false, hi, v.frame, 0) * mix;
        sr = epLayerSample(v, false, lo, v.frame, 1) * (1.0f - mix)
           + epLayerSample(v, false, hi, v.frame, 1) * mix;
        v.frame += ratio;

        const auto* endRef = v.sus[hi] ? v.sus[hi] : v.sus[lo];
        if (!endRef) {
            v.active = false;
        } else if (v.frame >= endRef->frames) {
            if (v.pendingRelease && partSustain_[8] >= 64) {
                v.frame = std::max(0.0, double(endRef->frames) - 1.001);
            } else {
                v.active = false;
            }
        }
    } else {
        const int xf = std::max(1, int(0.020 * sampleRate_));
        const float x = std::clamp(float(v.releaseFrame) / float(xf), 0.0f, 1.0f);

        const float bodyL = epLayerSample(v, false, lo, v.frame, 0) * (1.0f - mix)
                          + epLayerSample(v, false, hi, v.frame, 0) * mix;
        const float bodyR = epLayerSample(v, false, lo, v.frame, 1) * (1.0f - mix)
                          + epLayerSample(v, false, hi, v.frame, 1) * mix;
        const float relL = epLayerSample(v, true, lo, v.releaseFrame, 0) * (1.0f - mix)
                         + epLayerSample(v, true, hi, v.releaseFrame, 0) * mix;
        const float relR = epLayerSample(v, true, lo, v.releaseFrame, 1) * (1.0f - mix)
                         + epLayerSample(v, true, hi, v.releaseFrame, 1) * mix;

        sl = bodyL * (1.0f - x) + relL * x;
        sr = bodyR * (1.0f - x) + relR * x;
        v.frame += ratio;
        v.releaseFrame += ratio;

        const auto* endRef = v.rel[hi] ? v.rel[hi] : v.rel[lo];
        if (!endRef || v.releaseFrame >= endRef->frames) v.active = false;
    }

    // The standalone EP master used 3.2x while the physical-model master uses 1.55x.
    // Compensate only the EP part so its established level is retained after the shared master.
    constexpr float EP_LEVEL_COMPENSATION = 3.2f / 1.55f;
    const float gain = (partVolume_[8] / 127.0f) * (epExpression_ / 127.0f)
            * EP_LEVEL_COMPENSATION;
    l += sl * gain;
    r += sr * gain;
    ++v.ageFrames;
}

void AudioEngine::handlePartNoteOn(int part, int note, int velocity) {
    part = std::clamp(part, 0, 8);
    if (velocity <= 0) { handlePartNoteOff(part, note); return; }
    if (part == 8) { epBeginVoice(note, velocity); return; }

    if (part == 7 && note >= 63 && note <= 68) {
        triggerDrumSample(note - 63, velocity);
        return;
    }

    if (part == 0) violin_.noteOn(note, velocity);
    else modelParts_[part - 1].noteOn(note, velocity);
}

void AudioEngine::handlePartNoteOff(int part, int note) {
    part = std::clamp(part, 0, 8);
    if (part == 8) { epReleaseVoice(note); return; }
    if (part == 7 && note >= 63 && note <= 68) {
        // Sample notes are one-shot; NoteOff does not truncate them.
        return;
    }
    const bool sustain = partSustain_[part] >= 64;
    if (part == 0) violin_.noteOff(note, sustain);
    else modelParts_[part - 1].noteOff(note, sustain);
}

void AudioEngine::handlePartPolyPressure(int part, int note, int pressure) {
    part = std::clamp(part, 0, 8);
    if (part == 8) epPolyPressure(note, pressure);
    else if (part == 0) violin_.polyPressure(note, pressure);
    else modelParts_[part - 1].polyPressure(note, pressure);
}

void AudioEngine::handlePartChannelPressure(int part, int pressure) {
    part = std::clamp(part, 0, 8);
    if (part == 8) epChannelPressure(pressure);
    else if (part == 0) violin_.channelPressure(pressure);
    else modelParts_[part - 1].channelPressure(pressure);
}

void AudioEngine::allNotesOffPart(int part) {
    part = std::clamp(part, 0, 8);
    if (part == 8) epAllNotesOff();
    else if (part == 0) violin_.allNotesOff();
    else modelParts_[part - 1].allNotesOff();
    if (part == 7) stopDrumSamples();
}

void AudioEngine::handlePartPitchBend(int part, int value14) {
    part = std::clamp(part, 0, 8);
    value14 = std::clamp(value14, 0, 16383);
    partPitch_[part] = value14;

    if (part == 0) violin_.pitchBend(value14);
    else if (part < 8) modelParts_[part - 1].pitchBend(value14);

    if (part == selectedInstrument_) pitch_ = value14;
}

void AudioEngine::handlePartControlChange(int part, int cc, int value) {
    part = std::clamp(part, 0, 8);
    value = std::clamp(value, 0, 127);

    // Per-part performance controls.
    if (cc == 7) {
        partVolume_[part] = value;
        if (part == selectedInstrument_) cc7_ = value;
    } else if (cc == 1) {
        if (part == 0) violin_.setVibratoDepth(value / 127.0f);
        else if (part < 8) modelParts_[part - 1].setControl(1, value / 127.0f);
        if (part == selectedInstrument_) cc1_ = value;
    } else if (cc == 10) {
        if (part == 0) violin_.setBowPressure(value / 127.0f);
        else if (part < 8) modelParts_[part - 1].setControl(10, value / 127.0f);
        if (part == selectedInstrument_) cc10_ = value;
    } else if (cc == 11) {
        if (part == 8) epExpression_ = value;
        else if (part == 0) violin_.setBowSpeed(value / 127.0f);
        else modelParts_[part - 1].setControl(11, value / 127.0f);
        if (part == selectedInstrument_) cc11_ = value;
    } else if (cc == 64) {
        partSustain_[part] = value;
        const bool down = value >= 64;
        if (part == 8) epSustainChanged(down);
        else if (part == 0) violin_.sustainChanged(down);
        else modelParts_[part - 1].sustainChanged(down);
        if (part == selectedInstrument_) cc64_ = value;
    } else if (cc == 74) {
        if (part == 0) violin_.setBowPosition(value / 127.0f);
        else if (part < 8) modelParts_[part - 1].setControl(74, value / 127.0f);
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
    } else if (cc == 106) {
        cc106_=value;
        dreamy_.setExtraParameters(cc106_/127.f, cc107_/127.f);
    } else if (cc == 107) {
        cc107_=value;
        dreamy_.setExtraParameters(cc106_/127.f, cc107_/127.f);
    } else if (cc == 108) {
        dreamyMode_=std::clamp(int(std::lround(value * (DreamyEffect::MODE_COUNT - 1) / 127.0)),0,DreamyEffect::MODE_COUNT-1);
        dreamy_.setMode(dreamyMode_);
    }
}

void AudioEngine::resetPerformanceDelay() {
    std::fill(performanceDelayL_.begin(), performanceDelayL_.end(), 0.0f);
    std::fill(performanceDelayR_.begin(), performanceDelayR_.end(), 0.0f);
    performanceDelayWrite_ = 0;
}

void AudioEngine::processPerformanceDelay(float& left, float& right) {
    if (performanceDelayL_.empty() || performanceDelayR_.empty()) return;

    const float smoothCoeff = 1.0f - std::exp(-1.0f / std::max(1.0f, 0.018f * sampleRate_));
    const float gateCoeff = 1.0f - std::exp(-1.0f / std::max(1.0f, 0.004f * sampleRate_));

    performanceXYSmoothX_ += (performanceXYX_ - performanceXYSmoothX_) * smoothCoeff;
    performanceXYSmoothY_ += (performanceXYY_ - performanceXYSmoothY_) * smoothCoeff;
    const float gateTarget = performanceXYActive_ ? 1.0f : 0.0f;
    performanceXYGate_ += (gateTarget - performanceXYGate_) * gateCoeff;

    const float delayMs = 25.0f + performanceXYSmoothX_ * 625.0f;
    const float targetSamples = std::clamp(
            delayMs * 0.001f * float(sampleRate_),
            1.0f,
            float(performanceDelayL_.size() - 3));

    if (performanceDelaySamples_ <= 0.0f) performanceDelaySamples_ = targetSamples;
    performanceDelaySamples_ += (targetSamples - performanceDelaySamples_) * smoothCoeff;

    float readPos = float(performanceDelayWrite_) - performanceDelaySamples_;
    while (readPos < 0.0f) readPos += float(performanceDelayL_.size());
    while (readPos >= float(performanceDelayL_.size())) readPos -= float(performanceDelayL_.size());

    const int i0 = int(std::floor(readPos));
    const int i1 = (i0 + 1) % int(performanceDelayL_.size());
    const float frac = readPos - std::floor(readPos);

    const float dl = performanceDelayL_[i0]
                   + (performanceDelayL_[i1] - performanceDelayL_[i0]) * frac;
    const float dr = performanceDelayR_[i0]
                   + (performanceDelayR_[i1] - performanceDelayR_[i0]) * frac;

    const float wet = performanceXYSmoothY_ * 0.88f * performanceXYGate_;
    const float feedback = performanceXYSmoothY_ * 0.74f * performanceXYGate_;

    const float dryL = left;
    const float dryR = right;

    performanceDelayL_[performanceDelayWrite_] =
            std::clamp(dryL + dl * feedback, -2.0f, 2.0f);
    performanceDelayR_[performanceDelayWrite_] =
            std::clamp(dryR + dr * feedback, -2.0f, 2.0f);

    performanceDelayWrite_++;
    if (performanceDelayWrite_ >= int(performanceDelayL_.size())) performanceDelayWrite_ = 0;

    left = dryL * (1.0f - wet) + dl * wet;
    right = dryR * (1.0f - wet) + dr * wet;
}

void AudioEngine::recordStutterHistory(float left, float right) {
    if (stutterHistoryL_.empty() || stutterHistoryR_.empty()) return;
    stutterHistoryL_[stutterWrite_] = left;
    stutterHistoryR_[stutterWrite_] = right;
    stutterWrite_++;
    if (stutterWrite_ >= int(stutterHistoryL_.size())) stutterWrite_ = 0;
}

void AudioEngine::processPerformanceStutter(float& left, float& right) {
    if (stutterHistoryL_.empty() || stutterHistoryR_.empty()) return;

    const float smoothCoeff = 1.0f - std::exp(-1.0f / std::max(1.0f, 0.018f * sampleRate_));
    const float gateCoeff = 1.0f - std::exp(-1.0f / std::max(1.0f, 0.004f * sampleRate_));

    performanceXYSmoothX_ += (performanceXYX_ - performanceXYSmoothX_) * smoothCoeff;
    performanceXYSmoothY_ += (performanceXYY_ - performanceXYSmoothY_) * smoothCoeff;
    const float gateTarget = performanceXYActive_ ? 1.0f : 0.0f;
    performanceXYGate_ += (gateTarget - performanceXYGate_) * gateCoeff;

    const float lengthMs = 18.0f + performanceXYSmoothX_ * 262.0f;
    const float targetLength = std::clamp(
            lengthMs * 0.001f * float(sampleRate_),
            16.0f,
            float(stutterHistoryL_.size() - 4));

    if (stutterLoopSamples_ <= 0.0f) stutterLoopSamples_ = targetLength;
    stutterLoopSamples_ += (targetLength - stutterLoopSamples_) * smoothCoeff;

    const int length = std::clamp(
            int(std::lround(stutterLoopSamples_)),
            16,
            int(stutterHistoryL_.size()) - 4);

    int start = stutterCaptureEnd_ - length;
    while (start < 0) start += int(stutterHistoryL_.size());

    const float offsetF = float(stutterPhase_) * float(length);
    const int offset0 = std::clamp(int(std::floor(offsetF)), 0, length - 1);
    const int offset1 = (offset0 + 1) % length;
    const float frac = offsetF - std::floor(offsetF);

    auto histL = [&](int off) {
        return stutterHistoryL_[(start + off) % int(stutterHistoryL_.size())];
    };
    auto histR = [&](int off) {
        return stutterHistoryR_[(start + off) % int(stutterHistoryR_.size())];
    };

    float loopL = histL(offset0) + (histL(offset1) - histL(offset0)) * frac;
    float loopR = histR(offset0) + (histR(offset1) - histR(offset0)) * frac;

    // Crossfade the wrap boundary over ~4 ms (or 1/4 of very short loops)
    // so unrelated waveform endpoints never meet as a hard discontinuity.
    const int xfade = std::max(4, std::min(length / 4, int(0.004f * sampleRate_)));
    if (xfade > 1 && offsetF >= float(length - xfade)) {
        const float t = std::clamp(
                (offsetF - float(length - xfade)) / float(xfade),
                0.0f, 1.0f);
        const float headOffsetF = offsetF - float(length - xfade);
        const int h0 = std::clamp(int(std::floor(headOffsetF)), 0, xfade - 1);
        const int h1 = std::min(h0 + 1, xfade - 1);
        const float hf = headOffsetF - std::floor(headOffsetF);
        const float headL = histL(h0) + (histL(h1) - histL(h0)) * hf;
        const float headR = histR(h0) + (histR(h1) - histR(h0)) * hf;
        const float shaped = t * t * (3.0f - 2.0f * t);
        loopL = loopL * (1.0f - shaped) + headL * shaped;
        loopR = loopR * (1.0f - shaped) + headR * shaped;
    }

    const float wet = performanceXYSmoothY_ * performanceXYGate_;
    left = left * (1.0f - wet) + loopL * wet;
    right = right * (1.0f - wet) + loopR * wet;

    stutterPhase_ += 1.0 / double(length);
    if (stutterPhase_ >= 1.0) stutterPhase_ -= std::floor(stutterPhase_);
}

float AudioEngine::processPart(int part) {
    part = std::clamp(part, 0, 8);
    if (part == 8) return 0.0f;
    const float raw = (part == 0) ? violin_.process() : modelParts_[part - 1].process();
    return raw * (partVolume_[part] / 127.0f);
}

int AudioEngine::activeVoicesPart(int part) const {
    part = std::clamp(part, 0, 8);
    if (part == 8) return epActiveVoices();
    if (part == 0) return violin_.activeVoices();
    int n = modelParts_[part - 1].activeVoices();
    if (part == 7) n += activeDrumSampleVoices();
    return n;
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
        case Event::DREAMY_MODE:
            dreamyMode_=std::clamp(e.a,0,DreamyEffect::MODE_COUNT-1);
            dreamy_.setMode(dreamyMode_);
            break;
        case Event::DREAMY_EXTRA:
            cc106_=std::clamp(int(std::lround(std::clamp(e.a,0,100)*1.27f)),0,127);
            cc107_=std::clamp(int(std::lround(std::clamp(e.b,0,100)*1.27f)),0,127);
            dreamy_.setExtraParameters(cc106_/127.f,cc107_/127.f);
            break;
        case Event::ADSR: {
            const float a = static_cast<float>(std::clamp(e.a,0,5000));
            const float d = static_cast<float>(std::clamp(e.b,0,5000));
            const float sus = std::clamp(e.c,0,100)/100.0f;
            const float r = static_cast<float>(std::clamp(e.d,0,5000));
            if (selectedInstrument_ == 0) violin_.setAdsr(a,d,sus,r);
            else if (selectedInstrument_ < 8) modelParts_[selectedInstrument_ - 1].setAdsr(a,d,sus,r);
            break;
        }
        case Event::INSTRUMENT:
            selectedInstrument_ = std::clamp(e.a, 0, 8);
            cc7_ = partVolume_[selectedInstrument_];
            cc64_ = partSustain_[selectedInstrument_];
            pitch_ = partPitch_[selectedInstrument_];
            break;
        case Event::DRUM_PARAM:
            modelParts_[6].setDrumParameter(
                    std::clamp(e.a, 0, 11),
                    std::clamp(e.b, 0, 127) / 127.0f);
            break;
        case Event::DRUM_FX:
            partBoostDb_[7] = std::clamp(e.a, 0, 18);
            partDistortion_[7] = std::clamp(e.b, 0, 127);
            break;
        case Event::PART_FX: {
            const int part = std::clamp(e.a, 0, 8);
            partBoostDb_[part] = std::clamp(e.b, 0, 18);
            partDistortion_[part] = std::clamp(e.c, 0, 127);
            break;
        }
        case Event::PART_MIXER: {
            const int part = std::clamp(e.a, 0, 8);
            partVolume_[part] = std::clamp(e.b, 0, 127);
            partPan_[part] = std::clamp(e.c, 0, 127);
            partMute_[part] = e.d != 0;
            if (part == selectedInstrument_) cc7_ = partVolume_[part];
            break;
        }
        case Event::FELT_REVERB:
            feltReverbMix_ = std::clamp(e.a, 0, 100);
            feltReverbDecay_ = std::clamp(e.b, 0, 100);
            feltPianoReverb_.setMode(feltReverbMix_ > 0 ? SpaceEffect::HALL : SpaceEffect::NONE);
            feltPianoReverb_.setParameters(feltReverbMix_ / 100.0f, feltReverbDecay_ / 100.0f);
            break;
        case Event::PERFORMANCE_XY: {
            const bool wasActive = performanceXYActive_;
            const int oldPart = performanceXYPart_;
            performanceXYActive_ = e.a != 0;
            performanceXYPart_ = std::clamp(e.b, 0, 8);
            performanceXYX_ = std::clamp(e.c, 0, 127) / 127.0f;
            performanceXYY_ = std::clamp(e.d, 0, 127) / 127.0f;

            if (performanceXYActive_ && (!wasActive || oldPart != performanceXYPart_)) {
                performanceXYSmoothX_ = performanceXYX_;
                performanceXYSmoothY_ = performanceXYY_;
                performanceXYGate_ = 0.0f;
                if (performanceXYPart_ == 7) {
                    stutterCaptureEnd_ = stutterWrite_;
                    stutterPhase_ = 0.0;
                    stutterLoopSamples_ = 0.0f;
                } else {
                    resetPerformanceDelay();
                    performanceDelaySamples_ = 0.0f;
                }
            }
            break;
        }
    }
}

void AudioEngine::render(float* out,int32_t frames) {
    std::lock_guard<std::mutex> epLock(epBankMutex_);
    Event e;
    while (pop(e)) handle(e);

    for (int32_t i=0; i<frames; ++i) {
        float mixL = 0.0f;
        float mixR = 0.0f;
        int activeParts = 0;

        for (int part=0; part<9; ++part) {
            const int voices = activeVoicesPart(part);
            float partL = 0.0f;
            float partR = 0.0f;

            if (part == 8) {
                for (auto& voice : epVoices_) epRenderVoice(voice, partL, partR);
            } else {
                partL = processPart(part);
                partR = partL;
                if (part == 7) {
                    // One-shot drum samples join the physical drum model before
                    // the DRUMS Part FX.
                    processDrumSamples(partL, partR);
                }
            }

            const float boost = std::pow(10.0f,
                    float(std::clamp(partBoostDb_[part], 0, 18)) / 20.0f);
            const float distAmount = std::clamp(partDistortion_[part], 0, 127) / 127.0f;
            const float drive = 1.0f + 14.0f * distAmount;
            auto partFx = [&](float x) {
                const float saturated = std::tanh(x * drive);
                return ((1.0f - distAmount) * x + distAmount * saturated) * boost;
            };
            partL = partFx(partL);
            partR = partFx(partR);

            // FELT PIANO only: dedicated local reverb before the momentary XY delay.
            if (part == 3) feltPianoReverb_.process(partL, partR);

            if (part == 7) {
                if (performanceXYPart_ == 7 &&
                        (performanceXYActive_ || performanceXYGate_ > 0.0005f)) {
                    processPerformanceStutter(partL, partR);
                } else {
                    // Keep one second of recent DRUMS audio ready so Stutter
                    // responds immediately when the user touches the XY area.
                    recordStutterHistory(partL, partR);
                }
            } else if (performanceXYPart_ == part &&
                    (performanceXYActive_ || performanceXYGate_ > 0.0005f)) {
                processPerformanceDelay(partL, partR);
            }

            // Mixer stage: balance-style PAN preserves the existing centre
            // level (L=R=1.0 at PAN C) and attenuates only the opposite side.
            // This also behaves naturally for EP-SAMPLE's stereo source.
            if (partMute_[part]) {
                partL = 0.0f;
                partR = 0.0f;
            } else {
                const int panValue = std::clamp(partPan_[part], 0, 127);
                const float pan = panValue < 64
                        ? float(panValue - 64) / 64.0f
                        : float(panValue - 64) / 63.0f;
                if (pan < 0.0f) partR *= (1.0f + pan);
                else if (pan > 0.0f) partL *= (1.0f - pan);
            }

            mixL += partL;
            mixR += partR;
            if (voices > 0 && !partMute_[part] && partVolume_[part] > 0) activeParts++;
        }

        const float norm = activeParts > 1 ? 1.0f / std::sqrt(float(activeParts)) : 1.0f;
        float l = mixL * norm;
        float r = mixR * norm;

        dreamy_.process(l,r);
        tape_.process(l,r);
        space_.process(l,r);

        const float boostGain = std::pow(10.0f, float(std::clamp(boostDb_,0,6)) / 20.0f);
        const float outGain = 1.55f * boostGain;

        const float baseL = std::isfinite(l) ? std::tanh(l * outGain) : 0.0f;
        const float baseR = std::isfinite(r) ? std::tanh(r * outGain) : 0.0f;

        // Capture the finished unified instrument mix after global FX/master,
        // but never feed recorder playback back into its own recording input.
        float mixedL = baseL;
        float mixedR = baseR;
        recorder_.process(baseL, baseR, mixedL, mixedR);
        out[i*2] = std::clamp(mixedL, -1.0f, 1.0f);
        out[i*2+1] = std::clamp(mixedR, -1.0f, 1.0f);
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
