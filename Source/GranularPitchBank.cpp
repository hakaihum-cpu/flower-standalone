#include "GranularPitchBank.h"
#include <algorithm>
#include <cmath>

namespace chordfx
{
void GranularPitchBank::prepare (double sr, int)
{
    sampleRate = sr > 1000.0 ? sr : 48000.0;
    // 512 samples at 48 kHz (~10.7 ms) was too short for voice/guitar and
    // produced a strong metallic/robotic texture. Use a longer 50%-overlapped
    // grain while keeping the same lightweight two-grain structure.
    grainSize = sampleRate >= 88200.0 ? 2048 : 1024;
    hopSize = grainSize / 2;
    ring.assign ((size_t) grainSize * 8u, 0.0f);
    reset();
}

void GranularPitchBank::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = 0;
    for (auto& v : voices) v = {};
}

void GranularPitchBank::setTargetRatios (const std::vector<float>& ratios)
{
    for (int i = 0; i < maxVoices; ++i)
    {
        auto& v = voices[(size_t) i];
        v.enabled = i < (int) ratios.size();
        if (v.enabled) v.ratio = std::clamp (ratios[(size_t) i], 0.5f, 2.5f);
    }
}

float GranularPitchBank::readLinear (double p) const
{
    const int n = (int) ring.size();
    while (p < 0.0) p += n;
    while (p >= n) p -= n;
    const int i0 = (int) p;
    const int i1 = (i0 + 1) % n;
    const float f = (float) (p - i0);
    return ring[(size_t) i0] + f * (ring[(size_t) i1] - ring[(size_t) i0]);
}

void GranularPitchBank::launch (Voice& voice, int grainIndex)
{
    auto& g = voice.grains[(size_t) grainIndex];
    g.pos = (double) writePos - (double) grainSize;
    if (g.pos < 0.0) g.pos += ring.size();
    g.age = 0;
    g.active = true;
}

float GranularPitchBank::processSample (float input)
{
    if (ring.empty()) return input;
    ring[(size_t) writePos] = input;
    writePos = (writePos + 1) % (int) ring.size();

    float sum = 0.0f;
    int activeVoices = 0;
    constexpr float pi = 3.14159265358979323846f;

    for (auto& v : voices)
    {
        if (! v.enabled) continue;
        ++activeVoices;
        if (v.launchCounter == 0) launch (v, 0);
        if (v.launchCounter == hopSize) launch (v, 1);
        v.launchCounter = (v.launchCounter + 1) % grainSize;

        float voiceOut = 0.0f;
        for (auto& g : v.grains)
        {
            if (! g.active) continue;
            const float phase = (float) g.age / (float) grainSize;
            if (phase >= 1.0f) { g.active = false; continue; }
            const float w = 0.5f - 0.5f * std::cos (2.0f * pi * phase);
            voiceOut += readLinear (g.pos) * w;
            g.pos += v.ratio;
            if (g.pos >= (double) ring.size()) g.pos -= ring.size();
            ++g.age;
        }
        sum += voiceOut;
    }
    // Average active voices rather than applying 1/sqrt(N) gain. The previous
    // law could raise peak level as voices were added and clip the Android output.
    return activeVoices > 0 ? sum / (float) activeVoices : 0.0f;
}
}