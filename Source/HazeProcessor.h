#pragma once
#include <JuceHeader.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace chordfx
{
class HazeProcessor
{
public:
    struct Params
    {
        float mix = 0.55f;
        float time = 0.50f;
        float haze = 0.50f;
        float filter = 0.50f;
        float repeat = 0.0f;
        float mod = 0.50f;
        int speed = 1;      // 0=.5x, 1=1x, 2=2x
        int loops = 1;      // 0=1, 1=2, 2=2+
        int warble = 0;     // 0=none, 1=light, 2=heavy
        bool transpose = false;
        bool echo = false;
        bool og = false;
        bool lock = false;
        bool bypass = false;
        bool highGain = false;
        bool stereoPath = true;
    };

    void prepare (double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 48000.0;
        capacity = std::max (4096, (int) std::ceil (sampleRate * 10.2));
        for (auto& lane : lanes)
        {
            lane.buffer.assign ((size_t) capacity, 0.0f);
            lane.recordPosition = 0;
            lane.playPosition = 0.0;
            lane.previousPlayPosition = 0.0;
            lane.jumpCrossfade = 0;
            lane.jumpCrossfadeTotal = 1;
            lane.randomGain = 1.0f;
            lane.bitDepth = 16;
            lane.reverse = false;
            lane.noiseAmount = 0.0f;
            lane.lastCycle = -1;
        }

        const int warbleCapacity = std::max (256, (int) std::ceil (sampleRate * 0.020));
        warbleBuffer.setSize (2, warbleCapacity, false, true, false);
        warbleBuffer.clear();
        warbleWrite = 0;
        warblePhase = 0.0f;
        warbleTarget = 0.0f;
        warbleSmoothed = 0.0f;
        modTarget = 0.0f;
        modSmoothed = 0.0f;
        modSamplesRemaining = 0;
        filterLow = { 0.0f, 0.0f };
        filterBand = { 0.0f, 0.0f };
        currentSpeed = 1.0;
        randomState = 0x48415A45u;
    }

    void clear() noexcept
    {
        for (auto& lane : lanes)
        {
            std::fill (lane.buffer.begin(), lane.buffer.end(), 0.0f);
            lane.recordPosition = 0;
            lane.playPosition = 0.0;
            lane.previousPlayPosition = 0.0;
            lane.jumpCrossfade = 0;
            lane.lastCycle = -1;
        }
        if (warbleBuffer.getNumSamples() > 0)
            warbleBuffer.clear();
        filterLow = { 0.0f, 0.0f };
        filterBand = { 0.0f, 0.0f };
    }

    void setParams (const Params& next) noexcept
    {
        params = next;
        params.mix = clamp01 (params.mix);
        params.time = clamp01 (params.time);
        params.haze = clamp01 (params.haze);
        params.filter = clamp01 (params.filter);
        params.repeat = clamp01 (params.repeat);
        params.mod = clamp01 (params.mod);
        params.speed = std::clamp (params.speed, 0, 2);
        params.loops = std::clamp (params.loops, 0, 2);
        params.warble = std::clamp (params.warble, 0, 2);
    }

    // Hardware Haze keeps recording even while its output is bypassed.
    // EFFECTS mirrors that behaviour while HAZE is not the visible mode too:
    // this method is called on the raw input before other audible processors.
    void captureBlock (const juce::AudioBuffer<float>& input) noexcept
    {
        const int channels = input.getNumChannels();
        const int samples = input.getNumSamples();
        if (capacity <= 0 || channels <= 0 || samples <= 0 || params.lock)
            return;

        const float gain = params.highGain ? 3.9810717f : 1.0f;

        for (int i = 0; i < samples; ++i)
        {
            const float left = finite (input.getSample (0, i) * gain);
            const float right = finite (
                (channels > 1 ? input.getSample (1, i) : input.getSample (0, i)) * gain);

            writeLane (0, left);
            writeLane (1, params.stereoPath ? right : left);
        }
    }

    void renderBlock (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int channels = buffer.getNumChannels();
        const int samples = buffer.getNumSamples();
        if (capacity <= 0 || channels <= 0 || samples <= 0)
            return;

        for (int i = 0; i < samples; ++i)
        {
            updateModulation();

            const float dryL = finite (buffer.getSample (0, i));
            const float dryR = finite (
                channels > 1 ? buffer.getSample (1, i) : dryL);

            float wetL = 0.0f;
            float wetR = 0.0f;
            renderWet (wetL, wetR);

            wetL = saturate (wetL);
            wetR = saturate (wetR);
            wetL = filterWet (0, wetL);
            wetR = filterWet (1, wetR);

            float outL = dryL + (wetL - dryL) * params.mix;
            float outR = dryR + (wetR - dryR) * params.mix;

            applyWarble (outL, outR);

            if (params.bypass)
            {
                outL = dryL;
                outR = dryR;
            }

            buffer.setSample (0, i, finite (outL));
            if (channels > 1)
                buffer.setSample (1, i, finite (outR));
        }
    }

private:
    struct Lane
    {
        std::vector<float> buffer;
        int recordPosition = 0;
        double playPosition = 0.0;
        double previousPlayPosition = 0.0;
        int jumpCrossfade = 0;
        int jumpCrossfadeTotal = 1;
        float randomGain = 1.0f;
        int bitDepth = 16;
        bool reverse = false;
        float noiseAmount = 0.0f;
        int lastCycle = -1;
    };

    static float clamp01 (float v) noexcept
    {
        return std::max (0.0f, std::min (1.0f, v));
    }

    static float finite (float v) noexcept
    {
        return std::isfinite (v) ? v : 0.0f;
    }

    uint32_t nextRandom() noexcept
    {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState;
    }

    float randomUnit() noexcept
    {
        return (float) (nextRandom() & 0x00ffffffu) / 16777215.0f;
    }

    float randomBipolar() noexcept
    {
        return randomUnit() * 2.0f - 1.0f;
    }

    double baseLoopSeconds (int lane) const noexcept
    {
        // Manual: both loops are 5 s at noon; turning TIME lengthens one
        // while shortening the other; each can reach 10 s.
        const float timeModDepth =
            params.mod < 0.5f ? (0.5f - params.mod) * 2.0f : 0.0f;
        const float effectiveTime = clamp01 (
            params.time + modSmoothed * 0.16f * timeModDepth);
        const double seconds =
            lane == 0 ? 10.0 * effectiveTime
                      : 10.0 * (1.0 - effectiveTime);
        return std::max (0.025, seconds);
    }

    int loopSamples (int lane) const noexcept
    {
        return std::clamp (
            (int) std::lround (baseLoopSeconds (lane) * sampleRate),
            64, capacity - 2);
    }

    float effectiveRepeat() const noexcept
    {
        const float depth =
            params.mod < 0.5f ? (0.5f - params.mod) * 2.0f : 0.0f;
        return clamp01 (params.repeat + modSmoothed * 0.22f * depth);
    }

    int activeSpan (int lane) const noexcept
    {
        const int loop = loopSamples (lane);
        const float repeat = effectiveRepeat();
        const float minFactor = params.og ? 0.0015f : 0.006f;
        const float factor = std::pow (
            minFactor, std::pow (repeat, params.og ? 1.05f : 1.25f));
        return std::clamp ((int) std::lround (loop * factor), 24, loop);
    }

    float effectiveHaze() const noexcept
    {
        const float depth =
            params.mod > 0.5f ? (params.mod - 0.5f) * 2.0f : 0.0f;
        return clamp01 (params.haze + modSmoothed * 0.22f * depth);
    }

    void reconfigureLane (int index) noexcept
    {
        auto& lane = lanes[(size_t) index];
        const float haze = effectiveHaze();
        const float amount = std::abs (haze - 0.5f) * 2.0f;
        const bool leftSide = haze < 0.5f;

        lane.randomGain = 1.0f - amount * (0.05f + 0.12f * randomUnit());
        lane.bitDepth = std::clamp (
            16 - (int) std::lround (amount * (params.og ? 9.0f : 6.0f)),
            params.og ? 5 : 8, 16);
        lane.noiseAmount =
            amount * (params.og ? 0.020f : 0.006f)
            * (0.35f + 0.65f * randomUnit());

        const float reverseChance =
            effectiveRepeat() * amount * (params.og ? 0.48f : 0.24f);
        lane.reverse = randomUnit() < reverseChance;

        // Counter-clockwise HAZE introduces random jumps into old/silent
        // regions of the record buffer. Clean mode sutures the jump.
        if (leftSide && amount > 0.02f
            && randomUnit() < amount * (params.og ? 0.85f : 0.58f))
        {
            lane.previousPlayPosition = lane.playPosition;
            lane.playPosition = randomUnit() * (double) loopSamples (index);
            lane.jumpCrossfadeTotal = std::max (
                1, (int) std::lround (
                    sampleRate * (params.og ? 0.0005 : 0.006)));
            lane.jumpCrossfade = lane.jumpCrossfadeTotal;
        }
    }

    void writeLane (int index, float input) noexcept
    {
        auto& lane = lanes[(size_t) index];
        const int loop = loopSamples (index);
        if (lane.recordPosition >= loop)
            lane.recordPosition = 0;

        const float old = lane.buffer[(size_t) lane.recordPosition];
        lane.buffer[(size_t) lane.recordPosition] =
            params.echo ? 0.5f * old + 0.5f * input : input;

        ++lane.recordPosition;
        if (lane.recordPosition >= loop)
        {
            lane.recordPosition = 0;
            reconfigureLane (index);
        }
    }

    float readLinear (const Lane& lane, double position, int loop) const noexcept
    {
        while (position < 0.0)
            position += (double) loop;
        while (position >= (double) loop)
            position -= (double) loop;
        const int i0 = (int) position;
        const int i1 = (i0 + 1) % loop;
        const float frac = (float) (position - (double) i0);
        return lane.buffer[(size_t) i0]
             + frac * (lane.buffer[(size_t) i1] - lane.buffer[(size_t) i0]);
    }

    float readLane (int index, double speed) noexcept
    {
        auto& lane = lanes[(size_t) index];
        const int loop = loopSamples (index);
        const int span = activeSpan (index);
        const int start = (lane.recordPosition - span + loop) % loop;

        double local = lane.playPosition;
        while (local < 0.0)
            local += (double) span;
        while (local >= (double) span)
            local -= (double) span;

        const int seam = std::clamp (
            params.og ? 12
                      : (int) std::lround (sampleRate * 0.004),
            8, std::max (8, span / 4));

        auto readSpan = [&] (double localPosition) noexcept
        {
            while (localPosition < 0.0)
                localPosition += (double) span;
            while (localPosition >= (double) span)
                localPosition -= (double) span;

            const double absolute =
                (double) start + localPosition;
            return readLinear (lane, absolute, loop);
        };

        float value = readSpan (local);
        if (! params.og && local >= (double) (span - seam))
        {
            const float t =
                (float) ((local - (double) (span - seam)) / (double) seam);
            const double headPosition =
                local - (double) (span - seam);
            value += (readSpan (headPosition) - value) * t;
        }

        if (lane.jumpCrossfade > 0)
        {
            const float t = 1.0f
                - (float) lane.jumpCrossfade
                  / (float) std::max (1, lane.jumpCrossfadeTotal);
            const float previous =
                readSpan (lane.previousPlayPosition);
            value = previous + (value - previous) * t;
            lane.previousPlayPosition += speed;
            --lane.jumpCrossfade;
        }

        const float levels = (float) (1u << std::clamp (lane.bitDepth, 4, 20));
        value = std::round (value * levels) / levels;
        value = value * lane.randomGain
              + randomBipolar() * lane.noiseAmount;

        const double direction = lane.reverse ? -1.0 : 1.0;
        lane.playPosition += speed * direction;

        while (lane.playPosition < 0.0)
            lane.playPosition += (double) span;
        while (lane.playPosition >= (double) span)
            lane.playPosition -= (double) span;

        return finite (value);
    }

    double targetSpeed() const noexcept
    {
        static constexpr double speeds[] { 0.5, 1.0, 2.0 };
        double speed = speeds[params.speed];
        if (params.transpose)
            speed *= 0.75;
        return speed;
    }

    void renderWet (float& left, float& right) noexcept
    {
        const double target = targetSpeed();
        // OG intentionally glides between tape speeds; clean mode settles
        // quickly while remaining click-safe.
        const double timeSeconds = params.og ? 0.080 : 0.008;
        const double coeff =
            1.0 - std::exp (-1.0 / std::max (1.0, sampleRate * timeSeconds));
        currentSpeed += coeff * (target - currentSpeed);

        const float a = readLane (0, currentSpeed);

        if (params.loops == 0)
        {
            left = right = a;
            return;
        }

        const double laneBMultiplier = params.loops == 2 ? 2.0 : 1.0;
        const float b = readLane (1, currentSpeed * laneBMultiplier);

        if (params.stereoPath)
        {
            left = a;
            right = b;
        }
        else
        {
            left = right = 0.5f * (a + b);
        }
    }

    float saturate (float x) const noexcept
    {
        const float drive = params.og ? 1.65f : 1.30f;
        return std::tanh (x * drive) / std::tanh (drive);
    }

    float filterWet (int channel, float input) noexcept
    {
        const float position = params.filter;
        const float distance = std::abs (position - 0.5f) * 2.0f;
        if (distance < 0.015f)
            return input;

        const float cutoff =
            18000.0f * std::pow (0.035f, distance);
        const float f = std::clamp (
            2.0f * std::sin (
                juce::MathConstants<float>::pi
                * cutoff / (float) sampleRate),
            0.001f, 0.95f);
        const float q =
            params.og ? (0.34f - 0.22f * distance)
                      : (0.62f - 0.22f * distance);

        float& low = filterLow[(size_t) channel];
        float& band = filterBand[(size_t) channel];
        low += f * band;
        const float high = input - low - q * band;
        band += f * high;

        const float filtered =
            position < 0.5f ? low : band;
        return input + (filtered - input) * distance;
    }

    void updateModulation() noexcept
    {
        if (--modSamplesRemaining <= 0)
        {
            modTarget = randomBipolar();
            modSamplesRemaining = std::max (
                64, (int) std::lround (
                    sampleRate * (0.22 + randomUnit() * 1.4)));
        }

        const float coeff =
            1.0f - std::exp (
                -1.0f / std::max (1.0f, (float) sampleRate * 0.18f));
        modSmoothed += coeff * (modTarget - modSmoothed);
    }

    void applyWarble (float& left, float& right) noexcept
    {
        if (warbleBuffer.getNumSamples() <= 8)
            return;

        const int size = warbleBuffer.getNumSamples();
        warbleBuffer.setSample (0, warbleWrite, left);
        warbleBuffer.setSample (1, warbleWrite, right);

        if (params.warble == 0)
        {
            warbleWrite = (warbleWrite + 1) % size;
            return;
        }

        warblePhase += 1.0f / std::max (
            1.0f, (float) sampleRate * 0.37f);
        if (warblePhase >= 1.0f)
        {
            warblePhase -= 1.0f;
            warbleTarget = randomBipolar();
        }

        const float smooth =
            1.0f - std::exp (
                -1.0f / std::max (1.0f, (float) sampleRate * 0.12f));
        warbleSmoothed += smooth * (warbleTarget - warbleSmoothed);

        const float depthMs =
            params.warble == 1 ? 0.45f : 1.7f;
        const float baseMs = params.warble == 1 ? 2.0f : 3.2f;
        const double delaySamples =
            sampleRate * (baseMs + depthMs * warbleSmoothed) / 1000.0;

        auto read = [&] (int channel)
        {
            double pos = (double) warbleWrite - delaySamples;
            while (pos < 0.0) pos += (double) size;
            while (pos >= (double) size) pos -= (double) size;
            const int i0 = (int) pos;
            const int i1 = (i0 + 1) % size;
            const float frac = (float) (pos - (double) i0);
            const float a = warbleBuffer.getSample (channel, i0);
            const float b = warbleBuffer.getSample (channel, i1);
            return a + (b - a) * frac;
        };

        left = read (0);
        right = read (1);
        warbleWrite = (warbleWrite + 1) % size;
    }

    double sampleRate = 48000.0;
    int capacity = 0;
    Params params;
    std::array<Lane, 2> lanes;
    double currentSpeed = 1.0;
    uint32_t randomState = 0x48415A45u;

    float modTarget = 0.0f;
    float modSmoothed = 0.0f;
    int modSamplesRemaining = 0;

    std::array<float, 2> filterLow { 0.0f, 0.0f };
    std::array<float, 2> filterBand { 0.0f, 0.0f };

    juce::AudioBuffer<float> warbleBuffer;
    int warbleWrite = 0;
    float warblePhase = 0.0f;
    float warbleTarget = 0.0f;
    float warbleSmoothed = 0.0f;
};
}
