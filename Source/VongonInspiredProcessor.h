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

    void requestPolyTrigger() noexcept
    {
        polyTriggerRequested.store (true, std::memory_order_release);
    }

    void setDirectCc (int mode, int number, int value) noexcept
    {
        if (acceptsCc (mode, number))
            midiCc[(size_t) (mode - ultrasheer)][(size_t) number]
                .store (juce::jlimit (0, 127, value),
                        std::memory_order_relaxed);
    }

    int getDirectCc (int mode, int number) const noexcept
    {
        if (! acceptsCc (mode, number)) return -1;
        return midiCc[(size_t) (mode - ultrasheer)][(size_t) number]
            .load (std::memory_order_relaxed);
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
        const double diffusionMs[4] { 4.77, 3.58, 12.73, 9.30 };
        for (int i = 0; i < 4; ++i)
            ultraInputDiff[(size_t) i].prepare (
                sampleRate, diffusionMs[i] * 0.001);
        ultraTankDiff[0].prepare (sampleRate, 0.0205);
        ultraTankDiff[1].prepare (sampleRate, 0.0268);
        ultraTank[0].prepare (sampleRate, 0.1496);
        ultraTank[1].prepare (sampleRate, 0.1417);
        ultraFeedback = { 0.0f, 0.0f };
        ultraDamped = { 0.0f, 0.0f };
        ultraWetSmoothed = 0.0f;
        ultraVibSmoothed = 0.0f;
        ultraDepthSmoothed = 0.0f;

        for (auto& channel : roseState)
            for (auto& resonator : channel)
            {
                resonator = {};
                resonator.line.assign (2048, 0.0f);
            }
        roseToneLow = { 0.0f, 0.0f };
        roseMixSmoothed = 0.5f;
        roseFeedbackSmoothed = 0.7f;
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
            2, juce::jmax (256, juce::roundToInt (sampleRate * 0.250)),
            false, true, false);
        solarDelay.clear();
        solarWrite = 0;
        solarPhase = 0.0;
        solarPitchPhase = { 0.1, 0.6 };
        solarRatioRefresh = 0;
        solarDetuneSmoothed = 0.0f;
        solarSatMixSmoothed = 0.0f;
        solarDetMixSmoothed = 0.0f;
        solarSizeSmoothed = 0.5f;
        solarRatio = { 1.0, 1.0 };
        solarRatioRefresh = 0;
        solarGateGain = 1.0f;
        solarToneLow = { 0.0f, 0.0f };
        for (auto& channel : solarBands)
            for (auto& band : channel) band = {};

        const int polyCapacity =
            juce::jmax (4096, juce::roundToInt (sampleRate * 22.5));
        polyDelay.setSize (2, polyCapacity, false, true, false);
        polyDelay.clear();
        polyWrite = 0;
        polyLowpass = { 0.0f, 0.0f };
        polyToneOut = { 0.0f, 0.0f };
        polyWetSmoothed = 0.0f;
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
        for (auto& stage : ultraInputDiff) stage.reset();
        for (auto& stage : ultraTankDiff) stage.reset();
        for (auto& stage : ultraTank) stage.reset();
        ultraFeedback = { 0.0f, 0.0f };
        ultraDamped = { 0.0f, 0.0f };
        ultraWetSmoothed = 0.0f;
        ultraVibSmoothed = 0.0f;
        ultraDepthSmoothed = 0.0f;

        for (auto& channel : roseState)
            for (auto& resonator : channel)
            {
                std::fill (
                    resonator.line.begin(), resonator.line.end(), 0.0f);
                resonator.cursor = 0;
                resonator.delaySmoothed = 0.0f;
            }
        roseToneLow = { 0.0f, 0.0f };
        roseMixSmoothed = 0.5f;
        roseFeedbackSmoothed = 0.7f;
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
        solarPitchPhase = { 0.1, 0.6 };
        solarRatioRefresh = 0;
        solarDetuneSmoothed = 0.0f;
        solarSatMixSmoothed = 0.0f;
        solarDetMixSmoothed = 0.0f;
        solarGateGain = 1.0f;
        solarToneLow = { 0.0f, 0.0f };
        for (auto& channel : solarBands)
            for (auto& band : channel) band = {};

        // Logical reset: silence unwritten samples without clearing 22.5s
        // of audio memory during the time-critical callback.
        polySamplesWritten = 0;
        polyWrite = 0;
        polyLowpass = { 0.0f, 0.0f };
        polyToneOut = { 0.0f, 0.0f };
        polyWetSmoothed = 0.0f;
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
        if (mode == polyphrase && polyTriggerRequested.exchange (
                false, std::memory_order_acq_rel))
        {
            if (ccOn (polyphrase, 11))
                setDirectCc (polyphrase, 14,
                    ccOn (polyphrase, 14) ? 0 : 127);
            else
            {
                const double now = midiSamplePosition;
                if (polyTapLast >= 0.0 && now > polyTapLast)
                {
                    const double dt = (now - polyTapLast) / sampleRate;
                    if (dt >= 0.15 && dt <= 2.0)
                        polyTapQuarterSeconds = dt;
                }
                polyTapLast = now;
            }
        }

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

    struct PlateStage
    {
        std::vector<float> buffer;
        int cursor = 0;
        void prepare (double rate, double seconds)
        {
            buffer.assign ((size_t) juce::jmax (
                2, juce::roundToInt (rate * seconds)), 0.0f);
            cursor = 0;
        }
        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            cursor = 0;
        }
        float tick (float input) noexcept
        {
            const float delayed = buffer[(size_t) cursor];
            buffer[(size_t) cursor] = input;
            if (++cursor == (int) buffer.size()) cursor = 0;
            return delayed;
        }
        float allpass (float input, float gain) noexcept
        {
            const float delayed = buffer[(size_t) cursor];
            const float output = delayed - gain * input;
            buffer[(size_t) cursor] = input + gain * output;
            if (++cursor == (int) buffer.size()) cursor = 0;
            return output;
        }
    };

    struct ResonatorState
    {
        // Tuned feedback delay line, distinct from the 48 detection filters.
        std::vector<float> line;
        int cursor = 0;
        float delaySmoothed = 0.0f;

        float tick (float drive, float samples, float feedback) noexcept
        {
            if (line.empty()) return 0.0f;
            const int count = (int) line.size();
            samples = juce::jlimit (
                2.0f, (float) (count - 2), samples);
            float pos = (float) cursor - samples;
            if (pos < 0.0f) pos += (float) count;
            const int i0 = (int) pos;
            const int i1 = (i0 + 1) % count;
            const float frac = pos - (float) i0;
            const float delayed = line[(size_t) i0]
                + (line[(size_t) i1] - line[(size_t) i0]) * frac;
            // Nonlinear soft limiter in the resonator feedback path.
            const float next = drive + delayed * feedback;
            line[(size_t) cursor] = next / (1.0f + std::abs (next));
            if (++cursor == count) cursor = 0;
            return delayed;
        }
    };

    struct SolarBand
    {
        float b0 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        void tune (double rate, float freq) noexcept
        {
            const double omega = juce::MathConstants<double>::twoPi
                * (double) juce::jlimit (
                    20.0f, (float) rate * 0.44f, freq) / rate;
            const double alpha = std::sin (omega) / 1.5;
            const double normaliser = 1.0 / (1.0 + alpha);
            b0 = (float) (alpha * normaliser);
            b2 = -b0;
            a1 = (float) (-2.0 * std::cos (omega) * normaliser);
            a2 = (float) ((1.0 - alpha) * normaliser);
        }
        float process (float input) noexcept
        {
            const float result = b0 * input + b2 * x2
                - a1 * y1 - a2 * y2;
            x2 = x1; x1 = input;
            y2 = y1; y1 = result;
            return result;
        }
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
        const float depthMs = 4.5f * depth;
        const float rateHz = 0.75f + 8.25f * rate;
        const float randomBlend = ccOn (ultrasheer, 102, x > 0.6f)
            ? 1.0f : 0.0f;
        const float brightness = ccValue (ultrasheer, 103, y);
        const float mix = ccValue (ultrasheer, 104, y);
        const float decay = 0.25f * std::pow (
            120.0f, ccValue (ultrasheer, 105, y));
        const bool vibBypass = ccOn (ultrasheer, 106);
        const bool revBypass = ccOn (ultrasheer, 107);
        const bool reverbFirst = ccOn (ultrasheer, 108);
        const bool trails = ccOn (ultrasheer, 109, true);
        const float decayFeedback = revBypass && ! trails
            ? 0.0f : std::pow (0.001f, 0.146f / decay);
        const float vibratoAlpha = onePoleCoeff (sampleRate, 15.0f);
        const float damping = onePoleCoeff (
            sampleRate, 650.0f + 15500.0f * brightness * brightness);
        // Independent effect level: no re-injection of original input.
        const float wetMix = mix;
        const float wetAlpha = onePoleCoeff (sampleRate, 15.0f);
        const float baseMs = 7.0f;
        const auto applyReverb = [&]() noexcept
        {
            for (int sample = 0; sample < samples; ++sample)
            {
                const float left = buffer.getSample (0, sample);
                const float right = channels > 1
                    ? buffer.getSample (1, sample) : left;
                float signal = revBypass ? 0.0f : (left + right) * 0.5f;
                signal = ultraInputDiff[0].allpass (signal, 0.75f);
                signal = ultraInputDiff[1].allpass (signal, 0.75f);
                signal = ultraInputDiff[2].allpass (signal, 0.625f);
                signal = ultraInputDiff[3].allpass (signal, 0.625f);
                float tanks[2] {};
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float feedback = signal + decayFeedback
                        * ultraFeedback[(size_t) (1 - ch)];
                    const float spread = ultraTankDiff[(size_t) ch]
                        .allpass (feedback, 0.55f);
                    const float delayed = ultraTank[(size_t) ch].tick (spread);
                    ultraDamped[(size_t) ch] += damping
                        * (delayed - ultraDamped[(size_t) ch]);
                    tanks[ch] = protect (ultraDamped[(size_t) ch]);
                }
                ultraFeedback = { tanks[0], tanks[1] };
                const float targetWet = revBypass && ! trails
                    ? 0.0f : wetMix;
                ultraWetSmoothed += wetAlpha
                    * (targetWet - ultraWetSmoothed);
                // If Reverb is bypassed, allow the *other* effect
                // (Vibrato) through; both bypassed means silence.
                const bool vibratoActive = ! vibBypass && depth > 0.0001f;
                const float serialPass = revBypass && vibratoActive
                    ? 1.0f : 0.0f;
                buffer.setSample (0, sample, protect (
                    left * serialPass + ultraWetSmoothed
                        * (0.8f * tanks[0] + 0.2f * tanks[1])));
                if (channels > 1)
                    buffer.setSample (1, sample, protect (
                        right * serialPass + ultraWetSmoothed
                            * (0.2f * tanks[0] + 0.8f * tanks[1])));
            }
        };
        if (reverbFirst) applyReverb();

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
            ultraDepthSmoothed += vibratoAlpha
                * (depth - ultraDepthSmoothed);
            ultraVibSmoothed += vibratoAlpha
                * ((vibBypass || depth < 0.0001f ? 0.0f : 1.0f)
                    - ultraVibSmoothed);

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
                if (ultraVibSmoothed > 0.0001f)
                {
                    // Preserve stereo by modulating L/R with the same waveform.
                    const double delaySamples = sampleRate
                        * (baseMs + 4.5f * ultraDepthSmoothed * mod) / 1000.0;
                    const float shifted = readDelay (
                        ultraDelay, ch, ultraWrite, delaySamples);
                    buffer.setSample (ch, i, dry
                        + (shifted - dry) * ultraVibSmoothed);
                }
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
        const float slider = ccValue (rosetones, 87, x);
        const float octave = 2.0f
            + (float) juce::roundToInt (slider * 36.0f) / 12.0f;
        const float glide = ccValue (rosetones, 83, 1.0f);
        const float glideAlpha = ccRaw (rosetones, 83) < 0
            ? 1.0f : 1.0f - std::exp (
                -(float) samples / juce::jmax (1.0f,
                    (float) sampleRate * (0.012f + 0.42f * (1.0f - glide))));
        roseOctaveSmoothed += glideAlpha * (octave - roseOctaveSmoothed);

        const float sens = ccValue (rosetones, 85, y);
        const float inputGain = 5.0f * sens;
        const float feedback = ccValue (rosetones, 86, y);
        // Feedback occurs once per resonator delay circuit, NOT once
        // per audio sample. Previous 0.3-0.65 default damped almost all
        // the synthesized signal before the user could hear it.
        const float feedbackGain = juce::jlimit (
            0.94f, 0.9995f, 0.94f + feedback * 0.0595f);
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
        float delayLength[2][12] {};
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
                auto& voice = roseState[(size_t) ch][(size_t) note];
                const float desiredDelay = (float) (
                    sampleRate / juce::jmax (30.0, frequency));
                if (voice.delaySmoothed <= 0.0f)
                    voice.delaySmoothed = desiredDelay;
                // Smooth octave/glide and chorus updates between buffers.
                voice.delaySmoothed += 0.12f
                    * (desiredDelay - voice.delaySmoothed);
                delayLength[ch][note] = voice.delaySmoothed;
                const float pan = (float) note / 11.0f;
                const float spread = (mode == 0 || mode == 2)
                    ? 0.35f : 0.78f;
                panning[ch][note] = channels == 1 ? 1.0f
                    : ch == 0 ? 1.0f - spread * pan
                              : 1.0f - spread * (1.0f - pan);
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
        const float mixSmooth = onePoleCoeff (sampleRate, 18.0f);
        for (int i = 0; i < samples; ++i)
        {
            roseMixSmoothed += mixSmooth * (mix - roseMixSmoothed);
            roseFeedbackSmoothed += mixSmooth
                * (feedbackGain - roseFeedbackSmoothed);
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
                        + midiGates[(size_t) note] * 0.32f);
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto& state = roseState[(size_t) ch][(size_t) note];
                    const float voice = state.tick (
                        excitation * 0.13f,
                        delayLength[ch][note], roseFeedbackSmoothed);
                    wet[ch] += voice * panning[ch][note];
                }
            }

            const float dry[2] { dryL, dryR };
            for (int ch = 0; ch < channels; ++ch)
            {
                const float synth = std::tanh (
                    wet[ch] * 1.25f * synthLevel);
                roseToneLow[(size_t) ch] +=
                    toneCoeff * (synth - roseToneLow[(size_t) ch]);
                // Resonator output only. MIX is resonator output level.
                const float out = roseToneLow[(size_t) ch]
                    * roseMixSmoothed;
                buffer.setSample (ch, i, protect (out));
            }
        }
    }

    void processSolarfaze (
        juce::AudioBuffer<float>& buffer, float x, float y) noexcept
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        const int count = buffer.getNumSamples();
        if (channels <= 0 || count <= 0) return;

        const bool detOn = ! ccOn (solarfaze, 13);
        const bool satOn = ! ccOn (solarfaze, 14);
        const bool satFirst = ccOn (solarfaze, 97, true);
        const bool gateOn = ! ccOn (solarfaze, 92);
        const float detuneMix = ccValue (solarfaze, 80, y);
        const float detuneAmount = ccValue (solarfaze, 81, y);
        const float detuneEnv = 2.0f * ccValue (solarfaze, 82, 0.5f) - 1.0f;
        const float detuneSize = ccValue (solarfaze, 96, 0.5f);
        const float satMix = ccValue (solarfaze, 86, x);
        const float satTone = ccValue (solarfaze, 87, 0.5f);
        const float mainDrive = 0.8f
            + 4.5f * ccValue (solarfaze, 88, x);
        const float levels[3] {
            ccValue (solarfaze, 83, 0.7f),
            ccValue (solarfaze, 84, 0.7f),
            ccValue (solarfaze, 85, 0.7f)
        };
        const float bandDrive[3] {
            ccRaw (solarfaze, 89) < 43 ? 1.0f
                : ccRaw (solarfaze, 89) < 86 ? 2.0f : 4.0f,
            ccRaw (solarfaze, 90) < 43 ? 1.0f
                : ccRaw (solarfaze, 90) < 86 ? 2.0f : 4.0f,
            ccRaw (solarfaze, 91) < 43 ? 1.0f
                : ccRaw (solarfaze, 91) < 86 ? 2.0f : 4.0f
        };

        // Independent centre-frequency controls for all three bands.
        const float hz[3] {
            20.0f * std::pow (22.0f,
                ccValue (solarfaze, 93, 0.523f)), // 100Hz default
            150.0f * std::pow (16.0f,
                ccValue (solarfaze, 94, 0.5f)),   // 600Hz default
            2000.0f * std::pow (
                juce::jmax (1.1f, (float) sampleRate * 0.44f / 2000.0f),
                ccValue (solarfaze, 95, 0.64f))   // approx 7.7kHz default
        };
        for (int ch = 0; ch < channels; ++ch)
            for (int band = 0; band < 3; ++band)
                solarBands[(size_t) ch][(size_t) band].tune (
                    sampleRate, hz[band]);

        const float toneCoeff = onePoleCoeff (
            sampleRate, 600.0f + 17500.0f * satTone * satTone);
        const float threshold = 0.0003f
            * std::pow (100.0f, ccValue (solarfaze, 98, 0.15f));
        const float smooth = onePoleCoeff (sampleRate, 15.0f);
        const auto saturate = [&] (float input, int ch) noexcept
        {
            float result = 0.0f;
            for (int band = 0; band < 3; ++band)
            {
                const float isolated = solarBands[(size_t) ch][(size_t) band]
                    .process (input);
                const float drive = mainDrive * bandDrive[band];
                const float coloured = std::tanh (isolated * drive)
                    / juce::jmax (1.0f, drive);
                result += coloured * levels[band];
            }
            solarToneLow[(size_t) ch] +=
                toneCoeff * (result - solarToneLow[(size_t) ch]);
            return solarToneLow[(size_t) ch];
        };

        for (int sample = 0; sample < count; ++sample)
        {
            float dry[2] {};
            float beforeDetune[2] {};
            float envSample = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
            {
                dry[ch] = buffer.getSample (ch, sample);
                envSample = juce::jmax (envSample, std::abs (dry[ch]));
            }
            solarGateEnvelope += 0.0015f
                * (envSample - solarGateEnvelope);
            const float gateTarget = ! gateOn
                || solarGateEnvelope >= threshold ? 1.0f : 0.0f;
            solarGateGain += smooth * (gateTarget - solarGateGain);
            const float envelope = juce::jlimit (
                0.0f, 1.0f, solarGateEnvelope * 5.0f);
            solarDetuneSmoothed += smooth
                * (detuneAmount - solarDetuneSmoothed);
            solarSizeSmoothed += smooth
                * (detuneSize - solarSizeSmoothed);
            solarSatMixSmoothed += smooth
                * ((satOn ? satMix : 0.0f) - solarSatMixSmoothed);
            solarDetMixSmoothed += smooth
                * ((detOn ? detuneMix : 0.0f) - solarDetMixSmoothed);

            for (int ch = 0; ch < channels; ++ch)
            {
                dry[ch] *= solarGateGain; // gate affects the entire signal
                const float processed = satFirst && satOn
                    ? saturate (dry[ch], ch) : 0.0f;
                // Pass input internally if the first stage is bypassed.
                // Only processed stages may contribute to the final output.
                beforeDetune[ch] = satFirst && satOn
                    ? processed * solarSatMixSmoothed : dry[ch];
                solarDelay.setSample (
                    ch, solarWrite, protect (beforeDetune[ch]));
            }

            const float semitones = 0.52f * solarDetuneSmoothed
                * juce::jlimit (
                    0.0f, 2.0f,
                    1.0f + detuneEnv * (envelope * 2.0f - 1.0f));
            const double window = sampleRate
                * (0.012 + 0.145 * (double) solarSizeSmoothed);
            // Recompute pitch ratios every 16 samples, not at audio rate.
            if (--solarRatioRefresh <= 0)
            {
                solarRatio[0] = std::pow (
                    2.0, (double) semitones / 12.0);
                solarRatio[1] = 1.0 / solarRatio[0];
                solarRatioRefresh = 16;
            }
            for (int ch = 0; ch < channels; ++ch)
            {
                const double delta = solarRatio[(size_t) ch] - 1.0;
                auto& phase = solarPitchPhase[(size_t) ch];
                phase += std::abs (delta) / window;
                if (phase >= 1.0) phase -= std::floor (phase);
                const double otherPhase = phase < 0.5
                    ? phase + 0.5 : phase - 0.5;
                const double headA = sampleRate * 0.003
                    + (delta >= 0.0 ? 1.0 - phase : phase) * window;
                const double headB = sampleRate * 0.003
                    + (delta >= 0.0 ? 1.0 - otherPhase : otherPhase) * window;
                // Smooth cubic crossfade, no costly cosine per sample.
                const float distance = (float) std::abs (2.0 * phase - 1.0);
                const float weight = 1.0f
                    - distance * distance * (3.0f - 2.0f * distance);
                const float shifted = std::abs (delta) < 1.0e-6
                    ? beforeDetune[ch]
                    : readDelay (solarDelay, ch, solarWrite, headA) * weight
                        + readDelay (solarDelay, ch, solarWrite, headB)
                            * (1.0f - weight);
                float output = 0.0f;
                if (satFirst)
                {
                    // SAT -> DETUNE. DETUNE takes the processed SAT
                    // output as its carrier; neither stage adds DRY.
                    output = detOn
                        ? shifted * solarDetMixSmoothed
                        : (satOn ? beforeDetune[ch] : 0.0f);
                }
                else
                {
                    // DETUNE -> SAT: the dry input is a carrier only
                    // when the DETUNE stage is bypassed.
                    const float detWet = detOn
                        ? shifted * solarDetMixSmoothed : 0.0f;
                    const float carrier = detOn ? detWet : dry[ch];
                    output = satOn
                        ? saturate (carrier, ch) * solarSatMixSmoothed
                        : detWet;
                }
                buffer.setSample (ch, sample, protect (output));
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

        // The first selected delay must be established before any history
        // is written. Otherwise wet-only output needlessly waits for the
        // old hardcoded 420-ms default even with an 85-ms time selection.
        if (polySamplesWritten == 0)
        {
            polyCurrentDelay[(size_t) channel] = targetSamples;
            polyOldDelay[(size_t) channel] = targetSamples;
            polyTargetDelay[(size_t) channel] = targetSamples;
            polyCrossfadeRemaining[(size_t) channel] = 0;
            return;
        }
        // Never crossfade to a delay tap that has not yet been written.
        // Retain the previous audible delay until the requested history
        // exists, then transition with the established 45-ms crossfade.
        if (targetSamples > (double) polySamplesWritten
                - sampleRate * 0.050)
            return;
        // A moving XY previously restarted the 45-ms crossfade every
        // audio block, so no transition could complete. Finish the
        // current fade before beginning another retarget.
        if (polyCrossfadeRemaining[(size_t) channel] > 0)
            return;
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
        const float validFade = juce::jlimit (0.0f, 1.0f,
            (float) ((polySamplesWritten - samples - 2.0)
                / juce::jmax (1.0, sampleRate * 0.050)));
        return readDelay (polyDelay, channel, polyWrite, samples)
            * validFade;
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
        // Physical knob and MIDI CC#8 both select one of nine times.
        const int physicalIndex = juce::jlimit (
            0, 8, juce::roundToInt (x * 8.0f));
        const double timeSeconds = timeIndex < 0
            ? secondsTable[physicalIndex] : secondsTable[timeIndex];
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
        const float feedback = 1.10f * repeats;
        const bool bypass = ccOn (polyphrase, 13);
        const bool trails = ccOn (polyphrase, 16);
        const bool infinite = ccOn (polyphrase, 14);
        const float mix = ccValue (polyphrase, 7, 0.12f + 0.68f * y);
        const float wetTarget = bypass && ! trails ? 0.0f : mix;
        const float wetAlpha = onePoleCoeff (sampleRate, 12.0f);
        // Polyphrase's TONE is a bipolar feedback EQ: centre remains flat.
        const float tone = ccValue (polyphrase, 2, 0.5f);
        const float toneDistance = 2.0f * std::abs (tone - 0.5f);
        const float cutoff = tone < 0.5f
            ? 18000.0f * std::pow (0.025f, toneDistance)
            : 40.0f + 2400.0f * toneDistance * toneDistance;
        const float toneCoeff = onePoleCoeff (sampleRate, cutoff);
        const float depthKnob = ccValue (polyphrase, 3, 0.5f);
        // DEPTH: noon = zero; left SINE, right RANDOM.
        const float depth = std::abs (2.0f * depthKnob - 1.0f);
        const float modDepthSamples = (float) sampleRate
            * 0.0035f * depth;
        const float modRate = 0.1f + 9.9f * ccValue (polyphrase, 4, x);
        const bool randomMode = depthKnob > 0.5f;
        const float randomAlpha = onePoleCoeff (sampleRate, modRate * 0.7f);

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
                randomAlpha * (polyRandomTarget - polyRandom);
            polyPhase +=
                juce::MathConstants<double>::twoPi
                * (double) modRate / sampleRate;
            if (polyPhase >= juce::MathConstants<double>::twoPi)
                polyPhase -= juce::MathConstants<double>::twoPi;
            const float mod = depth < 0.001f ? 0.0f
                : randomMode ? polyRandom : std::sin ((float) polyPhase);

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
                    toneCoeff * (delayed[ch] - polyLowpass[(size_t) ch]);
                polyToneOut[(size_t) ch] = tone < 0.5f
                    ? delayed[ch] + (polyLowpass[(size_t) ch] - delayed[ch])
                        * toneDistance
                    : delayed[ch] - polyLowpass[(size_t) ch] * toneDistance;
            }

            // Middle/right XY area progressively becomes ping-pong.
            // When delay memory is empty (first seconds after reset),
            // fade in the wet signal rather than dropping the dry audio.
            const float history = juce::jlimit (0.0f, 1.0f,
                (float) (((double) polySamplesWritten
                    - std::min (
                        polyCurrentDelay[0], polyCurrentDelay[1]) + 2.0)
                / juce::jmax (1.0, sampleRate * 0.080)));
            const int style = ccRaw (polyphrase, 9);
            const int echoStyle = style < 0 ? 0
                : style < 44 ? 0 : style < 88 ? 1 : 2;
            const float pingPong = echoStyle == 1 ? 1.0f : 0.0f;
            const bool monoStyle = echoStyle == 2;
            polyWetSmoothed += wetAlpha
                * (wetTarget * history - polyWetSmoothed);
            const float monoReturn = 0.5f * (polyToneOut[0]
                + polyToneOut[(size_t) (channels > 1 ? 1 : 0)]);
            for (int ch = 0; ch < channels; ++ch)
            {
                const int other = channels > 1 ? 1 - ch : ch;
                const float fbSignal = monoStyle ? monoReturn
                    : polyToneOut[(size_t) ch] * (1.0f - pingPong)
                        + polyToneOut[(size_t) other] * pingPong;
                const float infiniteBoost = infinite
                    || (ccOn (polyphrase, 11, true)
                        && touchDown && y > 0.93f)
                    ? 0.998f : feedback;
                const float incoming = bypass ? 0.0f : dry[ch];
                const float externalReturn = monoStyle && channels > 1
                    ? dry[1] : fbSignal;
                const float write = incoming
                    + (monoStyle && ch == 0
                        ? externalReturn : fbSignal) * infiniteBoost;
                polyDelay.setSample (ch, polyWrite, protect (write));
                const float wet = delayed[ch] * polyWetSmoothed;
                // Right output is the external send in MONO style; no
                // unprocessed pass-through in other configurations.
                const float output = monoStyle && channels > 1 && ch == 1
                    ? polyToneOut[0]
                    : wet;
                buffer.setSample (ch, i, protect (output));
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
    std::atomic<bool> polyTriggerRequested { false };
    double polyTapQuarterSeconds = 0.0;

    double sampleRate = 48000.0;

    juce::AudioBuffer<float> ultraDelay;
    int ultraWrite = 0;
    double ultraPhase = 0.0;
    uint32_t ultraRandomState = 0x554c5452u;
    float ultraRandom = 0.0f;
    float ultraRandomTarget = 0.0f;
    int ultraRandomSamples = 1;
    std::array<PlateStage, 4> ultraInputDiff {};
    std::array<PlateStage, 2> ultraTankDiff {};
    std::array<PlateStage, 2> ultraTank {};
    std::array<float, 2> ultraFeedback {};
    std::array<float, 2> ultraDamped {};
    float ultraWetSmoothed = 0.0f;
    float ultraVibSmoothed = 0.0f;
    float ultraDepthSmoothed = 0.0f;

    std::array<RoseDetectorState, 48> roseDetectors {};
    std::array<std::array<ResonatorState, 12>, 2> roseState {};
    std::array<float, 2> roseToneLow { 0.0f, 0.0f };
    float roseMixSmoothed = 0.5f;
    float roseFeedbackSmoothed = 0.7f;
    double rosePhase = 0.0;

    std::array<float, 2> solarLow { 0.0f, 0.0f };
    std::array<float, 2> solarUpper { 0.0f, 0.0f };
    float solarGateEnvelope = 0.0f;
    float solarGateGain = 1.0f;
    float solarDetuneSmoothed = 0.0f;
    float solarSatMixSmoothed = 0.0f;
    float solarDetMixSmoothed = 0.0f;
    float solarSizeSmoothed = 0.5f;
    std::array<double, 2> solarPitchPhase { 0.1, 0.6 };
    std::array<double, 2> solarRatio { 1.0, 1.0 };
    int solarRatioRefresh = 0;
    std::array<float, 2> solarToneLow {};
    std::array<std::array<SolarBand, 3>, 2> solarBands {};
    juce::AudioBuffer<float> solarDelay;
    int solarWrite = 0;
    double solarPhase = 0.0;

    juce::AudioBuffer<float> polyDelay;
    int polyWrite = 0;
    int polySamplesWritten = 0;
    std::array<float, 2> polyLowpass { 0.0f, 0.0f };
    std::array<float, 2> polyToneOut { 0.0f, 0.0f };
    float polyWetSmoothed = 0.0f;
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
