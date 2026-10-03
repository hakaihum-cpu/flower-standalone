#pragma once
#include <vector>
#include <array>
#include <cstddef>
#include <cstdint>

// FLOWER/MIYAKO-style Dreamy core:
// two pitch-up micro-loop voices (+5 / +12 semitones) reading a ~2.5 s history.
// X controls drift, Y controls micro-loop length and wet amount.
class DreamyEffect {
public:
    void prepare(int sampleRate);
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }
    void setXY(float x, float y);
    void process(float& left, float& right);

private:
    struct Grain {
        double read = 0.0;
        double phase = 0.0;
        bool initialized = false;
    };
    struct Voice {
        std::array<Grain,2> grains{};
    };

    bool enabled_ = true;
    int sampleRate_ = 48000;
    std::vector<float> histL_, histR_;
    size_t write_ = 0;
    uint64_t historyFrames_ = 0;
    std::array<Voice,2> voices_{};

    float targetX_ = 0.28f, targetY_ = 0.28f;
    float x_ = 0.28f, y_ = 0.28f;
    float wetLpL_ = 0.f, wetLpR_ = 0.f;

    double wrap(double p) const;
    float readInterp(const std::vector<float>& b, double p) const;
    float grainSample(Grain& g, const std::vector<float>& b, double speed,
                      double loopFrames, double resetLagFrames, int voiceIndex, int grainIndex);
};
