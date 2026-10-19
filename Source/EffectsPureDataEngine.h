#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <cstdint>
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
        {
            const juce::String filename = selectedFile.getFileName();
            const juce::String folder =
                selectedFile.getParentDirectory().getFullPathName();
            patch = libpd_openfile (filename.toRawUTF8(), folder.toRawUTF8());
        }
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
        // Own both UTF-8 backing strings throughout libpd_openfile().
        // Capturing toRawUTF8() from temporary juce::String values would
        // leave dangling pointers before this call.
        const juce::String filename = file.getFileName();
        const juce::String folder =
            file.getParentDirectory().getFullPathName();
        void* opened = libpd_openfile (filename.toRawUTF8(), folder.toRawUTF8());
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


    // Capture the incoming MIDI before EFFECTS rebuilds its MIDI OUT buffer.
    // Bounded, allocation-free, and restricted to the PATCH mode caller.
    void captureMidi (const juce::MidiBuffer& midi) noexcept
    {
        const juce::SpinLock::ScopedTryLockType guard (engineLock);
        if (! guard.isLocked())
            return;
        pendingCount = 0;
        if (! prepared || patch == nullptr)
            return;

        for (const auto event : midi)
        {
            const auto& message = event.getMessage();
            MidiEvent entry {};
            entry.sample = event.samplePosition;
            if (message.isNoteOnOrOff())
            {
                entry.kind = MidiEvent::note;
                entry.channel = message.getChannel() - 1; // libpd is 0-based.
                entry.number = message.getNoteNumber();
                entry.value = message.isNoteOff() ? 0 : (int) message.getVelocity();
            }
            else if (message.isMidiStart())
                entry.kind = MidiEvent::start;
            else if (message.isMidiContinue())
                entry.kind = MidiEvent::resume;
            else if (message.isMidiStop())
                entry.kind = MidiEvent::stop;
            else if (message.isMidiClock())
                entry.kind = MidiEvent::clock;
            else
                continue;

            if (pendingCount < (int) pendingMidi.size())
                pendingMidi[(size_t) pendingCount++] = entry;
            else
                midiOverflow = true;
        }
    }

    void process (juce::AudioBuffer<float>& buffer,
                  float x, float y, bool touching) noexcept
    {
        // Fail-safe: if a user imports/reloads a patch concurrently, do not
        // block the Android audio thread while libpd manipulates its graph.
        const juce::SpinLock::ScopedTryLockType guard (engineLock);
        if (! guard.isLocked())
        {
            buffer.clear();
            return;
        }
        if (! prepared || patch == nullptr || buffer.getNumChannels() < 2)
        {
            pendingCount = 0;
            buffer.clear();
            return;
        }

        // Stable names are accessible from ordinary Vanilla [r] objects.
        // Send only changed values so control-rate Pd graphs are not flooded.
        x = juce::jlimit (0.0f, 1.0f, x);
        y = juce::jlimit (0.0f, 1.0f, y);
        const float touch = touching ? 1.0f : 0.0f;
        if (x != lastX) { libpd_float ("effects-x", x); lastX = x; }
        if (y != lastY) { libpd_float ("effects-y", y); lastY = y; }
        if (touch != lastTouch)
        {
            libpd_float ("effects-touch", touch);
            lastTouch = touch;
        }
        if (midiOverflow)
        {
            // A bounded queue overflow discards the burst and releases only
            // currently active notes, avoiding 2048 Pd calls on audio thread.
            pendingCount = 0;
            for (int ch = 0; ch < 16; ++ch)
                for (int note = 0; note < 128; ++note)
                    if (activeNotes[(size_t) ch][(size_t) note])
                    {
                        libpd_noteon (ch, note, 0);
                        activeNotes[(size_t) ch][(size_t) note] = false;
                    }
            midiOverflow = false;
        }

        const int samples = buffer.getNumSamples();
        int midiIndex = 0;
        for (int n = 0; n < samples; ++n)
        {
            // Events are in JUCE timestamp order. Pd runs on 64-frame ticks.
            while (midiIndex < pendingCount
                   && pendingMidi[(size_t) midiIndex].sample <= n)
                dispatchMidi (pendingMidi[(size_t) midiIndex++]);
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
        // A boundary-timestamped event applies on the next Pd tick.
        while (midiIndex < pendingCount)
            dispatchMidi (pendingMidi[(size_t) midiIndex++]);
        pendingCount = 0;
    }

private:
    struct MidiEvent
    {
        enum Kind : std::uint8_t { note, start, stop, resume, clock };
        Kind kind = note;
        int sample = 0;
        int channel = 0;
        int number = 0;
        int value = 0;
    };

    void dispatchMidi (const MidiEvent& event) noexcept
    {
        switch (event.kind)
        {
            case MidiEvent::note:
                libpd_noteon (event.channel, event.number, event.value);
                activeNotes[(size_t) event.channel][(size_t) event.number] =
                    event.value != 0;
                break;
            case MidiEvent::start:
                libpd_sysrealtime (0, 0xFA); // [realtimein]
                libpd_float ("effects-transport", 1.0f);
                libpd_bang ("effects-start");
                break;
            case MidiEvent::stop:
                libpd_sysrealtime (0, 0xFC);
                libpd_float ("effects-transport", 0.0f);
                libpd_bang ("effects-stop");
                break;
            case MidiEvent::resume:
                libpd_sysrealtime (0, 0xFB);
                libpd_float ("effects-transport", 1.0f);
                libpd_bang ("effects-continue");
                break;
            case MidiEvent::clock:
                libpd_sysrealtime (0, 0xF8);
                libpd_bang ("effects-clock");
                break;
        }
    }

    void clearFifo() noexcept
    {
        inputFifo.fill (0.0f);
        outputFifo.fill (0.0f);
        inputCursor = 0;
        outputCursor = tickFrames;
        pendingCount = 0;
        midiOverflow = false;
        lastX = lastY = lastTouch = -1.0f;
        for (auto& channel : activeNotes)
            channel.fill (false);
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
    std::array<MidiEvent, 512> pendingMidi {};
    std::array<std::array<bool, 128>, 16> activeNotes {};
    int pendingCount = 0;
    bool midiOverflow = false;
    float lastX = -1.0f, lastY = -1.0f, lastTouch = -1.0f;
    juce::File selectedFile;
    int inputCursor = 0;
    int outputCursor = tickFrames;
    void* patch = nullptr;
    bool prepared = false;
};
