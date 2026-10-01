#include "YinPitchDetector.h"
#include <algorithm>
#include <cmath>

namespace chordfx
{
void YinPitchDetector::prepare (double sr, int ws, int hs)
{
    sampleRate = sr > 1000.0 ? sr : 48000.0;
    windowSize = std::max (512, ws);
    hopSize = std::max (64, hs);
    ring.assign ((size_t) windowSize, 0.0f);
    work.assign ((size_t) windowSize, 0.0f);
    diff.assign ((size_t) (windowSize / 2 + 1), 0.0f);
    reset();
}

void YinPitchDetector::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = samplesSinceAnalysis = filled = 0;
}

PitchEstimate YinPitchDetector::pushSample (float x)
{
    if (ring.empty()) prepare (sampleRate, windowSize, hopSize);
    ring[(size_t) writePos] = x;
    writePos = (writePos + 1) % windowSize;
    filled = std::min (windowSize, filled + 1);
    if (++samplesSinceAnalysis < hopSize || filled < windowSize) return {};
    samplesSinceAnalysis = 0;
    return analyse();
}

PitchEstimate YinPitchDetector::analyse()
{
    for (int i = 0; i < windowSize; ++i)
        work[(size_t) i] = ring[(size_t) ((writePos + i) % windowSize)];

    double energy = 0.0;
    for (float x : work) energy += x * x;
    const float rms = std::sqrt ((float) (energy / std::max (1, windowSize)));
    if (rms < 0.0025f) return {};

    const int minTau = std::max (2, (int) std::floor (sampleRate / 1100.0));
    const int maxTau = std::min (windowSize / 2, (int) std::ceil (sampleRate / 75.0));
    if (maxTau <= minTau + 2) return {};

    std::fill (diff.begin(), diff.end(), 0.0f);
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double d = 0.0;
        const int n = windowSize - tau;
        for (int i = 0; i < n; ++i)
        {
            const float delta = work[(size_t) i] - work[(size_t) (i + tau)];
            d += delta * delta;
        }
        diff[(size_t) tau] = (float) d;
    }

    float running = 0.0f;
    diff[0] = 1.0f;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        running += diff[(size_t) tau];
        diff[(size_t) tau] = running > 0.0f ? diff[(size_t) tau] * tau / running : 1.0f;
    }

    constexpr float threshold = 0.16f;
    int tau = -1;
    for (int t = minTau; t < maxTau; ++t)
    {
        if (diff[(size_t) t] < threshold)
        {
            while (t + 1 <= maxTau && diff[(size_t) (t + 1)] < diff[(size_t) t]) ++t;
            tau = t;
            break;
        }
    }
    if (tau < 0)
    {
        tau = minTau;
        for (int t = minTau + 1; t <= maxTau; ++t)
            if (diff[(size_t) t] < diff[(size_t) tau]) tau = t;
        if (diff[(size_t) tau] > 0.30f) return {};
    }

    float refined = (float) tau;
    if (tau > 1 && tau < maxTau)
    {
        const float a = diff[(size_t) (tau - 1)], b = diff[(size_t) tau], c = diff[(size_t) (tau + 1)];
        const float denom = 2.0f * (2.0f * b - c - a);
        if (std::abs (denom) > 1.0e-9f) refined += (c - a) / denom;
    }

    PitchEstimate out;
    out.hz = (float) (sampleRate / refined);
    out.confidence = std::clamp (1.0f - diff[(size_t) tau], 0.0f, 1.0f);
    out.valid = out.hz >= 75.0f && out.hz <= 1100.0f && out.confidence >= 0.70f;
    return out;
}
}