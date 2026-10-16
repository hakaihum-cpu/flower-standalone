#pragma once
#include <JuceHeader.h>

#if JUCE_ANDROID
#include "clouds/dsp/granular_processor.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

// Android JUCE bridge around the original Mutable Instruments Clouds DSP.
// The engine runs at Clouds' native 32 kHz / 32-sample block size and performs
// fixed-size host-rate conversion without allocating in process().
class EffectsCloudsEngine final
{
public:
    static constexpr double nativeRate = 32000.0;
    static constexpr int nativeBlock = 32;
    using Frame = clouds::ShortFrame;

    void prepare (double requestedSampleRate) noexcept
    {
        hostRate = requestedSampleRate > 0.0 ? requestedSampleRate : 48000.0;
        inputPhase = outputPhase = 0.0;
        lastInput = { 0.0f, 0.0f };
        lastOutput = nextOutput = { 0.0f, 0.0f };
        inputCount = 0;
        outputRead = outputWrite = 0;
        wetMix = 0.0f;
        smoothedX = 0.5f;
        smoothedY = 0.5f;
        visualEnvelope = 0.0f;
        previousTouchDown = false;
        activity.store (0.0f, std::memory_order_relaxed);

        engine.Init (largeBuffer.data(), largeBuffer.size(),
                     smallBuffer.data(), smallBuffer.size());
        engine.set_quality (0);
        engine.set_playback_mode (clouds::PLAYBACK_MODE_GRANULAR);

        auto* params = engine.mutable_parameters();
        *params = {};
        params->position = 0.5f;
        params->size = 0.5f;
        params->density = 0.5f;
        params->texture = 0.5f;
        params->pitch = 0.0f;
        params->stereo_spread = 0.65f;
        params->feedback = 0.18f;
        params->reverb = 0.35f;
        params->dry_wet = 1.0f;
        params->freeze = false;
        params->trigger = false;
        params->gate = true;

        engine.Prepare();
        prepared = true;
    }

    float getActivity() const noexcept
    {
        return activity.load (std::memory_order_relaxed);
    }

