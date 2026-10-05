#pragma once
#include <aaudio/AAudio.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include "SampleBank.h"
#include "PhysicalViolin.h"
#include "InstrumentModels.h"
#include "DreamyEffect.h"
#include "SpaceEffect.h"
#include "TapeEffect.h"
#include "IntegratedRecorder.h"

class AudioEngine {
public:
    static AudioEngine& instance();
    bool start();
    void stop();

    // EP-SAMPLE bank. Physical-model parts remain independent of this bank.
    bool loadBank(const std::string& path);
    bool loadBankFd(int fd);
    bool bankLoaded() const { return epBank_.loaded(); }
    std::string bankStatus() const { return epBank_.status(); }

    void noteOn(int note, int velocity);
    void noteOff(int note, int velocity);
    void polyPressure(int note, int pressure);
    void channelPressure(int pressure);
    void controlChange(int cc, int value);
    void pitchBend(int value14);
    void setDreamy(bool enabled);
    void setBoosterStep(int step);
    void setBoostDb(int db);
    void setSpaceMode(int mode);
    void setSpaceParameters(int mix, int decay);
    void setTape(bool enabled);
    void setTapeParameters(int wow, int flutter, int drive);
    void setDreamyParameters(int x, int y, int mix);
    void setAdsr(int attackMs, int decayMs, int sustainPct, int releaseMs);
    void setInstrument(int instrument);
    void setDrumParameter(int parameter, int value);
    void setDrumFx(int boostDb, int distortion);
    void setPartFx(int part, int boostDb, int distortion);
    void setPartMixer(int part, int volume, int pan, bool muted);
    void setFeltReverb(int mix, int decay);
    void setPerformanceXY(bool active, int part, int x, int y);
    int setAudioBufferBursts(float bursts);
    int audioFramesPerBurst() const;
    int audioBufferSizeFrames() const;
    int audioBufferCapacityFrames() const;
    int audioXRunCount() const;
    bool loadDrumSample(int slot, const uint8_t* data, size_t size);
    void noteOnPart(int part, int note, int velocity);
    void noteOffPart(int part, int note, int velocity);
    void polyPressurePart(int part, int note, int pressure);
    void channelPressurePart(int part, int pressure);
    void controlChangePart(int part, int cc, int value);
    void pitchBendPart(int part, int value14);

    void recorderToggleRecording();
    void recorderToggleRandom();
    void recorderClear();
    void recorderPlaySlot(int slot);
    void recorderRecordSlot(int slot);
    void recorderToggleClock();
    void recorderSetBpm(int bpm);
    void recorderMidiRealtime(int status);
    bool recorderRecording() const { return recorder_.isRecording(); }
    bool recorderRandom() const { return recorder_.isRandom(); }
    bool recorderMidiClock() const { return recorder_.isMidiClockMode(); }
    int recorderBpm() const { return recorder_.internalBpm(); }
    int recorderRecordingSlot() const { return recorder_.recordingSlot(); }
    bool recorderSlotPlaying(int slot) const { return recorder_.isSlotPlaying(slot); }
    float recorderSlotProgress(int slot) const { return recorder_.slotProgress(slot); }
    int recorderValidSamples(int slot) const { return recorder_.validSamples(slot); }
    float recorderPeak(int slot, int bin) const { return recorder_.peak(slot, bin); }
    float recorderInputLevel() const { return recorder_.inputLevel(); }

private:
    AudioEngine() = default;
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    struct Event {
        enum Type : uint8_t {
            NOTE_ON, NOTE_OFF, POLY_AT, CH_AT, CC, PITCH,
            DREAMY, BOOST, BOOST_DB, SPACE_MODE, SPACE_PARAMS,
            TAPE, TAPE_PARAMS, DREAMY_PARAMS, ADSR, INSTRUMENT,
            PART_NOTE_ON, PART_NOTE_OFF, PART_POLY_AT, PART_CH_AT, PART_CC, PART_PITCH,
            DRUM_PARAM, DRUM_FX, PART_FX, PART_MIXER, FELT_REVERB, PERFORMANCE_XY
        } type;
        int a=0,b=0,c=0,d=0;
    };

    static constexpr uint32_t QUEUE = 1024;
    std::array<Event, QUEUE> queue_{};
    std::atomic<uint32_t> write_{0}, read_{0};
    void push(Event e);
    bool pop(Event& e);

    PhysicalViolin violin_;
    std::array<InstrumentModels, 7> modelParts_{};
    int selectedInstrument_=0;
    std::array<int, 9> partVolume_{{112,112,112,112,112,112,112,112,127}};
    std::array<int, 9> partPan_{{64,64,64,64,64,64,64,64,64}};
    std::array<bool, 9> partMute_{{false,false,false,false,false,false,false,false,false}};
    std::array<int, 9> partSustain_{{0,0,0,0,0,0,0,0,0}};
    std::array<int, 9> partPitch_{{8192,8192,8192,8192,8192,8192,8192,8192,8192}};
    DreamyEffect dreamy_;
    SpaceEffect space_;
    SpaceEffect feltPianoReverb_;
    TapeEffect tape_;
    IntegratedRecorder recorder_;

