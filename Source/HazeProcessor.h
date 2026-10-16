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
        float reverb = 0.0f;
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
        const auto pole = [this] (double seconds)
        {
            return (float) (1.0 - std::exp (
                -1.0 / std::max (1.0, sampleRate * seconds)));
        };
        controlCoeff = pole (0.055);
        reverbCoeff = pole (0.080);
        modulationCoeff = pole (0.18);
        speedCoeffClean = pole (0.008);
        speedCoeffOg = pole (0.080);
        warbleCoeff = pole (0.12);
        spanCoeff = pole (0.050);
        capacity = std::max (4096, (int) std::ceil (sampleRate * 10.2));
        for (auto& lane : lanes)
        {
            lane.buffer.assign ((size_t) capacity, 0.0f);
            lane.writeGeneration.assign ((size_t) capacity, 0u);
            lane.generation = 1u;
            lane.recordPosition = 0;
            lane.validSamples = 0;
            lane.smoothedSpan = 0.0f;
            lane.cachedSpan = 0;
            lane.spanFramesUntilRefresh = 0;
            lane.stutterStartValid = false;
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
        smoothedTime = 0.50f;
        smoothedRepeat = 0.0f;
        smoothedReverb = 0.0f;
        randomState = 0x48415A45u;
        hallReverb.setSampleRate (sampleRate);
        hallReverb.reset();
    }

    void clear() noexcept
    {
        for (auto& lane : lanes)
        {
            // Invalidate the old recording without clearing ~10 seconds
            // of stereo buffers on Android's real-time audio callback.
            ++lane.generation;
            if (lane.generation == 0u) lane.generation = 1u;
            lane.recordPosition = 0;
            lane.validSamples = 0;
            lane.smoothedSpan = 0.0f;
            lane.cachedSpan = 0;
            lane.spanFramesUntilRefresh = 0;
            lane.stutterStartValid = false;
            lane.playPosition = 0.0;
            lane.previousPlayPosition = 0.0;
            lane.jumpCrossfade = 0;
            lane.lastCycle = -1;
        }
        if (warbleBuffer.getNumSamples() > 0)
            warbleBuffer.clear();
        filterLow = { 0.0f, 0.0f };
        filterBand = { 0.0f, 0.0f };
        smoothedTime = params.time;
        smoothedRepeat = params.repeat;
        smoothedReverb = params.reverb;
        hallReverb.reset();
    }

    void forceShuffle() noexcept
    {
        reconfigureLane (0);
        reconfigureLane (1);
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
        params.reverb = clamp01 (params.reverb);
        params.speed = std::clamp (params.speed, 0, 2);
        params.loops = std::clamp (params.loops, 0, 2);
        params.warble = std::clamp (params.warble, 0, 2);
        filterDistance = std::abs (params.filter - 0.5f) * 2.0f;
        if (filterDistance >= 0.015f)
        {
            const float cutoff =
                18000.0f * std::pow (0.035f, filterDistance);
            filterCoeff = std::clamp (
                2.0f * std::sin (
                    juce::MathConstants<float>::pi
                    * cutoff / (float) sampleRate),
                0.001f, 0.95f);
            filterQ = params.og ? (0.34f - 0.22f * filterDistance)
                                : (0.62f - 0.22f * filterDistance);
        }
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

            // No old buffer exists immediately after CLEAR. Crossfade
            // gradually from live audio as fresh history accumulates.
            const int available = params.loops == 0
                ? lanes[0].validSamples
                : std::min (lanes[0].validSamples, lanes[1].validSamples);
            const float historyReady = std::clamp (
                (float) available / std::max (
                    64.0f, (float) sampleRate * 0.030f), 0.0f, 1.0f);
            const float activeMix = params.mix * historyReady;
            float outL = dryL + (wetL - dryL) * activeMix;
            float outR = dryR + (wetR - dryR) * activeMix;

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

        juce::Reverb::Parameters hall;
        hall.roomSize = 0.88f;
        hall.damping = 0.42f;
        hall.wetLevel = params.bypass ? 0.0f : 0.52f * smoothedReverb;
        hall.dryLevel = 1.0f;
        hall.width = 1.0f;
        hall.freezeMode = 0.0f;
        hallReverb.setParameters (hall);

        if (channels >= 2)
            hallReverb.processStereo (
                buffer.getWritePointer (0),
                buffer.getWritePointer (1), samples);
        else
            hallReverb.processMono (
                buffer.getWritePointer (0), samples);

        if (! params.bypass)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* out = buffer.getWritePointer (ch);
                for (int i = 0; i < samples; ++i)
                    out[i] = softProtect (out[i]);
            }
        }
    }

