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

    float process();
    int activeVoices() const;

private:
    static constexpr int kStrings = 4;
    static constexpr int kMaxModes = 24;
    static constexpr int kBodyModes = 14;

    struct StringMode {
        float y1 = 0.0f;
        float y2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float excite = 0.0f;
        float phiBow = 0.0f;
        float bridgeWeight = 0.0f;
        bool enabled = false;
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
        float energyFollower = 0.0f;
        double fundamental = 196.0;
        double targetFundamental = 196.0;
        double vibratoPhase = 0.0;
        int coeffCountdown = 0;
        uint64_t age = 0;
        std::array<StringMode, kMaxModes> modes{};
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
    int channelPressure_ = 0;
    int pitchBend_ = 8192;
    bool sustainDown_ = false;

    std::array<StringState, kStrings> strings_{};
    std::array<BodyMode, kBodyModes> body_{};

    static double midiToHz(double note);
    int chooseString(int note) const;
    void updateStringCoefficients(StringState& s);
    float processString(StringState& s);
    float processBody(float bridgeInput);
};
