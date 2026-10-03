#pragma once
#include <vector>
#include <array>
#include <cstddef>

class SpaceEffect {
public:
    enum Mode { NONE=0, ROOM=1, HALL=2, SPACE=3 };
    void prepare(int sampleRate);
    void setMode(int mode);
    void setParameters(float mix, float decay);
    int mode() const { return mode_; }
    void process(float& left, float& right);

private:
    int sampleRate_ = 48000;
    int mode_ = NONE;
    std::vector<float> histL_, histR_;
    size_t write_ = 0;
    float dampL_ = 0.f, dampR_ = 0.f;
    float mix_ = 0.50f, decay_ = 0.50f;

    float readDelay(const std::vector<float>& b, float seconds) const;
};