    void process (juce::AudioBuffer<float>& buffer,
                  bool enabled,
                  float x,
                  float y,
                  bool touchDown) noexcept
    {
        if (! prepared || buffer.getNumChannels() <= 0)
            return;

        x = juce::jlimit (0.0f, 1.0f, x);
        y = juce::jlimit (0.0f, 1.0f, y);

        const int channels = buffer.getNumChannels();
        const int length = buffer.getNumSamples();

        if (! enabled && wetMix < 0.0001f)
        {
            wetMix = 0.0f;
            activity.store (0.0f, std::memory_order_relaxed);
            return;
        }

        const float xyAlpha = juce::jlimit (
            0.0f, 1.0f,
            static_cast<float> (
                1.0 - std::exp (
                    -static_cast<double> (juce::jmax (1, length))
                    / juce::jmax (1.0, hostRate * 0.025))));
        smoothedX += (x - smoothedX) * xyAlpha;
        smoothedY += (y - smoothedY) * xyAlpha;

        auto* params = engine.mutable_parameters();

        // Dedicated CLOUDS mode: every Clouds parameter is derived from the
        // same XY/touch surface.  Continuous parameters use X, Y or X*Y.
        // FREEZE occupies the upper-left zone, TRIGGER is a one-block pulse
        // on a new touch, and GATE follows touch-down state.
        // Keep the central performance area musical.  Most of the XY
        // plane stays at unshifted pitch; only the far left/right edges enter
        // consonant interval zones.  Other parameters avoid the destructive
        // extremes of the raw Clouds ranges while still covering the full
        // character of the effect.
        params->position = juce::jlimit (
            0.0f, 1.0f, 0.08f + smoothedX * 0.84f);
        params->size = juce::jlimit (
            0.0f, 1.0f, 0.14f + smoothedY * 0.72f);
        params->density = juce::jlimit (
            0.0f, 1.0f, 0.28f + smoothedY * 0.44f);
        params->texture = juce::jlimit (
            0.0f, 1.0f, 0.24f + smoothedX * 0.52f);

        const float pitchSemitones =
            smoothedX < 0.10f ? -12.0f
            : smoothedX < 0.22f ? -7.0f
            : smoothedX > 0.90f ? 12.0f
            : smoothedX > 0.78f ? 7.0f
                                : 0.0f;
        params->pitch = pitchSemitones;

        params->stereo_spread = juce::jlimit (
            0.0f, 1.0f, 0.18f + smoothedX * 0.58f);
        params->feedback = juce::jlimit (
            0.0f, 0.42f, 0.03f + smoothedY * 0.34f);
        params->reverb = juce::jlimit (
            0.0f, 0.72f,
            0.02f + std::pow (smoothedX * smoothedY, 1.35f) * 0.68f);
        params->dry_wet = juce::jlimit (
            0.18f, 0.88f,
            0.24f + 0.58f * (0.30f * smoothedX + 0.70f * smoothedY));

        const bool freezeZone =
            smoothedY > 0.93f && smoothedX < 0.24f;
        params->freeze = freezeZone;
        params->trigger = touchDown && ! previousTouchDown;
        params->gate = touchDown;

        const float targetWet = enabled ? 1.0f : 0.0f;
        const float step = static_cast<float> (
            1.0 / juce::jmax (1.0, 0.035 * hostRate));
        const double ratio = nativeRate / hostRate;
        float* left = buffer.getWritePointer (0);
        float* right = channels > 1 ? buffer.getWritePointer (1) : nullptr;

        float peak = 0.0f;

        for (int n = 0; n < length; ++n)
        {
            const float dryL = std::isfinite (left[n]) ? left[n] : 0.0f;
            const float dryR = right
                ? (std::isfinite (right[n]) ? right[n] : 0.0f)
                : dryL;

            inputPhase += ratio;
            if (inputPhase >= 1.0)
            {
                inputPhase -= 1.0;
                const float frac = static_cast<float> (
                    1.0 - inputPhase / ratio);
                const float cloudL =
                    lastInput.l + (dryL - lastInput.l) * frac;
                const float cloudR =
                    lastInput.r + (dryR - lastInput.r) * frac;

                inputBlock[static_cast<size_t> (inputCount++)] = {
                    toInt16 (cloudL), toInt16 (cloudR)
                };

                if (inputCount == nativeBlock)
                {
                    engine.Prepare();
                    engine.Process (
                        inputBlock.data(), outputBlock.data(), nativeBlock);

                    for (const auto& sample : outputBlock)
                    {
                        const unsigned next =
                            (outputWrite + 1u) % outputRing.size();
                        if (next == outputRead)
                            outputRead =
                                (outputRead + 1u) % outputRing.size();

                        outputRing[outputWrite] = sample;
                        outputWrite = next;
                    }

                    inputCount = 0;
                }
            }

            lastInput = { dryL, dryR };

            outputPhase += ratio;
            if (outputPhase >= 1.0)
            {
                outputPhase -= 1.0;
                lastOutput = nextOutput;

                if (outputRead != outputWrite)
                {
                    const auto out = outputRing[outputRead];
                    outputRead = (outputRead + 1u) % outputRing.size();
                    nextOutput = {
                        static_cast<float> (out.l) / 32768.0f,
                        static_cast<float> (out.r) / 32768.0f
                    };
                }
                else
                {
                    nextOutput = { 0.0f, 0.0f };
                }
            }

            const float interp = static_cast<float> (outputPhase);
            const float wetL =
                lastOutput.l + (nextOutput.l - lastOutput.l) * interp;
            const float wetR =
                lastOutput.r + (nextOutput.r - lastOutput.r) * interp;

            wetMix += juce::jlimit (-step, step, targetWet - wetMix);

            // CLOUDS outputs granular audio only; input is still captured
            // and analysed, but never leaks into the monitoring output.
            left[n] = softProtect (wetL * wetMix);
            if (right)
                right[n] = softProtect (wetR * wetMix);

            peak = juce::jmax (
                peak,
                juce::jmax (std::abs (wetL * wetMix),
                            std::abs (wetR * wetMix)));
        }

        visualEnvelope +=
            (peak - visualEnvelope)
            * (peak > visualEnvelope ? 0.45f : 0.10f);
        activity.store (
            juce::jlimit (0.0f, 1.0f, visualEnvelope * 2.8f),
            std::memory_order_relaxed);
        previousTouchDown = touchDown;
    }

private:
    static float softProtect (float sample) noexcept
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

    static int16_t toInt16 (float value) noexcept
    {
        const float finite = std::isfinite (value) ? value : 0.0f;
        return static_cast<int16_t> (juce::roundToInt (
            juce::jlimit (-32767.0f, 32767.0f, finite * 32767.0f)));
    }

    struct StereoFloat
    {
        float l = 0.0f;
        float r = 0.0f;
    };

    clouds::GranularProcessor engine;

    alignas (16) std::array<uint8_t, 118784> largeBuffer {};
    alignas (16) std::array<uint8_t, 65408> smallBuffer {};

    std::array<Frame, nativeBlock> inputBlock {};
    std::array<Frame, nativeBlock> outputBlock {};
    std::array<Frame, 4096> outputRing {};

    std::atomic<float> activity { 0.0f };

    size_t outputRead = 0;
    size_t outputWrite = 0;
    int inputCount = 0;

    double hostRate = 48000.0;
    double inputPhase = 0.0;
    double outputPhase = 0.0;

    StereoFloat lastInput;
    StereoFloat lastOutput;
    StereoFloat nextOutput;

    float wetMix = 0.0f;
    float smoothedX = 0.5f;
    float smoothedY = 0.5f;
    float visualEnvelope = 0.0f;
    bool previousTouchDown = false;
    bool prepared = false;
};

#else

class EffectsCloudsEngine final
{
public:
    void prepare (double) noexcept {}
    float getActivity() const noexcept { return 0.0f; }
    void process (juce::AudioBuffer<float>&, bool, float, float, bool) noexcept {}
};

#endif
