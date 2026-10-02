#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// Real-time TD-PSOLA/PAWS-style resynthesis adapted for EFFECTS from the
// algorithmic structure described by Keith Lent (1989) and the MIT-licensed
// DspTap basic_psola implementation (Timothy Place / DspTap contributors).
// See THIRD_PARTY_NOTICES.md. All storage is allocated in prepare(); process()
// is allocation-free and is intended for harmonic-rich monophonic input.
namespace chordfx
{
class PsolaVoice
{
public:
    void prepare (std::size_t maxPeriodSamples)
    {
        maxPeriod = std::max<std::size_t> (16, maxPeriodSamples);
        latency = 2 * maxPeriod + 2;
        const std::size_t ring = 4 * maxPeriod + 8;
        input.assign (ring, 0.0f);
        accum.assign (ring, 0.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill (input.begin(), input.end(), 0.0f);
        std::fill (accum.begin(), accum.end(), 0.0f);
        sampleClock = 0;
        nextAnalysisMark = 0.0;
        previousAnalysisMark = 0.0;
        haveMark = false;
        nextSynthesisMark = static_cast<double> (latency);
    }

    std::size_t latencySamples() const noexcept { return latency; }

    float process (float in, float periodSamples, float pitchRatio) noexcept
    {
        if (input.empty() || accum.empty())
            return 0.0f;

        const double period = std::clamp ((double) periodSamples, 8.0, (double) maxPeriod);
        const double ratio = std::clamp ((double) pitchRatio, 0.25, 4.0);
        const auto inSize = (std::int64_t) input.size();

        input[(std::size_t) (sampleClock % inSize)] = in;
        const double now = (double) sampleClock;

        while (nextAnalysisMark <= now)
        {
            previousAnalysisMark = nextAnalysisMark;
            haveMark = true;
            nextAnalysisMark += period;
        }

        while (nextSynthesisMark <= now + period)
        {
            if (haveMark)
            {
                const double sourceMark = previousAnalysisMark - period;
                if (sourceMark - period >= now - (double) input.size() + 4.0
                    && sourceMark + period <= now)
                {
                    placeGrain (nextSynthesisMark, sourceMark, period,
                                (float) (1.0 / ratio));
                }
            }
            nextSynthesisMark += period / ratio;
        }

        const double emit = now - (double) latency;
        if (nextSynthesisMark < emit)
            nextSynthesisMark = emit;

        float out = 0.0f;
        if (sampleClock >= (std::int64_t) latency)
        {
            const auto index = (std::size_t)
                ((sampleClock - (std::int64_t) latency) % (std::int64_t) accum.size());
            out = accum[index];
            accum[index] = 0.0f;
        }

        ++sampleClock;
        return out;
    }

private:
    float readHermite (double pos) const noexcept
    {
        const double baseD = std::floor (pos);
        const float frac = (float) (pos - baseD);
        const std::int64_t base = (std::int64_t) baseD;
        const std::int64_t n = (std::int64_t) input.size();

        auto at = [this, n] (std::int64_t i) noexcept
        {
            const auto wrapped = (i % n + n) % n;
            return input[(std::size_t) wrapped];
        };

        const float xm1 = at (base - 1);
        const float x0 = at (base);
        const float x1 = at (base + 1);
        const float x2 = at (base + 2);
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((a * frac - b) * frac + c) * frac + x0;
    }

    void placeGrain (double synthesisMark, double sourceMark,
                     double period, float gain) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        const std::int64_t first = (std::int64_t) std::ceil (synthesisMark - period);
        const std::int64_t last = (std::int64_t) std::floor (synthesisMark + period);
        const double invPeriod = 1.0 / period;
        const std::int64_t n = (std::int64_t) accum.size();

        for (std::int64_t o = first; o <= last; ++o)
        {
            const double delta = (double) o - synthesisMark;
            const float window = (float) (0.5 + 0.5 * std::cos (pi * delta * invPeriod));
            const auto wrapped = (o % n + n) % n;
            accum[(std::size_t) wrapped] += gain * window
                                           * readHermite (sourceMark + delta);
        }
    }

    std::size_t maxPeriod = 800;
    std::size_t latency = 1602;
    std::vector<float> input;
    std::vector<float> accum;
    std::int64_t sampleClock = 0;
    double nextAnalysisMark = 0.0;
    double previousAnalysisMark = 0.0;
    bool haveMark = false;
    double nextSynthesisMark = 0.0;
};

class PsolaHarmonyBank
{
public:
    static constexpr int maxVoices = 4;

    void prepare (double sampleRate)
    {
        const auto maxPeriod = (std::size_t) std::ceil (
            (sampleRate > 1000.0 ? sampleRate : 48000.0) / 60.0);
        for (auto& voice : voices)
            voice.prepare (maxPeriod);
        reset();
    }

    void reset() noexcept
    {
        for (auto& voice : voices)
            voice.reset();
        voiceCount = 0;
        targets.fill (1.0f);
        current.fill (1.0f);
    }

    void setRatios (const std::array<float, maxVoices>& ratios, int count) noexcept
    {
        voiceCount = std::clamp (count, 0, maxVoices);
        for (int i = 0; i < maxVoices; ++i)
        {
            targets[(std::size_t) i] = i < voiceCount
                ? std::clamp (ratios[(std::size_t) i], 0.5f, 2.0f)
                : 1.0f;
            if (i >= voiceCount)
                current[(std::size_t) i] = 1.0f;
        }
    }

    int activeVoices() const noexcept { return voiceCount; }

    float processSample (float inputSample, float periodSamples) noexcept
    {
        if (voiceCount <= 0)
            return 0.0f;

        float sum = 0.0f;
        for (int i = 0; i < voiceCount; ++i)
        {
            auto& r = current[(std::size_t) i];
            r += 0.0025f * (targets[(std::size_t) i] - r);
            sum += voices[(std::size_t) i].process (inputSample, periodSamples, r);
        }
        return sum / (float) voiceCount;
    }

private:
    std::array<PsolaVoice, maxVoices> voices;
    std::array<float, maxVoices> targets { 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<float, maxVoices> current { 1.0f, 1.0f, 1.0f, 1.0f };
    int voiceCount = 0;
};
}
