#pragma once
#include <aaudio/AAudio.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include "SampleBank.h"
#include "DreamyEffect.h"

class AudioEngine {
public:
    static AudioEngine& instance();
    bool start();
    void stop();
    bool loadBank(const std::string& path);
    bool loadBankFd(int fd);
    bool bankLoaded() const { return bank_.loaded(); }
    std::string bankStatus() const { return bank_.status(); }

    void noteOn(int note, int velocity);
    void noteOff(int note, int velocity);
    void polyPressure(int note, int pressure);
    void channelPressure(int pressure);
    void controlChange(int cc, int value);
    void pitchBend(int value14);
    void setDreamy(bool enabled);

private:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    struct Event {
        enum Type : uint8_t { NOTE_ON, NOTE_OFF, POLY_AT, CH_AT, CC, PITCH, DREAMY } type;
        int a=0,b=0;
    };
    static constexpr uint32_t QUEUE = 1024;
    std::array<Event, QUEUE> queue_{};
    std::atomic<uint32_t> write_{0}, read_{0};
    void push(Event e);
    bool pop(Event& e);

    struct Voice {
        bool active=false;
        bool releasing=false;
        bool pendingRelease=false;
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

    static constexpr std::array<int,8> VELS{{16,32,48,64,80,96,112,127}};
    static constexpr int MAX_VOICES = 64;
    std::array<Voice,MAX_VOICES> voices_{};
    std::array<uint8_t,128> rrCounter_{};
    std::array<int,128> polyAT_{};
    int channelAT_=0, cc7_=127, cc11_=127, cc64_=0, cc103_=36, cc104_=36, pitch_=8192;

    SampleBank bank_;
    DreamyEffect dreamy_;
    AAudioStream* stream_=nullptr;
    int sampleRate_=48000;
    std::mutex bankMutex_;

    void handle(const Event& e);
    void beginVoice(int note, int velocity);
    void releaseVoice(int note);
    void render(float* out, int32_t frames);
    void renderVoice(Voice& v, float& l, float& r);
    const SampleBank::Entry* sampleFor(const Voice& v, bool release, int layer) const;
    float layerSample(const Voice& v, bool release, int layer, double frame, int channel) const;
    static void bracket(float v, int& lo, int& hi, float& mix);
    static aaudio_data_callback_result_t dataCallback(AAudioStream*, void*, void*, int32_t);
    static void errorCallback(AAudioStream*, void*, aaudio_result_t);
};
