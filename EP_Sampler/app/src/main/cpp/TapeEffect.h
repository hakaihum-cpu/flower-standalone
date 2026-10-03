#pragma once
#include <vector>
#include <cstddef>

class TapeEffect {
public:
    void prepare(int sampleRate);
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }
    void setParameters(float wow, float flutter, float drive);
    void process(float& left, float& right);

private:
    bool enabled_ = false;
    int sampleRate_ = 48000;
    std::vector<float> histL_, histR_;
    size_t write_ = 0;
    double wowPhase_ = 0.0;
    double flutterPhase_ = 0.0;
    float lpL_ = 0.f, lpR_ = 0.f;
    float wow_ = 0.50f, flutter_ = 0.50f, drive_ = 0.50f;

    float readInterp(const std::vector<float>& b, double p) const;
};
