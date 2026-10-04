#pragma once
#include <aaudio/AAudio.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include "PhysicalViolin.h"
#include "InstrumentModels.h"
#include "DreamyEffect.h"
#include "SpaceEffect.h"
#include "TapeEffect.h"

class AudioEngine {
public:
    static AudioEngine& instance();
    bool start();
    void stop();

    // Kept for JNI/source compatibility with EP Sampler. The physical-model
    // branch does not require or read a sample bank.
    bool loadBank(const std::string&) { return true; }
    bool loadBankFd(int) { return true; }
    bool bankLoaded() const { return true; }
    std::string bankStatus() const { return "MODEL READY"; }

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
    void noteOnPart(int part, int note, int velocity);
    void noteOffPart(int part, int note, int velocity);
    void polyPressurePart(int part, int note, int pressure);
    void channelPressurePart(int part, int pressure);
    void controlChangePart(int part, int cc, int value);
    void pitchBendPart(int part, int value14);

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
            DRUM_PARAM
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
    std::array<int, 8> partVolume_{{112,112,112,112,112,112,112,112}};
    std::array<int, 8> partSustain_{{0,0,0,0,0,0,0,0}};
    std::array<int, 8> partPitch_{{8192,8192,8192,8192,8192,8192,8192,8192}};
    DreamyEffect dreamy_;
    SpaceEffect space_;
    TapeEffect tape_;

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

    AAudioStream* stream_=nullptr;
    int sampleRate_=48000;

    void handle(const Event& e);
    void handlePartNoteOn(int part, int note, int velocity);
    void handlePartNoteOff(int part, int note);
    void handlePartPolyPressure(int part, int note, int pressure);
    void handlePartChannelPressure(int part, int pressure);
    void handlePartControlChange(int part, int cc, int value);
    void handlePartPitchBend(int part, int value14);
    void allNotesOffPart(int part);
    float processPart(int part);
    int activeVoicesPart(int part) const;
    void render(float* out, int32_t frames);
    static aaudio_data_callback_result_t dataCallback(AAudioStream*, void*, void*, int32_t);
    static void errorCallback(AAudioStream*, void*, aaudio_result_t);
};
