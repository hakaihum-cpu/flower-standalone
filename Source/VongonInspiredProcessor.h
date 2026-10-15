#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Four original EFFECTS modes inspired by the signal-flow ideas of Vongon
// Ultrasheer / Rosetones / Solarfaze / Polyphrase.  This is an independent
// implementation; no Vongon firmware or source code is used.
class VongonInspiredProcessor final
{
public:
    enum Mode
    {
        ultrasheer = 5,
        rosetones = 6,
        solarfaze = 7,
        polyphrase = 8
    };

    void prepare (double requestedRate)
    {
        sampleRate = requestedRate > 1000.0 ? requestedRate : 48000.0;

        ultraDelay.setSize (
            2, juce::jmax (256, juce::roundToInt (sampleRate * 0.050)),
            false, true, false);
        ultraDelay.clear();
        ultraWrite = 0;
        ultraPhase = 0.0;
        ultraRandom = ultraRandomTarget = 0.0f;
        ultraRandomSamples = 1;
        ultraReverb.setSampleRate (sampleRate);
        ultraReverb.reset();

        for (auto& channel : roseState)
            for (auto& resonator : channel)
                resonator = {};
        roseToneLow = { 0.0f, 0.0f };
        rosePhase = 0.0;

        solarLow = { 0.0f, 0.0f };
        solarUpper = { 0.0f, 0.0f };
        solarGateEnvelope = 0.0f;
        solarDelay.setSize (
            2, juce::jmax (256, juce::roundToInt (sampleRate * 0.035)),
            false, true, false);
        solarDelay.clear();
        solarWrite = 0;
        solarPhase = 0.0;

        const int polyCapacity =
            juce::jmax (4096, juce::roundToInt (sampleRate * 22.5));
        polyDelay.setSize (2, polyCapacity, false, true, false);
        polyDelay.clear();
        polyWrite = 0;
        polyLowpass = { 0.0f, 0.0f };
        polyPhase = 0.0;
        polyRandom = polyRandomTarget = 0.0f;
        polyRandomSamples = 1;
        const double initial = sampleRate * 0.42;
        polyCurrentDelay = { initial, initial * 0.75 };
        polyOldDelay = polyCurrentDelay;
        polyTargetDelay = polyCurrentDelay;
        polyCrossfadeRemaining = { 0, 0 };
    }

    void reset() noexcept
    {
        ultraDelay.clear();
        ultraWrite = 0;
        ultraReverb.reset();

        for (auto& channel : roseState)
            for (auto& resonator : channel)
                resonator = {};
        roseToneLow = { 0.0f, 0.0f };

        solarLow = { 0.0f, 0.0f };
        solarUpper = { 0.0f, 0.0f };
        solarDelay.clear();
        solarWrite = 0;

        polyDelay.clear();
        polyWrite = 0;
        polyLowpass = { 0.0f, 0.0f };
        polyCrossfadeRemaining = { 0, 0 };
    }

    void process (
        juce::AudioBuffer<float>& buffer,
        int mode,
        float x,
        float y,
        bool touchDown) noexcept
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        y = juce::jlimit (0.0f, 1.0f, y);

        switch (mode)
        {
            case ultrasheer: processUltrasheer (buffer, x, y); break;
            case rosetones: processRosetones (buffer, x, y); break;
            case solarfaze: processSolarfaze (buffer, x, y); break;
            case polyphrase: processPolyphrase (buffer, x, y, touchDown); break;
            default: break;
        }
    }