private:
    struct Lane
    {
        std::vector<float> buffer;
        std::vector<uint32_t> writeGeneration;
        uint32_t generation = 1u;
        int recordPosition = 0;
        int validSamples = 0;
        float smoothedSpan = 0.0f;
        int cachedSpan = 0;
        int spanFramesUntilRefresh = 0;
        bool stutterStartValid = false;
        int stutterStart = 0;
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

    static float softProtect (float sample) noexcept
    {
        sample = finite (sample);
        constexpr float threshold = 0.92f;
        constexpr float ceiling = 0.995f;
        const float magnitude = std::abs (sample);
        if (magnitude <= threshold)
            return sample;

        const float knee = ceiling - threshold;
        const float shaped =
            threshold + knee * std::tanh ((magnitude - threshold) / knee);
        return std::copysign (shaped, sample);
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
            smoothedTime + modSmoothed * 0.16f * timeModDepth);
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
        return clamp01 (smoothedRepeat + modSmoothed * 0.22f * depth);
    }

    int activeSpan (int lane) const noexcept
    {
        const int loop = loopSamples (lane);
        const float repeat = effectiveRepeat();
        const float minFactor = params.og ? 0.0015f : 0.006f;
        const float factor = std::pow (
            minFactor, std::pow (repeat, params.og ? 1.05f : 1.25f));
        const int requestedMinimumSpan = params.og
            ? 24
            : std::max (96, (int) std::lround (sampleRate * 0.003));
        const int minimumSpan = std::min (requestedMinimumSpan, loop);
        return std::clamp (
            (int) std::lround (loop * factor),
            minimumSpan, loop);
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
        // Clean mode must not jump unpredictably into old/empty audio.
        // OG permits a jump only when there is sufficient valid history.
        if (params.og && leftSide && amount > 0.35f
            && lane.validSamples >= std::max (
                1024, (int) (sampleRate * 0.080))
            && randomUnit() < amount * 0.40f)
        {
            lane.previousPlayPosition = lane.playPosition;
            lane.playPosition = randomUnit() * (double) loopSamples (index);
            lane.jumpCrossfadeTotal = std::max (
                1, (int) std::lround (
                    sampleRate * (params.og ? 0.0005 : 0.010)));
            lane.jumpCrossfade = lane.jumpCrossfadeTotal;
        }
    }

    void writeLane (int index, float input) noexcept
    {
        auto& lane = lanes[(size_t) index];
        const int loop = loopSamples (index);
        if (lane.recordPosition >= loop)
            lane.recordPosition = 0;

        const size_t pos = (size_t) lane.recordPosition;
        const float old = lane.writeGeneration[pos] == lane.generation
            ? lane.buffer[pos] : 0.0f;
        lane.buffer[pos] =
            params.echo ? 0.5f * old + 0.5f * input : input;
        lane.writeGeneration[pos] = lane.generation;

        lane.validSamples = std::min (loop, lane.validSamples + 1);
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
        const float a = lane.writeGeneration[(size_t) i0]
            == lane.generation ? lane.buffer[(size_t) i0] : 0.0f;
        const float b = lane.writeGeneration[(size_t) i1]
            == lane.generation ? lane.buffer[(size_t) i1] : 0.0f;
        return a + frac * (b - a);
    }

    float readLane (int index, double speed) noexcept
    {
        auto& lane = lanes[(size_t) index];
        const int loop = loopSamples (index);
        // activeSpan contains two pow() calls. Recompute control only
        // once every 64 audio samples, never at the sample rate.
        if (--lane.spanFramesUntilRefresh <= 0
            || lane.cachedSpan > loop || lane.cachedSpan <= 0)
        {
            lane.cachedSpan = activeSpan (index);
            lane.spanFramesUntilRefresh = 64;
        }
        const int requestedSpan = lane.cachedSpan;
        if (lane.smoothedSpan <= 0.0f)
            lane.smoothedSpan = (float) requestedSpan;
        // Follow moving stutter boundaries over ~50 ms, not instantly.
        lane.smoothedSpan += spanCoeff
            * ((float) requestedSpan - lane.smoothedSpan);
        const int span = std::clamp (
            (int) std::lround (lane.smoothedSpan), 16, loop);
        // Anchor each captured stutter phrase in the ring buffer.
        // Using the moving record head as the start EVERY sample formerly
        // made playback advance twice and caused audible discontinuities.
        if (! lane.stutterStartValid)
        {
            lane.stutterStart = (lane.recordPosition - span + loop) % loop;
            lane.stutterStartValid = true;
        }
        const int start = lane.stutterStart;
        const int nextStart = (lane.recordPosition - span + loop) % loop;

        double local = lane.playPosition;
        while (local < 0.0)
            local += (double) span;
        while (local >= (double) span)
            local -= (double) span;

        const int seam = std::clamp (
            params.og ? 12
                      : (int) std::lround (sampleRate * 0.008),
            8, std::max (8, span / 4));

        auto readSpan = [&] (double localPosition,
                             int windowStart) noexcept
        {
            while (localPosition < 0.0)
                localPosition += (double) span;
            while (localPosition >= (double) span)
                localPosition -= (double) span;

            const double absolute =
                (double) windowStart + localPosition;
            return readLinear (lane, absolute, loop);
        };

        float value = readSpan (local, start);
        if (! params.og && local >= (double) (span - seam))
        {
            const float t =
                (float) ((local - (double) (span - seam)) / (double) seam);
            const double headPosition =
                local - (double) (span - seam);
            // Blend into the NEXT freshly captured phrase at the seam.
            value += (readSpan (headPosition, nextStart) - value) * t;
        }

        const double direction = lane.reverse ? -1.0 : 1.0;
        if (lane.jumpCrossfade > 0)
        {
            const float t = 1.0f
                - (float) lane.jumpCrossfade
                  / (float) std::max (1, lane.jumpCrossfadeTotal);
            const float previous =
                readSpan (lane.previousPlayPosition, start);
            value = previous + (value - previous) * t;
            lane.previousPlayPosition += speed * direction;
            --lane.jumpCrossfade;
        }

        const float levels = (float) (1u << std::clamp (lane.bitDepth, 4, 20));
        value = std::round (value * levels) / levels;
        value = value * lane.randomGain
              + randomBipolar() * lane.noiseAmount;

        // The phrase start is fixed during playback; advance by the
        // requested speed only. Capture the next phrase on each wrap.
        lane.playPosition += speed * direction;
        bool wrapped = false;
        while (lane.playPosition < 0.0)
        {
            lane.playPosition += (double) span;
            wrapped = true;
        }
        while (lane.playPosition >= (double) span)
        {
            lane.playPosition -= (double) span;
            wrapped = true;
        }
        if (wrapped)
            lane.stutterStart = nextStart;

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
        currentSpeed += (params.og ? speedCoeffOg : speedCoeffClean)
            * (target - currentSpeed);

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
        if (filterDistance < 0.015f) return input;
        float& low = filterLow[(size_t) channel];
        float& band = filterBand[(size_t) channel];
        low += filterCoeff * band;
        const float high = input - low - filterQ * band;
        band += filterCoeff * high;
        const float filtered = params.filter < 0.5f ? low : band;
        return input + (filtered - input) * filterDistance;
    }

    void updateModulation() noexcept
    {
        smoothedTime += controlCoeff * (params.time - smoothedTime);
        smoothedRepeat += controlCoeff * (params.repeat - smoothedRepeat);

        smoothedReverb +=
            reverbCoeff * (params.reverb - smoothedReverb);

        if (--modSamplesRemaining <= 0)
        {
            modTarget = randomBipolar();
            modSamplesRemaining = std::max (
                64, (int) std::lround (
                    sampleRate * (0.22 + randomUnit() * 1.4)));
        }

        modSmoothed += modulationCoeff * (modTarget - modSmoothed);
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

        warbleSmoothed += warbleCoeff * (warbleTarget - warbleSmoothed);

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
    float smoothedTime = 0.50f;
    float smoothedRepeat = 0.0f;
    float smoothedReverb = 0.0f;
    float controlCoeff = 0.001f;
    float reverbCoeff = 0.001f;
    float modulationCoeff = 0.001f;
    float speedCoeffClean = 0.001f;
    float speedCoeffOg = 0.001f;
    float warbleCoeff = 0.001f;
    float spanCoeff = 0.001f;
    float filterDistance = 0.0f;
    float filterCoeff = 0.0f;
    float filterQ = 0.0f;

    juce::Reverb hallReverb;
    juce::AudioBuffer<float> warbleBuffer;
    int warbleWrite = 0;
    float warblePhase = 0.0f;
    float warbleTarget = 0.0f;
    float warbleSmoothed = 0.0f;
};
}
