#include "GranularPitchBank.h"
#include <algorithm>
#include <cmath>

namespace chordfx
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;
}

void GranularPitchBank::prepare (double sr, int)
{
    sampleRate = sr > 1000.0 ? sr : 48000.0;

    // Periodic Hann. With 4x overlap (hop = N/4), the sum of Hann^2 is 1.5,
    // so synthesis applies a 2/3 COLA gain.
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (twoPi * (float) i / (float) fftSize);

    reset();
}

void GranularPitchBank::reset()
{
    sampleCounter = 0;
    inputWritePos = 0;
    analysisPrimed = false;
    inputHistory.fill (0.0f);
    outputRing.fill (0.0f);
    spectrum.fill (std::complex<float> { 0.0f, 0.0f });
    previousAnalysisPhase.fill (0.0f);
    analysisMagnitude.fill (0.0f);
    analysisTrueBin.fill (0.0f);

    for (auto& v : voices)
        v = {};
}

void GranularPitchBank::setTargetRatios (const std::vector<float>& ratios)
{
    for (int i = 0; i < maxVoices; ++i)
    {
        auto& v = voices[(size_t) i];
        const bool nextEnabled = i < (int) ratios.size();

        if (! nextEnabled)
        {
            v.enabled = false;
            continue;
        }

        const float nextRatio = std::clamp (ratios[(size_t) i], 0.5f, 2.5f);
        if (! v.enabled)
            v.sumPhase.fill (0.0f);

        v.enabled = true;
        v.ratio = nextRatio;
    }
}

float GranularPitchBank::wrapPhase (float phase) noexcept
{
    while (phase > pi) phase -= twoPi;
    while (phase < -pi) phase += twoPi;
    return phase;
}

void GranularPitchBank::fft (std::array<std::complex<float>, fftSize>& data, bool inverse)
{
    // Iterative radix-2 FFT. Kept local to this engine so the existing
    // dependency-free C++ core preflight remains unchanged.
    for (int i = 1, j = 0; i < fftSize; ++i)
    {
        int bit = fftSize >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (data[(size_t) i], data[(size_t) j]);
    }

    for (int len = 2; len <= fftSize; len <<= 1)
    {
        const float angle = (inverse ? 2.0f : -2.0f) * pi / (float) len;
        const std::complex<float> wlen (std::cos (angle), std::sin (angle));

        for (int i = 0; i < fftSize; i += len)
        {
            std::complex<float> w (1.0f, 0.0f);
            const int half = len >> 1;
            for (int j = 0; j < half; ++j)
            {
                const auto u = data[(size_t) (i + j)];
                const auto v = data[(size_t) (i + j + half)] * w;
                data[(size_t) (i + j)] = u + v;
                data[(size_t) (i + j + half)] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse)
    {
        const float scale = 1.0f / (float) fftSize;
        for (auto& x : data)
            x *= scale;
    }
}

void GranularPitchBank::processFrame()
{
    // inputWritePos always points to the oldest sample after fftSize samples
    // have been collected.
    for (int i = 0; i < fftSize; ++i)
    {
        const int index = (inputWritePos + i) % fftSize;
        spectrum[(size_t) i] = { inputHistory[(size_t) index] * window[(size_t) i], 0.0f };
    }

    fft (spectrum, false);

    constexpr float expectedPhaseAdvance = twoPi * (float) hopSize / (float) fftSize;

    for (int k = 0; k < numBins; ++k)
    {
        const auto bin = spectrum[(size_t) k];
        const float magnitude = std::abs (bin);
        const float phase = std::atan2 (bin.imag(), bin.real());

        if (! analysisPrimed)
        {
            previousAnalysisPhase[(size_t) k] = phase;
            analysisMagnitude[(size_t) k] = magnitude;
            analysisTrueBin[(size_t) k] = (float) k;
            continue;
        }

        float delta = phase
                    - previousAnalysisPhase[(size_t) k]
                    - expectedPhaseAdvance * (float) k;
        previousAnalysisPhase[(size_t) k] = phase;
        delta = wrapPhase (delta);

        analysisMagnitude[(size_t) k] = magnitude;
        analysisTrueBin[(size_t) k] = (float) k + delta / expectedPhaseAdvance;
    }

    if (! analysisPrimed)
    {
        analysisPrimed = true;
        return;
    }

    int activeVoices = 0;
    for (const auto& v : voices)
        if (v.enabled) ++activeVoices;

    if (activeVoices == 0)
        return;

    constexpr float colaScale = 2.0f / 3.0f;

    for (auto& voice : voices)
    {
        if (! voice.enabled)
            continue;

        voice.synthMagnitude.fill (0.0f);
        voice.synthWeightedBin.fill (0.0f);

        for (int k = 0; k < numBins; ++k)
        {
            const int target = (int) ((float) k * voice.ratio);
            if (target < 0 || target >= numBins)
                continue;

            const float magnitude = analysisMagnitude[(size_t) k];
            const float shiftedTrueBin = analysisTrueBin[(size_t) k] * voice.ratio;

            voice.synthMagnitude[(size_t) target] += magnitude;
            voice.synthWeightedBin[(size_t) target] += magnitude * shiftedTrueBin;
        }

        spectrum.fill (std::complex<float> { 0.0f, 0.0f });

        for (int k = 0; k < numBins; ++k)
        {
            const float magnitude = voice.synthMagnitude[(size_t) k];
            if (magnitude <= 1.0e-12f)
                continue;

            const float trueBin = voice.synthWeightedBin[(size_t) k] / magnitude;
            voice.sumPhase[(size_t) k] += trueBin * expectedPhaseAdvance;

            const float phase = voice.sumPhase[(size_t) k];
            std::complex<float> value (magnitude * std::cos (phase),
                                       magnitude * std::sin (phase));

            if (k == 0 || k == fftSize / 2)
                value = { value.real(), 0.0f };

            spectrum[(size_t) k] = value;
            if (k > 0 && k < fftSize / 2)
                spectrum[(size_t) (fftSize - k)] = std::conj (value);
        }

        fft (spectrum, true);

        const float voiceScale = colaScale / (float) activeVoices;
        for (int i = 0; i < fftSize; ++i)
        {
            const auto absoluteIndex = sampleCounter + (std::uint64_t) i;
            const int ringIndex = (int) (absoluteIndex % (std::uint64_t) outputRingSize);
            outputRing[(size_t) ringIndex] += spectrum[(size_t) i].real()
                                            * window[(size_t) i]
                                            * voiceScale;
        }
    }
}

float GranularPitchBank::processSample (float input)
{
    const int outputIndex = (int) (sampleCounter % (std::uint64_t) outputRingSize);
    const float output = outputRing[(size_t) outputIndex];
    outputRing[(size_t) outputIndex] = 0.0f;

    inputHistory[(size_t) inputWritePos] = input;
    inputWritePos = (inputWritePos + 1) % fftSize;
    ++sampleCounter;

    if (sampleCounter >= (std::uint64_t) fftSize
        && sampleCounter % (std::uint64_t) hopSize == 0)
        processFrame();

    return output;
}
}