    struct DrumSample {
        std::vector<float> left;
        std::vector<float> right;
        int sampleRate = 48000;
        bool loaded = false;
    };
    struct DrumSampleVoice {
        bool active = false;
        int slot = -1;
        double position = 0.0;
        float gain = 1.0f;
    };
    static constexpr int DRUM_SAMPLE_COUNT = 6;
    static constexpr int DRUM_SAMPLE_VOICES = 12;
    std::array<DrumSample, DRUM_SAMPLE_COUNT> drumSamples_{};
    std::array<DrumSampleVoice, DRUM_SAMPLE_VOICES> drumSampleVoices_{};
    int drumSampleSteal_ = 0;

    int cc1_=14;
    int cc7_=112;
    int cc10_=74;
    int cc11_=74;
    int cc74_=42;
    int cc64_=0;
    int cc103_=36;
    int cc104_=36;
    int pitch_=8192;
    int boosterStep_=0;
    int boostDb_=0;
    float spaceMix_=0.50f, spaceDecay_=0.50f;
    float tapeWow_=0.50f, tapeFlutter_=0.50f, tapeDrive_=0.50f;
    float dreamyMix_=0.34f;
    std::array<int,9> partBoostDb_{{0,0,0,0,0,0,0,6,0}};
    std::array<int,9> partDistortion_{{0,0,0,0,0,0,0,0,0}};
    int feltReverbMix_=28;
    int feltReverbDecay_=58;

    // Momentary touchscreen performance FX.
    bool performanceXYActive_=false;
    int performanceXYPart_=0;
    float performanceXYX_=0.5f;
    float performanceXYY_=0.5f;
    float performanceXYSmoothX_=0.5f;
    float performanceXYSmoothY_=0.5f;
    float performanceXYGate_=0.0f;
    float performanceDelaySamples_=0.0f;
    float stutterLoopSamples_=0.0f;

    std::vector<float> performanceDelayL_;
    std::vector<float> performanceDelayR_;
    int performanceDelayWrite_=0;

    std::vector<float> stutterHistoryL_;
    std::vector<float> stutterHistoryR_;
    int stutterWrite_=0;
    int stutterCaptureEnd_=0;
    double stutterPhase_=0.0;

    // EP-SAMPLE is the ninth instrument (part 8). It deliberately keeps the
    // proven EPBANK1 playback path separate from the eight physical-model parts.
    struct EpVoice {
        bool active=false;
        bool releasing=false;
        bool pendingRelease=false;
        bool keyDown=false;
        int note=0;
        int velocity=0;
        int rr=1;
        double frame=0.0;
        double releaseFrame=0.0;
        float bodyVelocity=0.f;
        float targetBodyVelocity=0.f;
        int ageFrames=0;
        std::array<const SampleBank::Entry*,8> sus{};
        std::array<const SampleBank::Entry*,8> rel{};
    };
    static constexpr std::array<int,8> EP_VELS{{16,32,48,64,80,96,112,127}};
    static constexpr int EP_MAX_VOICES = 64;
    std::array<EpVoice,EP_MAX_VOICES> epVoices_{};
    std::array<uint8_t,128> epRrCounter_{};
    std::array<int,128> epPolyAT_{};
    int epChannelAT_=0;
    int epExpression_=127;
    SampleBank epBank_;
    std::mutex epBankMutex_;

    AAudioStream* stream_=nullptr;
    int sampleRate_=48000;
    int defaultBufferSizeFrames_=0;
    float requestedBufferBursts_=0.0f;

    void epBeginVoice(int note, int velocity);
    void epReleaseVoice(int note);
    void epPolyPressure(int note, int pressure);
    void epChannelPressure(int pressure);
    void epSustainChanged(bool down);
    void epAllNotesOff();
    int epActiveVoices() const;
    void epRenderVoice(EpVoice& v, float& l, float& r);
    float epLayerSample(const EpVoice& v, bool release, int layer, double frame, int channel) const;
    static void epBracket(float velocity, int& lo, int& hi, float& mix);

    void handle(const Event& e);
    void handlePartNoteOn(int part, int note, int velocity);
    void handlePartNoteOff(int part, int note);
    void handlePartPolyPressure(int part, int note, int pressure);
    void handlePartChannelPressure(int part, int pressure);
    void handlePartControlChange(int part, int cc, int value);
    void handlePartPitchBend(int part, int value14);
    void allNotesOffPart(int part);
    void triggerDrumSample(int slot, int velocity);
    void stopDrumSamples();
    void processDrumSamples(float& left, float& right);
    int activeDrumSampleVoices() const;
    void resetPerformanceDelay();
    void processPerformanceDelay(float& left, float& right);
    void recordStutterHistory(float left, float right);
    void processPerformanceStutter(float& left, float& right);
    float processPart(int part);
    int activeVoicesPart(int part) const;
    void render(float* out, int32_t frames);
    static aaudio_data_callback_result_t dataCallback(AAudioStream*, void*, void*, int32_t);
    static void errorCallback(AAudioStream*, void*, aaudio_result_t);
};