private:
    struct ResonatorState
    {
        float y1 = 0.0f;
        float y2 = 0.0f;
    };

    static float protect (float sample) noexcept
    {
        if (! std::isfinite (sample))
            return 0.0f;
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

    static float onePoleCoeff (double sr, float hz) noexcept
    {
        const float clamped =
            juce::jlimit (10.0f, (float) (sr * 0.45), hz);
        return 1.0f - std::exp (
            -2.0f * juce::MathConstants<float>::pi
            * clamped / (float) sr);
    }

    float readDelay (
        const juce::AudioBuffer<float>& delay,
        int channel,
        int write,
        double samples) const noexcept
    {
        const int size = delay.getNumSamples();
        if (size <= 2)
            return 0.0f;

        samples = juce::jlimit (1.0, (double) (size - 2), samples);
        double pos = (double) write - samples;
        while (pos < 0.0)
            pos += (double) size;
        while (pos >= (double) size)
            pos -= (double) size;

        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float frac = (float) (pos - (double) i0);
        const float a = delay.getSample (channel, i0);
        const float b = delay.getSample (channel, i1);
        return a + (b - a) * frac;
    }

    void processUltrasheer (
        juce::AudioBuffer<float>& buffer,
        float x,
        float y) noexcept
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        const int samples = buffer.getNumSamples();
        if (channels <= 0 || samples <= 0)
            return;

        // X: vibrato depth/rate and cycle->random character.
        // Y: plate decay / brightness / wet amount.
        const float depthMs = 0.12f + 3.0f * x * x;
        const float rateHz = 0.18f + 3.2f * x;
        const float randomBlend =
            juce::jlimit (0.0f, 1.0f, (x - 0.55f) / 0.45f);
        const float wet = 0.12f + 0.66f * y;
        const float baseMs = 7.0f;

        const float randomSmooth =
            1.0f - std::exp (
                -1.0f / juce::jmax (
                    1.0f, (float) sampleRate * 0.080f));

        for (int i = 0; i < samples; ++i)
        {
            if (--ultraRandomSamples <= 0)
            {
                ultraRandomState ^= ultraRandomState << 13;
                ultraRandomState ^= ultraRandomState >> 17;
                ultraRandomState ^= ultraRandomState << 5;
                ultraRandomTarget =
                    ((float) (ultraRandomState & 0xffffu) / 32767.5f) - 1.0f;
                ultraRandomSamples = juce::jmax (
                    64, juce::roundToInt (
                        sampleRate / juce::jmax (0.1f, rateHz * 1.6f)));
            }
            ultraRandom +=
                randomSmooth * (ultraRandomTarget - ultraRandom);

            const float sine = std::sin ((float) ultraPhase);
            const float mod =
                sine + (ultraRandom - sine) * randomBlend;
            ultraPhase +=
                juce::MathConstants<double>::twoPi
                * (double) rateHz / sampleRate;
            if (ultraPhase >= juce::MathConstants<double>::twoPi)
                ultraPhase -= juce::MathConstants<double>::twoPi;

            for (int ch = 0; ch < channels; ++ch)
            {
                const float dry = buffer.getSample (ch, i);
                ultraDelay.setSample (ch, ultraWrite, dry);
                const float polarity = ch == 0 ? 1.0f : -1.0f;
                const double delaySamples =
                    sampleRate * (baseMs + depthMs * mod * polarity)
                    / 1000.0;
                const float vib =
                    readDelay (ultraDelay, ch, ultraWrite, delaySamples);
                buffer.setSample (
                    ch, i,
                    dry + (vib - dry) * (0.12f + 0.55f * x));
            }

            ultraWrite = (ultraWrite + 1) % ultraDelay.getNumSamples();
        }

        juce::Reverb::Parameters rev;
        rev.roomSize = 0.35f + 0.62f * y;
        rev.damping = 0.72f - 0.46f * y;
        rev.wetLevel = wet;
        rev.dryLevel = 1.0f - 0.42f * wet;
        rev.width = 1.0f;
        rev.freezeMode = 0.0f;
        ultraReverb.setParameters (rev);

        if (channels >= 2)
            ultraReverb.processStereo (
                buffer.getWritePointer (0),
                buffer.getWritePointer (1), samples);
        else
            ultraReverb.processMono (
                buffer.getWritePointer (0), samples);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* out = buffer.getWritePointer (ch);
            for (int i = 0; i < samples; ++i)
            {
                // 16-bit-ish legacy digital texture without harsh truncation.
                const float q =
                    std::round (out[i] * 32768.0f) / 32768.0f;
                out[i] = protect (q);
            }
        }
    }

    void processRosetones (
        juce::AudioBuffer<float>& buffer,
        float x,
        float y) noexcept
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        const int samples = buffer.getNumSamples();
        if (channels <= 0 || samples <= 0)
            return;

        // X chooses octave/register and tone. Y controls sensitivity/resonance.
        const int octave =
            x < 0.25f ? 3
            : x < 0.50f ? 4
            : x < 0.75f ? 5
                        : 6;
        const float sensitivity = 0.45f + 1.75f * y;
        const float radius = juce::jlimit (
            0.88f, 0.9965f, 0.91f + y * 0.084f);
        const float toneCoeff = onePoleCoeff (
            sampleRate, 900.0f + 9200.0f * x * x);
        const float synthMix = 0.12f + 0.66f * y;
        const float modDepth = 0.0007f + 0.004f * x * y;
        const float modRate = 0.07f + 0.26f * x;

        for (int i = 0; i < samples; ++i)
        {
            const float mod =
                std::sin ((float) rosePhase) * modDepth;
            rosePhase +=
                juce::MathConstants<double>::twoPi
                * (double) modRate / sampleRate;
            if (rosePhase >= juce::MathConstants<double>::twoPi)
                rosePhase -= juce::MathConstants<double>::twoPi;

            float dry[2] { 0.0f, 0.0f };
            for (int ch = 0; ch < channels; ++ch)
                dry[ch] = buffer.getSample (ch, i);

            const float mono =
                channels > 1 ? 0.5f * (dry[0] + dry[1]) : dry[0];
            const float excitation =
                std::tanh (mono * sensitivity);

            float synth[2] { 0.0f, 0.0f };
            for (int note = 0; note < 12; ++note)
            {
                const double midi = 12.0 * (double) octave + (double) note;
                const double frequency =
                    440.0 * std::pow (2.0, (midi - 69.0) / 12.0)
                    * (1.0 + mod * (note % 2 == 0 ? 1.0 : -1.0));
                const double omega =
                    juce::MathConstants<double>::twoPi
                    * frequency / sampleRate;
                const float a1 =
                    2.0f * radius * (float) std::cos (omega);
                const float a2 = -(radius * radius);

                for (int ch = 0; ch < channels; ++ch)
                {
                    auto& state = roseState[(size_t) ch][(size_t) note];
                    const float inputGain =
                        ((note + ch) % 2 == 0 ? 1.0f : 0.72f)
                        * (0.012f + 0.012f * y);
                    const float next =
                        excitation * inputGain
                        + a1 * state.y1
                        + a2 * state.y2;
                    state.y2 = state.y1;
                    state.y1 = protect (next);
                    synth[ch] += state.y1;
                }
            }

            for (int ch = 0; ch < channels; ++ch)
            {
                synth[ch] *= 0.11f;
                roseToneLow[(size_t) ch] +=
                    toneCoeff * (synth[ch] - roseToneLow[(size_t) ch]);
                const float shaped =
                    std::tanh (roseToneLow[(size_t) ch] * 1.25f);
                buffer.setSample (
                    ch, i,
                    protect (
                        dry[ch]
                        + shaped * synthMix));
            }
        }
    }

    void processSolarfaze (
        juce::AudioBuffer<float>& buffer,
        float x,
        float y) noexcept
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        const int samples = buffer.getNumSamples();
        if (channels <= 0 || samples <= 0)
            return;

        // X: multiband saturation balance/drive. Y: dynamic stereo detune.
        const float lowCoeff = onePoleCoeff (sampleRate, 240.0f);
        const float upperCoeff = onePoleCoeff (sampleRate, 2600.0f);
        const float driveLow = 1.15f + 3.0f * x;
        const float driveMid = 1.10f + 4.2f * x;
        const float driveHigh = 1.05f + 2.4f * x;
        const float detuneDepthMs = 0.18f + 2.4f * y;
        const float mix = 0.16f + 0.68f * y;

        for (int i = 0; i < samples; ++i)
        {
            float dry[2] { 0.0f, 0.0f };
            float saturated[2] { 0.0f, 0.0f };

            for (int ch = 0; ch < channels; ++ch)
            {
                dry[ch] = buffer.getSample (ch, i);
                solarGateEnvelope +=
                    0.0025f * (std::abs (dry[ch]) - solarGateEnvelope);

                solarLow[(size_t) ch] +=
                    lowCoeff * (dry[ch] - solarLow[(size_t) ch]);
                solarUpper[(size_t) ch] +=
                    upperCoeff * (dry[ch] - solarUpper[(size_t) ch]);

                const float low = solarLow[(size_t) ch];
                const float high = dry[ch] - solarUpper[(size_t) ch];
                const float mid = dry[ch] - low - high;

                const float gate =
                    solarGateEnvelope > 0.0016f ? 1.0f : 0.0f;
                saturated[ch] =
                    gate * (
                        std::tanh (low * driveLow) * 0.95f
                        + std::tanh (mid * driveMid) * 0.78f
                        + std::tanh (high * driveHigh) * 0.62f);

                solarDelay.setSample (
                    ch, solarWrite, protect (saturated[ch]));
            }

            const float envMod =
                juce::jlimit (0.0f, 1.0f, solarGateEnvelope * 7.0f);
            const float lfo = std::sin ((float) solarPhase);
            solarPhase +=
                juce::MathConstants<double>::twoPi
                * (0.12 + 0.55 * (double) y) / sampleRate;
            if (solarPhase >= juce::MathConstants<double>::twoPi)
                solarPhase -= juce::MathConstants<double>::twoPi;

            for (int ch = 0; ch < channels; ++ch)
            {
                const float polarity = ch == 0 ? 1.0f : -1.0f;
                const double delaySamples =
                    sampleRate
                    * (3.1f
                       + detuneDepthMs
                         * (0.35f + 0.65f * envMod)
                         * lfo * polarity)
                    / 1000.0;
                const float detuned =
                    readDelay (
                        solarDelay, ch, solarWrite, delaySamples);
                buffer.setSample (
                    ch, i,
                    protect (
                        dry[ch]
                        + (detuned - dry[ch]) * mix));
            }

            solarWrite = (solarWrite + 1) % solarDelay.getNumSamples();
        }
    }

    void updatePolyTarget (
        int channel,
        double targetSamples) noexcept
    {
        targetSamples = juce::jlimit (
            8.0,
            (double) (polyDelay.getNumSamples() - 2),
            targetSamples);

        if (std::abs (targetSamples - polyTargetDelay[(size_t) channel])
            < 16.0)
            return;

        polyOldDelay[(size_t) channel] =
            polyCrossfadeRemaining[(size_t) channel] > 0
                ? polyTargetDelay[(size_t) channel]
                : polyCurrentDelay[(size_t) channel];
        polyTargetDelay[(size_t) channel] = targetSamples;
        polyCrossfadeRemaining[(size_t) channel] =
            juce::jmax (64, juce::roundToInt (sampleRate * 0.045));
    }

    void processPolyphrase (
        juce::AudioBuffer<float>& buffer,
        float x,
        float y,
        bool touchDown) noexcept
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        const int samples = buffer.getNumSamples();
        if (channels <= 0 || samples <= 0)
            return;

        // Log-like 85 ms .. 22 s time range.
        const double seconds =
            0.085 * std::pow (22.0 / 0.085, (double) x);
        updatePolyTarget (0, seconds * sampleRate);
        updatePolyTarget (
            1,
            seconds * sampleRate * (0.58 + 0.38 * (double) y));

        const float feedback =
            juce::jlimit (0.0f, 1.08f, 0.12f + 0.94f * y);
        const float mix = 0.12f + 0.68f * y;
        const float tone = 0.08f + 0.82f * (1.0f - y);
        const float toneCoeff =
            onePoleCoeff (sampleRate, 650.0f + 12500.0f * tone * tone);
        const float modDepthSamples =
            (float) sampleRate
            * (0.00015f + 0.0018f * y);
        const float modRate = 0.05f + 0.33f * x;

        for (int i = 0; i < samples; ++i)
        {
            if (--polyRandomSamples <= 0)
            {
                polyRandomState ^= polyRandomState << 13;
                polyRandomState ^= polyRandomState >> 17;
                polyRandomState ^= polyRandomState << 5;
                polyRandomTarget =
                    ((float) (polyRandomState & 0xffffu) / 32767.5f) - 1.0f;
                polyRandomSamples = juce::jmax (
                    128, juce::roundToInt (
                        sampleRate / juce::jmax (0.05f, modRate)));
            }
            polyRandom +=
                0.0008f * (polyRandomTarget - polyRandom);
            polyPhase +=
                juce::MathConstants<double>::twoPi
                * (double) modRate / sampleRate;
            if (polyPhase >= juce::MathConstants<double>::twoPi)
                polyPhase -= juce::MathConstants<double>::twoPi;
            const float mod =
                0.55f * std::sin ((float) polyPhase)
                + 0.45f * polyRandom;

            float dry[2] { 0.0f, 0.0f };
            float delayed[2] { 0.0f, 0.0f };

            for (int ch = 0; ch < channels; ++ch)
            {
                dry[ch] = buffer.getSample (ch, i);

                double readDelaySamples =
                    polyCurrentDelay[(size_t) ch];
                if (polyCrossfadeRemaining[(size_t) ch] > 0)
                {
                    const int total =
                        juce::jmax (64, juce::roundToInt (sampleRate * 0.045));
                    const float t =
                        1.0f
                        - (float) polyCrossfadeRemaining[(size_t) ch]
                          / (float) total;
                    const float oldRead = readDelay (
                        polyDelay, ch, polyWrite,
                        polyOldDelay[(size_t) ch]
                        + mod * modDepthSamples);
                    const float newRead = readDelay (
                        polyDelay, ch, polyWrite,
                        polyTargetDelay[(size_t) ch]
                        + mod * modDepthSamples);
                    delayed[ch] =
                        oldRead + (newRead - oldRead) * t;

                    if (--polyCrossfadeRemaining[(size_t) ch] <= 0)
                        polyCurrentDelay[(size_t) ch] =
                            polyTargetDelay[(size_t) ch];
                }
                else
                {
                    delayed[ch] = readDelay (
                        polyDelay, ch, polyWrite,
                        readDelaySamples + mod * modDepthSamples);
                }

                polyLowpass[(size_t) ch] +=
                    toneCoeff
                    * (delayed[ch] - polyLowpass[(size_t) ch]);
            }

            // Middle/right XY area progressively becomes ping-pong.
            const float pingPong =
                juce::jlimit (0.0f, 1.0f, (x - 0.35f) / 0.45f);
            for (int ch = 0; ch < channels; ++ch)
            {
                const int other = channels > 1 ? 1 - ch : ch;
                const float fbSignal =
                    polyLowpass[(size_t) ch] * (1.0f - pingPong)
                    + polyLowpass[(size_t) other] * pingPong;
                const float infiniteBoost =
                    touchDown && y > 0.93f ? 1.04f : feedback;
                const float write =
                    protect (dry[ch] + fbSignal * infiniteBoost);
                polyDelay.setSample (ch, polyWrite, write);
                buffer.setSample (
                    ch, i,
                    protect (
                        dry[ch]
                        + (delayed[ch] - dry[ch]) * mix));
            }

            polyWrite = (polyWrite + 1) % polyDelay.getNumSamples();
        }
    }

    double sampleRate = 48000.0;

    juce::AudioBuffer<float> ultraDelay;
    int ultraWrite = 0;
    double ultraPhase = 0.0;
    uint32_t ultraRandomState = 0x554c5452u;
    float ultraRandom = 0.0f;
    float ultraRandomTarget = 0.0f;
    int ultraRandomSamples = 1;
    juce::Reverb ultraReverb;

    std::array<std::array<ResonatorState, 12>, 2> roseState {};
    std::array<float, 2> roseToneLow { 0.0f, 0.0f };
    double rosePhase = 0.0;

    std::array<float, 2> solarLow { 0.0f, 0.0f };
    std::array<float, 2> solarUpper { 0.0f, 0.0f };
    float solarGateEnvelope = 0.0f;
    juce::AudioBuffer<float> solarDelay;
    int solarWrite = 0;
    double solarPhase = 0.0;

    juce::AudioBuffer<float> polyDelay;
    int polyWrite = 0;
    std::array<float, 2> polyLowpass { 0.0f, 0.0f };
    std::array<double, 2> polyCurrentDelay { 0.0, 0.0 };
    std::array<double, 2> polyOldDelay { 0.0, 0.0 };
    std::array<double, 2> polyTargetDelay { 0.0, 0.0 };
    std::array<int, 2> polyCrossfadeRemaining { 0, 0 };
    double polyPhase = 0.0;
    uint32_t polyRandomState = 0x504f4c59u;
    float polyRandom = 0.0f;
    float polyRandomTarget = 0.0f;
    int polyRandomSamples = 1;
};
