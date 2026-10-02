#pragma once
#include <array>
#include <complex>
#include <cstdint>
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
    static constexpr int fftSize = 1024;
    static constexpr int hopSize = 256;
    static constexpr int numBins = fftSize / 2 + 1;
    static constexpr int outputRingSize = fftSize * 4;

    struct Voice
    {
        float ratio = 1.0f;
        bool enabled = false;
        std::array<float, numBins> sumPhase {};
        std::array<float, numBins> synthMagnitude {};
        std::array<float, numBins> synthWeightedBin {};
    };

    static void fft (std::array<std::complex<float>, fftSize>& data, bool inverse);
    static float wrapPhase (float phase) noexcept;
    void processFrame();

    double sampleRate = 48000.0;
    std::uint64_t sampleCounter = 0;
    int inputWritePos = 0;
    bool analysisPrimed = false;

    std::array<float, fftSize> window {};
    std::array<float, fftSize> inputHistory {};
    std::array<float, outputRingSize> outputRing {};
    std::array<std::complex<float>, fftSize> spectrum {};
    std::array<float, numBins> previousAnalysisPhase {};
    std::array<float, numBins> analysisMagnitude {};
    std::array<float, numBins> analysisTrueBin {};
    std::array<Voice, maxVoices> voices {};
};
}
