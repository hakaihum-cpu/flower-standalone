#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
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

    // The real ULTRASHEER has no MIDI; CC 100-109 are EFFECTS-only.
    // Other mode numbers and thresholds follow Vongon user manuals.
    static bool acceptsCc (int mode, int number) noexcept
    {
        if (mode == ultrasheer) return number >= 100 && number <= 109;
        if (mode == rosetones)
            return number == 13 || number == 14
                || (number >= 80 && number <= 87) || number == 90;
        if (mode == solarfaze)
            return number == 13 || number == 14
                || (number >= 80 && number <= 98);
        if (mode == polyphrase)
            return (number >= 1 && number <= 11)
                || (number >= 13 && number <= 17);
        return false;
    }

    void handleMidi (const juce::MidiMessage& message, int mode,
                     int channel, int sampleOffset) noexcept
    {
        if (mode == polyphrase)
        {
            if (message.isMidiStart())
            {
                polyClockLast = -1.0;
                polyClockQuarterSeconds = 0.0;
            }
            if (message.isMidiClock())
            {
                const double now = midiSamplePosition
                    + (double) juce::jmax (0, sampleOffset);
                if (polyClockLast >= 0.0 && now > polyClockLast)
                {
                    const double quarter = (now - polyClockLast)
                        * 24.0 / sampleRate;
                    if (quarter >= 0.1 && quarter <= 2.0)
                        polyClockQuarterSeconds =
                            polyClockQuarterSeconds > 0.0
                                ? 0.8 * polyClockQuarterSeconds + 0.2 * quarter
                                : quarter;
                }
                polyClockLast = now;
            }
        }

        if (! message.isForChannel (channel)
            || mode < ultrasheer || mode > polyphrase)
            return;

        auto& bank = midiCc[(size_t) (mode - ultrasheer)];
        if (message.isController())
        {
            const int number = message.getControllerNumber();
            if (acceptsCc (mode, number))
            {
                bank[(size_t) number].store (
                    juce::jlimit (0, 127, message.getControllerValue()),
                    std::memory_order_relaxed);
                if (mode == polyphrase && number == 15)
                {
                    const double now = midiSamplePosition
                        + (double) juce::jmax (0, sampleOffset);
                    if (polyTapLast >= 0.0 && now > polyTapLast)
                    {
                        const double seconds = (now - polyTapLast) / sampleRate;
                        if (seconds >= 0.15 && seconds <= 2.0)
                            polyTapQuarterSeconds = seconds;
                    }
                    polyTapLast = now;
                }
            }
            return;
        }

        if (mode == rosetones && (message.isNoteOn() || message.isNoteOff()))
        {
            const int note = message.getNoteNumber();
            if (juce::isPositiveAndBelow (note, 128))
            {
                if (message.isNoteOn())
                    roseNotes[(size_t) note] =
                        (float) message.getVelocity() / 127.0f;
                else
                    roseNotes[(size_t) note] = 0.0f;
            }
            return;
        }

        if (mode != ultrasheer && message.isProgramChange())
        {
            // JUCE program numbers are zero-based: PC#1 => raw 0.
            const int pc = message.getProgramChangeNumber();
            auto& slots = midiPresets[(size_t) (mode - ultrasheer)];
            auto& valid = midiPresetValid[(size_t) (mode - ultrasheer)];
            if (pc >= 10 && pc <= 18)
            {
                const size_t slot = (size_t) (pc - 10);
                for (size_t i = 0; i < 128; ++i)
                    slots[slot][i].store (
                        bank[i].load (std::memory_order_relaxed),
                        std::memory_order_relaxed);
                valid[slot].store (true, std::memory_order_release);
            }
            else if (pc >= 0 && pc <= 8
                     && valid[(size_t) pc].load (std::memory_order_acquire))
            {
                for (size_t i = 0; i < 128; ++i)
                    bank[i].store (
                        slots[(size_t) pc][i].load (
                            std::memory_order_relaxed),
                        std::memory_order_relaxed);
            }
        }
    }

    VongonInspiredProcessor() noexcept
    {
        for (auto& bank : midiCc)
            for (auto& value : bank)
                value.store (-1, std::memory_order_relaxed);
    }

    // Serializable PC bank, exported only by the host state callback.
    // Atomic values avoid racing getStateInformation against MIDI/audio.
    juce::String exportMidiSnapshot() const
    {
        juce::StringArray parts;
        const auto encode = [&] (const auto& bank) -> juce::String
        {
            juce::String hex;
            for (const auto& cell : bank)
                hex += juce::String::toHexString (
                    juce::jlimit (0, 128,
                        cell.load (std::memory_order_relaxed) + 1))
                    .paddedLeft ('0', 2);
            return hex;
        };
        for (size_t mode = 0; mode < 4; ++mode)
        {
            parts.add (encode (midiCc[mode]));
            for (size_t slot = 0; slot < 9; ++slot)
                parts.add (midiPresetValid[mode][slot].load (
                    std::memory_order_relaxed)
                    ? encode (midiPresets[mode][slot]) : "-");
        }
        return parts.joinIntoString ("|");
    }

    void importMidiSnapshot (const juce::String& serialised)
    {
        const auto parts = juce::StringArray::fromTokens (
            serialised, "|", "");
        if (parts.size() != 40)
            return;
        const auto decode = [&] (const juce::String& hex, auto& bank)
        {
            if (hex.length() != 256)
                return false;
            for (int i = 0; i < 128; ++i)
            {
                const int raw = hex.substring (i * 2, i * 2 + 2)
                    .getHexValue32() - 1;
                if (raw < -1 || raw > 127)
                    return false;
            }
            for (int i = 0; i < 128; ++i)
            {
                const int raw = hex.substring (i * 2, i * 2 + 2)
                    .getHexValue32() - 1;
                bank[(size_t) i].store (
                    raw, std::memory_order_relaxed);
            }
            return true;
        };
        for (size_t mode = 0; mode < 4; ++mode)
        {
            decode (parts[(int) (mode * 10)], midiCc[mode]);
            for (size_t slot = 0; slot < 9; ++slot)
            {
                const auto& encoded = parts[(int) (mode * 10 + slot + 1)];
                const bool valid = encoded != "-"
                    && decode (encoded, midiPresets[mode][slot]);
                midiPresetValid[mode][slot].store (
                    valid, std::memory_order_relaxed);
            }
        }
    }

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
        for (int note = 0; note < 48; ++note)
        {
            auto& detector = roseDetectors[(size_t) note];
            detector = {};
            const double midiNote = (double) note + 36.0; // C2..B5
            const double frequency = 440.0
                * std::pow (2.0, (midiNote - 69.0) / 12.0);
            detector.coeff = 2.0f * std::sin (
                juce::MathConstants<float>::pi
                * (float) frequency / (float) sampleRate);
        }

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
        polySamplesWritten = 0;
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
        for (auto& detector : roseDetectors)
        {
            detector.low = 0.0f;
            detector.band = 0.0f;
            detector.envelope = 0.0f;
        }

        solarLow = { 0.0f, 0.0f };
        solarUpper = { 0.0f, 0.0f };
        solarDelay.clear();
        solarWrite = 0;

        // Logical reset: silence unwritten samples without clearing 22.5s
        // of audio memory during the time-critical callback.
        polySamplesWritten = 0;
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
        midiSamplePosition += buffer.getNumSamples();

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
    int ccRaw (int mode, int number) const noexcept
    {
        return midiCc[(size_t) (mode - ultrasheer)][(size_t) number]
            .load (std::memory_order_relaxed);
    }

    float ccValue (int mode, int number, float fallback) const noexcept
    {
        const int val = ccRaw (mode, number);
        return val < 0 ? fallback : (float) val / 127.0f;
    }

    bool ccOn (int mode, int number, bool fallback = false) const noexcept
    {
        const int val = ccRaw (mode, number);
        return val < 0 ? fallback : val >= 64;
    }

    struct ResonatorState
    {
        float y1 = 0.0f;
        float y2 = 0.0f;
    };

    struct RoseDetectorState
    {
        float low = 0.0f;
        float band = 0.0f;
        float envelope = 0.0f;
        float coeff = 0.0f;
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
        const float depth = ccValue (ultrasheer, 100, x);
        const float rate = ccValue (ultrasheer, 101, x);
        const float depthMs = 0.12f + 3.0f * depth * depth;
        const float rateHz = 0.18f + 3.2f * rate;
        const float randomBlend = ccRaw (ultrasheer, 102) >= 0
            ? (ccOn (ultrasheer, 102) ? 1.0f : 0.0f)
            : juce::jlimit (0.0f, 1.0f, (x - 0.55f) / 0.45f);
        const float wet = ccValue (ultrasheer, 104, 0.12f + 0.66f * y);
        const bool vibBypass = ccOn (ultrasheer, 106);
        const bool revBypass = ccOn (ultrasheer, 107);
        const bool reverbFirst = ccOn (ultrasheer, 108);
        if (revBypass && ! ccOn (ultrasheer, 109, true))
            ultraReverb.reset();
        const float baseMs = 7.0f;

        juce::Reverb::Parameters rev;
        rev.roomSize = 0.35f + 0.62f * ccValue (ultrasheer, 105, y);
        rev.damping = ccRaw (ultrasheer, 103) < 0
            ? 0.72f - 0.46f * y
            : 0.85f - 0.75f * ccValue (ultrasheer, 103, y);
        rev.wetLevel = revBypass ? 0.0f : wet;
        rev.dryLevel = 1.0f - 0.42f * (revBypass ? 0.0f : wet);
        rev.width = 1.0f;
        rev.freezeMode = 0.0f;
        ultraReverb.setParameters (rev);


        const auto applyReverb = [&]() noexcept
        {
            if (channels >= 2)
                ultraReverb.processStereo (
                    buffer.getWritePointer (0),
                    buffer.getWritePointer (1), samples);
            else
                ultraReverb.processMono (
                    buffer.getWritePointer (0), samples);
        };
        if (reverbFirst)
            applyReverb();

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
                    dry + (vib - dry) * (vibBypass ? 0.0f : 0.12f + 0.55f * depth));
            }

            ultraWrite = (ultraWrite + 1) % ultraDelay.getNumSamples();
        }

        if (! reverbFirst)
            applyReverb();

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
        juce::AudioBuffer<float>& buffer, float x, float y) noexcept
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        const int samples = buffer.getNumSamples();
        if (channels <= 0 || samples <= 0)
            return;
        if (ccOn (rosetones, 13))
            return;

        // Manual architecture: 48 chromatic input filters C2..B5, folded
        // by pitch class into 12 separately excited tuned resonators.
        // OCTAVE revoices the outputs; it does not change pitch detection.
        const float octave = ccRaw (rosetones, 87) < 0
            ? 3.0f + std::floor (x * 3.999f)
            : 3.0f + 3.0f * ccValue (rosetones, 87, x);
        const float glide = ccValue (rosetones, 83, 1.0f);
        const float glideAlpha = ccRaw (rosetones, 83) < 0
            ? 1.0f : 1.0f - std::exp (
                -(float) samples / juce::jmax (1.0f,
                    (float) sampleRate * (0.012f + 0.42f * (1.0f - glide))));
        roseOctaveSmoothed += glideAlpha * (octave - roseOctaveSmoothed);

        const float sens = ccValue (rosetones, 85, y);
        const float inputGain = 1.0f + 5.0f * sens;
        const float feedback = ccValue (rosetones, 86, y);
        const float radius = juce::jlimit (
            0.91f, 0.9997f, 0.91f + 0.0894f * feedback);
        const float mix = ccValue (rosetones, 80, y);
        const float tone = ccValue (rosetones, 82, x);
        const float toneCoeff = onePoleCoeff (
            sampleRate, 350.0f + 12000.0f * tone * tone);
        const float synthLevel = 2.0f * ccValue (rosetones, 81, 0.5f);
        const float modulation = ccValue (rosetones, 84, 0.4f * x * y);
        const int mode = ccRaw (rosetones, 14) < 0 ? 2
            : ccRaw (rosetones, 14) < 43 ? 0
            : ccRaw (rosetones, 14) < 86 ? 1 : 2;
        const int interval = ccRaw (rosetones, 90) < 0 ? 0
            : juce::jmin (11, ccRaw (rosetones, 90) * 12 / 128);

        // High-cost pitch math once per audio buffer, not per sample.
        float a1[2][12] {};
        float panning[2][12] {};
        const double phase = rosePhase;
        for (int note = 0; note < 12; ++note)
        {
            const double midi = 12.0 * (double) roseOctaveSmoothed
                + (double) note + (double) interval;
            const double baseHz = 440.0
                * std::pow (2.0, (midi - 69.0) / 12.0);
            const float vibrato = (mode == 1 || mode == 2)
                ? 0.18f * modulation * std::sin (
                    (float) phase + (float) note * 0.52f) : 0.0f;
            for (int ch = 0; ch < channels; ++ch)
            {
                const float chorus = (mode == 0 || mode == 2)
                    ? (ch == 0 ? -0.14f : 0.14f) * modulation : 0.0f;
                const double frequency = baseHz
                    * std::pow (2.0, (double) (vibrato + chorus) / 12.0);
                const double omega = juce::MathConstants<double>::twoPi
                    * juce::jmin (frequency, sampleRate * 0.42) / sampleRate;
                a1[ch][note] = 2.0f * radius * (float) std::cos (omega);
                const float pan = (float) note / 11.0f;
                panning[ch][note] = channels == 1 ? 1.0f
                    : (ch == 0 ? 1.0f - 0.78f * pan
                               : 0.22f + 0.78f * pan);
            }
        }
        rosePhase += juce::MathConstants<double>::twoPi
            * (0.09 + 0.2 * (double) modulation)
            * (double) samples / sampleRate;
        if (rosePhase >= juce::MathConstants<double>::twoPi)
            rosePhase = std::fmod (
                rosePhase, juce::MathConstants<double>::twoPi);

        std::array<float, 12> midiGates {};
        for (int midiNote = 0; midiNote < 128; ++midiNote)
            midiGates[(size_t) (midiNote % 12)] = juce::jmax (
                midiGates[(size_t) (midiNote % 12)],
                roseNotes[(size_t) midiNote]);

        const float attack = onePoleCoeff (sampleRate, 85.0f);
        const float release = onePoleCoeff (sampleRate, 12.0f);
        for (int i = 0; i < samples; ++i)
        {
            const float dryL = buffer.getSample (0, i);
            const float dryR = channels > 1 ? buffer.getSample (1, i) : dryL;
            const float mono = std::tanh (
                0.5f * (dryL + dryR) * inputGain);
            std::array<float, 12> pitchEnergy {};
            for (int bandIndex = 0; bandIndex < 48; ++bandIndex)
            {
                auto& detector = roseDetectors[(size_t) bandIndex];
                const float high = mono - detector.low
                    - 0.11f * detector.band;
                detector.band += detector.coeff * high;
                detector.low += detector.coeff * detector.band;
                const float energy = std::abs (detector.band);
                detector.envelope +=
                    (energy > detector.envelope ? attack : release)
                    * (energy - detector.envelope);
                pitchEnergy[(size_t) (bandIndex % 12)] +=
                    detector.envelope;
            }

            float wet[2] { 0.0f, 0.0f };
            for (int note = 0; note < 12; ++note)
            {
                const float excitation = juce::jlimit (
                    0.0f, 1.0f,
                    pitchEnergy[(size_t) note] * 0.6f
                        + midiGates[(size_t) note] * 0.16f);
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto& state = roseState[(size_t) ch][(size_t) note];
                    const float next = excitation * 0.0025f
                        + a1[ch][note] * state.y1
                        - radius * radius * state.y2;
                    state.y2 = state.y1;
                    state.y1 = protect (next);
                    wet[ch] += state.y1 * panning[ch][note];
                }
            }

            const float dry[2] { dryL, dryR };
            for (int ch = 0; ch < channels; ++ch)
            {
                const float synth = std::tanh (
                    wet[ch] * 0.3f * synthLevel);
                roseToneLow[(size_t) ch] +=
                    toneCoeff * (synth - roseToneLow[(size_t) ch]);
                const float out = dry[ch] * (1.0f - mix)
                    + roseToneLow[(size_t) ch] * mix;
                buffer.setSample (ch, i, protect (out));
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
        const bool satOn = ! ccOn (solarfaze, 14);
        const bool detOn = ! ccOn (solarfaze, 13);
        const bool satFirst = ccOn (solarfaze, 97, true);
        const float midFrequency = ccValue (solarfaze, 94, 0.5f);
        const float crossoverScale = 0.75f + 0.5f * midFrequency;
        const float lowCoeff = onePoleCoeff (
            sampleRate, (90.0f + 450.0f
                * ccValue (solarfaze, 93, 0.3333333f))
                * crossoverScale);
        const float upperCoeff = onePoleCoeff (
            sampleRate, (1500.0f + 3100.0f
                * ccValue (solarfaze, 95, 0.3548387f))
                * crossoverScale);
        const float driveMaster = 0.5f + 2.0f
            * ccValue (solarfaze, 88, 0.25f);
        const float driveLow = (1.15f + 3.0f * x) * driveMaster
            * (1.0f + (float) (ccRaw (solarfaze, 89) < 0 ? 0
                : ccRaw (solarfaze, 89) < 43 ? 0
                : ccRaw (solarfaze, 89) < 86 ? 1 : 3));
        const float driveMid = (1.10f + 4.2f * x) * driveMaster
            * (1.0f + (float) (ccRaw (solarfaze, 90) < 0 ? 0
                : ccRaw (solarfaze, 90) < 43 ? 0
                : ccRaw (solarfaze, 90) < 86 ? 1 : 3));
        const float driveHigh = (1.05f + 2.4f * x) * driveMaster
            * (1.0f + (float) (ccRaw (solarfaze, 91) < 0 ? 0
                : ccRaw (solarfaze, 91) < 43 ? 0
                : ccRaw (solarfaze, 91) < 86 ? 1 : 3));
        const float detAmount = ccValue (solarfaze, 81, y);
        const float detuneDepthMs = 0.18f + 2.4f * detAmount;
        const float detEnv = 2.0f * ccValue (solarfaze, 82, 0.5f) - 1.0f;
        const float detSize = ccValue (solarfaze, 96, 0.3095238f);
        const float mix = ccValue (solarfaze, 80, 0.16f + 0.68f * y);
        const float satMix = ccValue (solarfaze, 86, 1.0f);
        const float satTone = ccValue (solarfaze, 87, 0.5f);
        const float bandLow = ccValue (solarfaze, 83, 0.95f);
        const float bandMid = ccValue (solarfaze, 84, 0.78f);
        const float bandHigh = ccValue (solarfaze, 85, 0.8266667f);
        const float gateThreshold = 0.0002f + 0.025f
            * ccValue (solarfaze, 98, 0.056f);
        const bool gateEnabled = ! ccOn (solarfaze, 92);

        // Implement CC97 signal order: SAT->DETUNE or DETUNE->SAT.
        const auto saturateBands = [&] (int ch, float signal) noexcept
        {
            solarLow[(size_t) ch] += lowCoeff * (signal - solarLow[(size_t) ch]);
            solarUpper[(size_t) ch] +=
                upperCoeff * (signal - solarUpper[(size_t) ch]);
            const float low = solarLow[(size_t) ch];
            const float high = signal - solarUpper[(size_t) ch];
            const float mid = signal - low - high;
            const float gate = ! gateEnabled
                || solarGateEnvelope > gateThreshold ? 1.0f : 0.0f;
            return gate * (
                std::tanh (low * driveLow) * bandLow
                + std::tanh (mid * driveMid) * bandMid
                + std::tanh (high * driveHigh)
                    * bandHigh * (0.25f + satTone));
        };
        for (int i = 0; i < samples; ++i)
        {
            float dry[2] { 0.0f, 0.0f };
            float preDetune[2] { 0.0f, 0.0f };
            for (int ch = 0; ch < channels; ++ch)
            {
                dry[ch] = buffer.getSample (ch, i);
                solarGateEnvelope +=
                    0.0025f * (std::abs (dry[ch]) - solarGateEnvelope);
                preDetune[ch] = satFirst && satOn
                    ? dry[ch] + (saturateBands (ch, dry[ch]) - dry[ch]) * satMix
                    : dry[ch];
                solarDelay.setSample (
                    ch, solarWrite, protect (preDetune[ch]));
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
                const float envShape = juce::jlimit (
                    0.1f, 2.0f,
                    (0.35f + 0.65f * envMod)
                        * (1.0f + detEnv * (2.0f * envMod - 1.0f)));
                const double delaySamples = sampleRate
                    * (1.8f + 4.2f * detSize
                        + detuneDepthMs * envShape * lfo * polarity)
                    / 1000.0;
                const float detuned = readDelay (
                    solarDelay, ch, solarWrite, delaySamples);
                const float detResult = detOn
                    ? preDetune[ch] + (detuned - preDetune[ch]) * mix
                    : preDetune[ch];
                const float result = ! satFirst && satOn
                    ? detResult
                        + (saturateBands (ch, detResult) - detResult) * satMix
                    : detResult;
                buffer.setSample (ch, i, protect (result));
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

    float readPolyDelay (int channel, double samples) const noexcept
    {
        if (samples > (double) polySamplesWritten - 2.0)
            return 0.0f;
        return readDelay (polyDelay, channel, polyWrite, samples);
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

        // Nine discrete time bands match CC#8 in the v2.3 manual.
        static constexpr double secondsTable[9] = {
            0.085, 0.170, 0.341, 0.682, 1.3, 2.7, 5.5, 11.0, 22.0
        };
        const int timeCc = ccRaw (polyphrase, 8);
        const int timeIndex = timeCc < 0 ? -1
            : timeCc <= 8 ? 0
            : timeCc <= 22 ? 1 : timeCc <= 37 ? 2
            : timeCc <= 56 ? 3 : timeCc <= 74 ? 4
            : timeCc <= 91 ? 5 : timeCc <= 106 ? 6
            : timeCc <= 120 ? 7 : 8;
        const double timeSeconds = timeIndex < 0
            ? 0.085 * std::pow (22.0 / 0.085, (double) x)
            : secondsTable[timeIndex];
        const bool clockEnabled = ccRaw (polyphrase, 17) < 64;
        const bool recentClock = polyClockLast >= 0.0
            && midiSamplePosition - polyClockLast < sampleRate * 3.0;
        const double seconds = clockEnabled && recentClock
            && polyClockQuarterSeconds > 0.0
            ? polyClockQuarterSeconds * (timeIndex < 0
                ? 1.0 : std::pow (2.0, (double) timeIndex - 4.0))
            : (polyTapQuarterSeconds > 0.0 && timeIndex < 0
                ? polyTapQuarterSeconds : timeSeconds);
        const bool sync = ccOn (polyphrase, 10);
        const double sliderLeft =
            0.05 + 0.95 * ccValue (polyphrase, 5, 1.0f);
        const double sliderRight = sync ? sliderLeft
            : 0.05 + 0.95 * ccValue (
                polyphrase, 6, 0.557894737f + 0.4f * y);
        updatePolyTarget (0, seconds * sliderLeft * sampleRate);
        updatePolyTarget (1, seconds * sliderRight * sampleRate);

        const float repeats = ccValue (polyphrase, 1, y);
        const float feedback = juce::jlimit (
            0.0f, 1.08f, 0.12f + 0.94f * repeats);
        const bool bypass = ccOn (polyphrase, 13);
        const bool trails = ccOn (polyphrase, 16);
        const bool infinite = ccOn (polyphrase, 14);
        const float mix = bypass ? 0.0f
            : ccValue (polyphrase, 7, 0.12f + 0.68f * y);
        const float tone = ccValue (polyphrase, 2, 0.08f + 0.82f * (1.0f - y));
        const float toneCoeff = onePoleCoeff (
            sampleRate, 650.0f + 12500.0f * tone * tone);
        const float modDepthSamples = (float) sampleRate
            * (0.00015f + 0.0018f * ccValue (polyphrase, 3, y));
        const float modRate = 0.05f + 0.33f * ccValue (polyphrase, 4, x);

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
                    const float oldRead = readPolyDelay (
                        ch, polyOldDelay[(size_t) ch] + mod * modDepthSamples);
                    const float newRead = readPolyDelay (
                        ch, polyTargetDelay[(size_t) ch] + mod * modDepthSamples);
                    delayed[ch] =
                        oldRead + (newRead - oldRead) * t;

                    if (--polyCrossfadeRemaining[(size_t) ch] <= 0)
                        polyCurrentDelay[(size_t) ch] =
                            polyTargetDelay[(size_t) ch];
                }
                else
                {
                    delayed[ch] = readPolyDelay (
                        ch, readDelaySamples + mod * modDepthSamples);
                }

                polyLowpass[(size_t) ch] +=
                    toneCoeff
                    * (delayed[ch] - polyLowpass[(size_t) ch]);
            }

            // Middle/right XY area progressively becomes ping-pong.
            const int style = ccRaw (polyphrase, 9);
            const float pingPong = style < 0
                ? juce::jlimit (0.0f, 1.0f, (x - 0.35f) / 0.45f)
                : (style < 44 ? 0.0f : style < 88 ? 1.0f : 0.5f);
            for (int ch = 0; ch < channels; ++ch)
            {
                const int other = channels > 1 ? 1 - ch : ch;
                const float fbSignal =
                    polyLowpass[(size_t) ch] * (1.0f - pingPong)
                    + polyLowpass[(size_t) other] * pingPong;
                const float infiniteBoost = infinite
                    || (ccOn (polyphrase, 11, true)
                        && touchDown && y > 0.93f)
                    ? 1.04f : feedback;
                const float incoming = bypass ? 0.0f : dry[ch];
                const float write = bypass && ! trails
                    ? 0.0f
                    : protect (incoming + fbSignal * infiniteBoost);
                polyDelay.setSample (ch, polyWrite, write);
                buffer.setSample (
                    ch, i,
                    protect (
                        dry[ch]
                        + (delayed[ch] - dry[ch]) * mix));
            }

            polyWrite = (polyWrite + 1) % polyDelay.getNumSamples();
            polySamplesWritten = juce::jmin (
                polyDelay.getNumSamples(), polySamplesWritten + 1);
        }
    }

    // In-memory, per-mode MIDI CC and nine PC snapshot banks.
    // Values of -1 mean XY retains authority until the first CC for that control.
    std::array<std::array<std::atomic<int>, 128>, 4> midiCc {};
    std::array<std::array<std::array<std::atomic<int>, 128>, 9>, 4>
        midiPresets {};
    std::array<std::array<std::atomic<bool>, 9>, 4> midiPresetValid {};
    std::array<float, 128> roseNotes {};
    float roseOctaveSmoothed = 4.0f;
    double midiSamplePosition = 0.0;
    double polyClockLast = -1.0;
    double polyClockQuarterSeconds = 0.0;
    double polyTapLast = -1.0;
    double polyTapQuarterSeconds = 0.0;

    double sampleRate = 48000.0;

    juce::AudioBuffer<float> ultraDelay;
    int ultraWrite = 0;
    double ultraPhase = 0.0;
    uint32_t ultraRandomState = 0x554c5452u;
    float ultraRandom = 0.0f;
    float ultraRandomTarget = 0.0f;
    int ultraRandomSamples = 1;
    juce::Reverb ultraReverb;

    std::array<RoseDetectorState, 48> roseDetectors {};
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
    int polySamplesWritten = 0;
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
