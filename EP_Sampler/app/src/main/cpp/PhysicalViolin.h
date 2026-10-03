#pragma once

#include <array>
#include <cstdint>

class PhysicalViolin {
public:
    void prepare(double sampleRate);
    void reset();

    void noteOn(int note, int velocity);
    void noteOff(int note, bool sustainDown);
    void sustainChanged(bool down);
    void allNotesOff();

    void polyPressure(int note, int value);
    void channelPressure(int value);
    void pitchBend(int value14);

    void setBowPressure(float normalized);
    void setBowSpeed(float normalized);
    void setBowPosition(float normalized);
    void setVibratoDepth(float normalized);
    void setAdsr(float attackMs, float decayMs, float sustain, float releaseMs);

    float process();
    int activeVoices() const;

private:
    static constexpr int kStrings = 4;
    static constexpr int kDelaySize = 2048;
    static constexpr int kBodyModes = 14;

    struct DelayLine {
        std::array<float, kDelaySize> data{};
        int writeIndex = 0;
    };

    struct StringState {
        int openNote = 55;
        int note = -1;
        int velocity = 0;
        int pressure = 0;
        bool active = false;
        bool keyDown = false;
        bool pendingRelease = false;

        float bowEnvelope = 0.0f;
        float frictionState = 0.0f;
        float bridgeFilter = 0.0f;
        float lastBridge = 0.0f;
        float energyFollower = 0.0f;
        float ampEnv = 0.0f;
        uint8_t ampStage = 0; // 0 off, 1 attack, 2 decay, 3 sustain, 4 release

        double fundamental = 196.0;
        double targetFundamental = 196.0;
        double vibratoPhase = 0.0;
        uint64_t age = 0;

        DelayLine bridgeDelay{};
        DelayLine neckDelay{};
    };

    struct BodyMode {
        float y1 = 0.0f;
        float y2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float gain = 0.0f;
    };

    double sampleRate_ = 48000.0;
    float bowPressure_ = 0.56f;
    float bowSpeed_ = 0.58f;
    float bowPosition_ = 0.12f;
    float vibratoDepth_ = 0.10f;
    float attackMs_ = 20.0f;
    float decayMs_ = 120.0f;
    float sustain_ = 0.90f;
    float releaseMs_ = 300.0f;
    int channelPressure_ = 0;
    int pitchBend_ = 8192;
    bool sustainDown_ = false;

    std::array<StringState, kStrings> strings_{};
    std::array<BodyMode, kBodyModes> body_{};

    static double midiToHz(double note);
    static float readDelay(const DelayLine& delay, float delaySamples);
    static void writeDelay(DelayLine& delay, float sample);
    int chooseString(int note) const;
    float processAmpEnvelope(StringState& s);
    float processString(StringState& s);
    float processBody(float bridgeInput);
};
