#pragma once
#include <array>
#include <vector>

namespace chordfx
{
class GranularPitchBank
{
public:
    static constexpr int maxVoices = 5;
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setTargetRatios (const std::vector<float>& ratios);
    float processSample (float input);

private:
    struct Grain { double pos = 0.0; int age = 0; bool active = false; };
    struct Voice { float ratio = 1.0f; std::array<Grain,2> grains {}; int launchCounter = 0; bool enabled = false; };
    float readLinear (double pos) const;
    void launch (Voice& voice, int grainIndex);

    double sampleRate = 48000.0;
    int grainSize = 512;
    int hopSize = 256;
    int writePos = 0;
    std::vector<float> ring;
    std::array<Voice, maxVoices> voices {};
};
}