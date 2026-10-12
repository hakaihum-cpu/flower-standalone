#pragma once
#include <JuceHeader.h>

#if JUCE_ANDROID
#include "clouds/dsp/granular_processor.h"
#include <array>
#include <cmath>
#include <cstdint>

// Android-only JUCE bridge around the original Mutable Instruments Clouds DSP.
// The original MIT-licensed Clouds/stmlib source is materialized unchanged
// except for the same Android host portability shims used by MIYAKO.
// No allocation occurs in process().
class FlowerCloudsEngine final
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
        visualEnvelope = 0.0f;
        activity.store (0.0f, std::memory_order_relaxed);

        engine.Init (largeBuffer.data(), largeBuffer.size(),
                     smallBuffer.data(), smallBuffer.size());
        engine.set_quality (0); // 16-bit, true stereo
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
                  float y) noexcept
    {
        if (! prepared || buffer.getNumChannels() <= 0)
            return;

        x = juce::jlimit (0.0f, 1.0f, x);
        y = juce::jlimit (0.0f, 1.0f, y);

        auto* params = engine.mutable_parameters();

        // FLOWER performance mapping.  X/Y remain the single performance
        // surface; Clouds follows the gesture instead of introducing another
        // hidden control page.
        params->position = x;
        params->size = juce::jlimit (0.0f, 1.0f, 0.16f + y * 0.74f);
        params->density = juce::jlimit (0.0f, 1.0f, 0.24f + y * 0.66f);
        params->texture = juce::jlimit (0.0f, 1.0f, 0.20f + x * 0.72f);
        params->pitch = 0.0f;
        params->stereo_spread = 0.72f;
        params->feedback = juce::jlimit (0.0f, 0.55f, 0.12f + y * 0.28f);
        params->reverb = juce::jlimit (0.0f, 0.75f, 0.28f + y * 0.36f);
        params->dry_wet = 1.0f;
        params->freeze = false;
        params->trigger = false;
        params->gate = true;

        const float targetWet = enabled ? (0.22f + y * 0.16f) : 0.0f;
        const float step = static_cast<float> (
            1.0 / juce::jmax (1.0, 0.035 * hostRate));
        const double ratio = nativeRate / hostRate;

        const int channels = buffer.getNumChannels();
        const int length = buffer.getNumSamples();
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

            // Additive host mix: enabling Clouds never turns the dry signal
            // down.  This mirrors the accepted MIYAKO dry/wet separation.
            left[n] = dryL + wetL * wetMix;
            if (right)
                right[n] = dryR + wetR * wetMix;

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
    }

private:
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

    // Original Clouds hardware work-buffer sizes.
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
    float visualEnvelope = 0.0f;
    bool prepared = false;
};

#else

class FlowerCloudsEngine final
{
public:
    void prepare (double) noexcept {}
    float getActivity() const noexcept { return 0.0f; }
    void process (juce::AudioBuffer<float>&, bool, float, float) noexcept {}
};

#endif
