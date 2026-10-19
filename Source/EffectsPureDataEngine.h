#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include "z_libpd.h"

// Real Pure Data Vanilla engine. Pd owns no Android audio device: JUCE supplies
// and receives L/R PCM. libpd's fixed 64-frame ticks are adapted without
// allocations, file I/O or graph mutation in the audio callback.
class EffectsPureDataEngine final
{
public:
    EffectsPureDataEngine()
    {
        // There is exactly one Pd engine per EFFECTS standalone process.
        // libpd_init() is idempotent (-1 means already initialized).
        libpd_init();
    }

    ~EffectsPureDataEngine()
    {
        const juce::SpinLock::ScopedLockType guard (engineLock);
        closeUnsafe();
    }

    void prepare (double rate)
    {
        const juce::SpinLock::ScopedLockType guard (engineLock);
        prepared = false;
        closeUnsafe();
        clearFifo();
        if (rate < 1000.0 || rate > 192000.0)
            return;
        if (libpd_blocksize() != tickFrames)
            return;
        if (libpd_init_audio (2, 2, juce::roundToInt (rate)) != 0)
            return;
        prepared = true;
        // libpd_init_audio configures buffers; Pd also requires DSP ON.
        libpd_start_message (1);
        libpd_add_float (1.0f);
        libpd_finish_message ("pd", "dsp");
        if (selectedFile.existsAsFile())
            patch = libpd_openfile (
                selectedFile.getFileName().toRawUTF8(),
                selectedFile.getParentDirectory().getFullPathName().toRawUTF8());
    }

    bool load (const juce::File& file, juce::String& error)
    {
        if (! file.existsAsFile() || file.getFileExtension() != ".pd")
        {
            error = "Choose an existing Pure Data .pd patch";
            return false;
        }
        const juce::SpinLock::ScopedLockType guard (engineLock);
        if (! prepared)
        {
            error = "libpd DSP is not ready";
            return false;
        }
        const auto filename = file.getFileName().toRawUTF8();
        const auto folder = file.getParentDirectory().getFullPathName().toRawUTF8();
        void* opened = libpd_openfile (filename, folder);
        if (opened == nullptr)
        {
            error = "libpd could not open .pd patch";
            return false;
        }
        closeUnsafe();
        patch = opened;
        selectedFile = file;
        clearFifo();
        error.clear();
        return true;
    }

    juce::String activeName() const
    {
        const juce::SpinLock::ScopedLockType guard (engineLock);
        return selectedFile.getFileNameWithoutExtension();
    }

    bool isReady() const
    {
        const juce::SpinLock::ScopedLockType guard (engineLock);
        return prepared && patch != nullptr;
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        // Fail-safe: if a user imports/reloads a patch concurrently, do not
        // block the Android audio thread while libpd manipulates its graph.
        const juce::SpinLock::ScopedTryLockType guard (engineLock);
        if (! guard.isLocked() || ! prepared || patch == nullptr
            || buffer.getNumChannels() < 2)
        {
            buffer.clear();
            return;
        }

        const int samples = buffer.getNumSamples();
        for (int n = 0; n < samples; ++n)
        {
            const float left = buffer.getSample (0, n);
            const float right = buffer.getSample (1, n);
            const bool outputAvailable = outputCursor < tickFrames;
            const float wetLeft = outputAvailable
                ? outputFifo[(size_t) outputCursor * 2] : 0.0f;
            const float wetRight = outputAvailable
                ? outputFifo[(size_t) outputCursor * 2 + 1] : 0.0f;
            if (outputAvailable)
                ++outputCursor;

            inputFifo[(size_t) inputCursor * 2] =
                std::isfinite (left) ? left : 0.0f;
            inputFifo[(size_t) inputCursor * 2 + 1] =
                std::isfinite (right) ? right : 0.0f;

            if (++inputCursor == tickFrames)
            {
                const int status = libpd_process_float (
                    1, inputFifo.data(), outputFifo.data());
                inputCursor = 0;
                outputCursor = status == 0 ? 0 : tickFrames;
                if (status != 0)
                    outputFifo.fill (0.0f);
            }

            // Same soft ceiling for both channels; final EFFECTS WET/DRY
            // and output safety remain in the existing common processing.
            buffer.setSample (
                0, n, juce::jlimit (-1.0f, 1.0f, wetLeft));
            buffer.setSample (
                1, n, juce::jlimit (-1.0f, 1.0f, wetRight));
        }
    }

private:
    void clearFifo() noexcept
    {
        inputFifo.fill (0.0f);
        outputFifo.fill (0.0f);
        inputCursor = 0;
        outputCursor = tickFrames;
    }
    void closeUnsafe() noexcept
    {
        if (patch != nullptr)
            libpd_closefile (patch);
        patch = nullptr;
    }

    static constexpr int tickFrames = 64;
    mutable juce::SpinLock engineLock;
    std::array<float, tickFrames * 2> inputFifo {};
    std::array<float, tickFrames * 2> outputFifo {};
    juce::File selectedFile;
    int inputCursor = 0;
    int outputCursor = tickFrames;
    void* patch = nullptr;
    bool prepared = false;
};
