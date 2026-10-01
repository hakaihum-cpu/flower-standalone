#pragma once
#include <vector>

namespace chordfx
{
struct PitchEstimate
{
    float hz = 0.0f;
    float confidence = 0.0f;
    bool valid = false;
};

class YinPitchDetector
{
public:
    void prepare (double sampleRate, int windowSize = 1024, int hopSize = 256);
    void reset();
    PitchEstimate pushSample (float monoSample);

private:
    PitchEstimate analyse();
    double sampleRate = 48000.0;
    int windowSize = 1024;
    int hopSize = 256;
    int writePos = 0;
    int samplesSinceAnalysis = 0;
    int filled = 0;
    std::vector<float> ring;
    std::vector<float> work;
    std::vector<float> diff;
};
}