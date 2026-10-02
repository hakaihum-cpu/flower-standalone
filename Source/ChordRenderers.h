#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace chordfx
{
class SampleChordRenderer
{
public:
    static constexpr int maxVoices = 6;

    void prepare (double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 48000.0;
        history.assign ((size_t) std::max (4096.0, sampleRate * 2.20), 0.0f);
        capture.assign ((size_t) std::max (4096.0, sampleRate * 2.05), 0.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill (history.begin(), history.end(), 0.0f);
        std::fill (capture.begin(), capture.end(), 0.0f);
        writePosition = 0;
        historyFilled = 0;
        captureLength = 0;
        targetCount = 0;
        sourceMidi = 60.0f;
        phase.fill (0.0);
        ratio.fill (1.0);
    }

    void pushInput (float sample) noexcept
    {
        if (history.empty())
            return;

        history[(size_t) writePosition] = sample;
        writePosition = (writePosition + 1) % (int) history.size();
        historyFilled = std::min (historyFilled + 1, (int) history.size());
    }

    void setHold (float amount01) noexcept
    {
        hold = std::clamp (amount01, 0.0f, 1.0f);
    }

    float captureSeconds() const noexcept
    {
        return 0.240f + hold * 1.760f;
    }

    float crossfadeSeconds() const noexcept
    {
        return 0.020f + hold * 0.140f;
    }

    void setPlan (const std::vector<int>& midiNotes, float sourceMidiFloat,
                  bool recapture = true) noexcept
    {
        sourceMidi = sourceMidiFloat;
        targetCount = std::min ((int) midiNotes.size(), maxVoices);

        for (int i = 0; i < targetCount; ++i)
        {
            const int note = std::clamp (midiNotes[(size_t) i], 60, 83);
            ratio[(size_t) i] = std::pow (2.0, ((double) note - (double) sourceMidi) / 12.0);
            phase[(size_t) i] = 0.0;
        }

        for (int i = targetCount; i < maxVoices; ++i)
        {
            ratio[(size_t) i] = 1.0;
            phase[(size_t) i] = 0.0;
        }

        if (recapture || captureLength <= 0)
            captureRecentPhrase();
    }

    float renderSample (bool gateOpen) noexcept
    {
        if (! gateOpen || captureLength < 64 || targetCount <= 0)
            return 0.0f;

        float sum = 0.0f;
        for (int voice = 0; voice < targetCount; ++voice)
        {
            sum += readCaptureLooped (phase[(size_t) voice]);
            phase[(size_t) voice] += ratio[(size_t) voice];

            while (phase[(size_t) voice] >= (double) captureLength)
                phase[(size_t) voice] -= (double) captureLength;
        }

        return sum / std::sqrt ((float) std::max (1, targetCount));
    }

    bool hasCapture() const noexcept { return captureLength >= 64; }

private:
    void captureRecentPhrase() noexcept
    {
        if (history.empty() || capture.empty())
            return;

        // HOLD=0 preserves the approved 240 ms minimum. HOLD extends the
        // captured phrase continuously up to 2.0 s without ever shortening it.
        const int wanted = std::clamp (
            (int) std::lround (sampleRate * (double) captureSeconds()),
            256,
            (int) capture.size());

        if (historyFilled < wanted)
            return;

        int start = writePosition - wanted;
        while (start < 0) start += (int) history.size();

        double mean = 0.0;
        for (int i = 0; i < wanted; ++i)
        {
            const int index = (start + i) % (int) history.size();
            capture[(size_t) i] = history[(size_t) index];
            mean += capture[(size_t) i];
        }

        mean /= (double) wanted;
        for (int i = 0; i < wanted; ++i)
            capture[(size_t) i] -= (float) mean;

        captureLength = wanted;
        phase.fill (0.0);
    }

    float readCaptureLooped (double position) const noexcept
    {
        if (captureLength <= 1)
            return 0.0f;

        while (position < 0.0) position += (double) captureLength;
        while (position >= (double) captureLength) position -= (double) captureLength;

        auto readLinear = [this] (double p) noexcept
        {
            while (p < 0.0) p += (double) captureLength;
            while (p >= (double) captureLength) p -= (double) captureLength;

            const int i0 = (int) p;
            const int i1 = (i0 + 1) % captureLength;
            const float frac = (float) (p - (double) i0);
            return capture[(size_t) i0]
                 + frac * (capture[(size_t) i1] - capture[(size_t) i0]);
        };

        // HOLD=0 preserves the approved 20 ms seam. It grows with HOLD up to
        // 160 ms, while the clamp below keeps it below one quarter of capture.
        const int xf = std::clamp (
            (int) std::lround (sampleRate * (double) crossfadeSeconds()),
            16,
            std::max (16, captureLength / 4));

        if (position < (double) xf)
        {
            const float t = (float) (position / (double) xf);
            const float a = readLinear (position + (double) captureLength - (double) xf);
            const float b = readLinear (position);
            return a + (b - a) * t;
        }

        return readLinear (position);
    }

    double sampleRate = 48000.0;
    std::vector<float> history;
    std::vector<float> capture;
    int writePosition = 0;
    int historyFilled = 0;
    int captureLength = 0;
    int targetCount = 0;
    float sourceMidi = 60.0f;
    float hold = 0.0f;
    std::array<double, maxVoices> phase {};
    std::array<double, maxVoices> ratio { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
};

class SineArpeggiator
{
public:
    static constexpr int maxNotes = 6;

    void prepare (double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 48000.0;
        reset();
    }

    void reset() noexcept
    {
        noteCount = 0;
        currentNoteIndex = -1;
        previousNoteIndex = -1;
        phase = 0.0;
        frequency = 440.0;
        envelope = 0.0f;
        stepSamplesRemaining = 0;
        noteSamplesRemaining = 0;
        rng = 0x43484232u;
        noteTriggered = false;
    }

    void setPlan (const std::vector<int>& midiNotes) noexcept
    {
        noteCount = std::min ((int) midiNotes.size(), maxNotes);
        for (int i = 0; i < noteCount; ++i)
            notes[(size_t) i] = std::clamp (midiNotes[(size_t) i], 60, 83);
        currentNoteIndex = -1;
        previousNoteIndex = -1;
        stepSamplesRemaining = 0;
        noteSamplesRemaining = 0;
        noteTriggered = false;
    }

    bool consumeNoteTrigger() noexcept
    {
        const bool triggered = noteTriggered;
        noteTriggered = false;
        return triggered;
    }

    void setTiming (float bpm, float length01) noexcept
    {
        const double clampedBpm = std::clamp ((double) bpm, 40.0, 240.0);
        stepSamples = std::max (64, (int) std::lround (sampleRate * 60.0 / clampedBpm * 0.5));
        const float l = std::clamp (length01, 0.0f, 1.0f);
        noteGateSamples = std::max (32, (int) std::lround ((double) stepSamples * (0.18 + 0.70 * l)));
    }

    float renderSample (bool gateOpen) noexcept
    {
        if (! gateOpen || noteCount <= 0)
        {
            envelope *= 0.992f;
            const float out = std::sin (phase) * envelope * 0.22f;
            phase += 6.28318530717958647692 * frequency / sampleRate;
            if (phase >= 6.28318530717958647692)
                phase -= 6.28318530717958647692;
            return out;
        }

        if (--stepSamplesRemaining <= 0)
        {
            triggerRandomNote();
            stepSamplesRemaining = stepSamples;
        }

        if (noteSamplesRemaining > 0)
            --noteSamplesRemaining;

        const float target = noteSamplesRemaining > 0 ? 1.0f : 0.0f;
        const float coeff = target > envelope ? 0.018f : 0.0065f;
        envelope += coeff * (target - envelope);

        const float out = std::sin (phase) * envelope * 0.22f;
        phase += 6.28318530717958647692 * frequency / sampleRate;
        if (phase >= 6.28318530717958647692)
            phase -= 6.28318530717958647692;
        return out;
    }

private:
    uint32_t nextRandom() noexcept
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    }

    void triggerRandomNote() noexcept
    {
        if (noteCount <= 0)
            return;

        int index = (int) (nextRandom() % (uint32_t) noteCount);
        if (noteCount > 1 && index == previousNoteIndex)
            index = (index + 1 + (int) (nextRandom() % (uint32_t) (noteCount - 1))) % noteCount;

        previousNoteIndex = index;
        currentNoteIndex = index;
        const int midi = notes[(size_t) index];
        frequency = 440.0 * std::pow (2.0, ((double) midi - 69.0) / 12.0);
        // Keep oscillator phase and envelope continuous across note changes.
        // Resetting both on every random step caused sharp discontinuities
        // that were perceived as clipping/crackle.
        noteSamplesRemaining = noteGateSamples;
        noteTriggered = true;
    }

    double sampleRate = 48000.0;
    std::array<int, maxNotes> notes { 60, 64, 67, 72, 76, 79 };
    int noteCount = 0;
    int currentNoteIndex = -1;
    int previousNoteIndex = -1;
    double phase = 0.0;
    double frequency = 440.0;
    float envelope = 0.0f;
    int stepSamples = 12000;
    int stepSamplesRemaining = 0;
    int noteGateSamples = 6000;
    int noteSamplesRemaining = 0;
    uint32_t rng = 0x43484232u;
    bool noteTriggered = false;
};

class ChordBRandomFx
{
public:
    enum EffectType
    {
        none = 0,
        shortDelay,
        longDelay,
        tapeDelay,
        tapeSim,
        bitcrush,
        chorus,
        reverb
    };

    void prepare (double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 48000.0;
        const int delaySize = std::max (4096, (int) std::lround (sampleRate * 2.5));
        delayL.assign ((size_t) delaySize, 0.0f);
        delayR.assign ((size_t) delaySize, 0.0f);
        chorusBuffer.assign ((size_t) std::max (2048, (int) std::lround (sampleRate * 0.060)), 0.0f);
        reverbL.assign ((size_t) std::max (4096, (int) std::lround (sampleRate * 0.50)), 0.0f);
        reverbR.assign (reverbL.size(), 0.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill (delayL.begin(), delayL.end(), 0.0f);
        std::fill (delayR.begin(), delayR.end(), 0.0f);
        std::fill (chorusBuffer.begin(), chorusBuffer.end(), 0.0f);
        std::fill (reverbL.begin(), reverbL.end(), 0.0f);
        std::fill (reverbR.begin(), reverbR.end(), 0.0f);
        delayWrite = chorusWrite = reverbWrite = 0;
        activeEffect = none;
        probability = 0.0f;
        tapeFeedbackLpL = tapeFeedbackLpR = 0.0f;
        tapeTone = 0.0f;
        bitHeld = 0.0f;
        bitCounter = 0;
        lfoPhase = 0.0;
        rng = 0x45464658u;
    }

    void setProbability (float amount01) noexcept
    {
        probability = std::clamp (amount01, 0.0f, 1.0f);
    }

    void chooseForNote() noexcept
    {
        const float draw = (float) (nextRandom() & 0xffffu) / 65535.0f;
        if (draw >= probability)
        {
            activeEffect = none;
            return;
        }

        activeEffect = 1 + (int) (nextRandom() % 7u);
    }

    int getActiveEffect() const noexcept { return activeEffect; }

    void processSample (float input, float& left, float& right) noexcept
    {
        const auto effect = activeEffect;

        float shortL = 0.0f, shortR = 0.0f;
        processDelayPair (effect == shortDelay ? input : 0.0f,
                          (int) std::lround (sampleRate * 0.075),
                          0.26f, 0.46f, shortL, shortR);

        float longL = 0.0f, longR = 0.0f;
        processDelayPair (effect == longDelay ? input : 0.0f,
                          (int) std::lround (sampleRate * 0.360),
                          0.36f, 0.50f, longL, longR);

        float tapeDelayL = 0.0f, tapeDelayR = 0.0f;
        processTapeDelay (effect == tapeDelay ? input : 0.0f,
                          tapeDelayL, tapeDelayR);

        float chorusL = 0.0f, chorusR = 0.0f;
        processChorus (effect == chorus ? input : 0.0f, chorusL, chorusR);

        float revL = 0.0f, revR = 0.0f;
        processReverb (effect == reverb ? input : 0.0f, revL, revR);

        switch (effect)
        {
            case shortDelay: left = shortL; right = shortR; break;
            case longDelay:  left = longL;  right = longR;  break;
            case tapeDelay:  left = tapeDelayL; right = tapeDelayR; break;
            case tapeSim:
            {
                tapeTone += 0.18f * (input - tapeTone);
                const float saturated = std::tanh (1.8f * tapeTone) / std::tanh (1.8f);
                left = right = saturated * 0.88f;
                break;
            }
            case bitcrush:
            {
                if (--bitCounter <= 0)
                {
                    constexpr float levels = 31.0f;
                    bitHeld = std::round (input * levels) / levels;
                    bitCounter = 4;
                }
                left = right = bitHeld * 0.92f;
                break;
            }
            case chorus: left = chorusL; right = chorusR; break;
            case reverb: left = revL; right = revR; break;
            default: left = right = input; break;
        }

        // Keep the generated layer safely below full scale before the existing
        // CHORD-B mix/reverb headroom stage.
        left = std::clamp (left, -0.72f, 0.72f);
        right = std::clamp (right, -0.72f, 0.72f);

        lfoPhase += 6.28318530717958647692 * 0.31 / sampleRate;
        if (lfoPhase >= 6.28318530717958647692)
            lfoPhase -= 6.28318530717958647692;
    }

private:
    uint32_t nextRandom() noexcept
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    }

    float readInterpolated (const std::vector<float>& buffer,
                            double position) const noexcept
    {
        if (buffer.empty())
            return 0.0f;

        const double size = (double) buffer.size();
        while (position < 0.0) position += size;
        while (position >= size) position -= size;
        const int i0 = (int) position;
        const int i1 = (i0 + 1) % (int) buffer.size();
        const float frac = (float) (position - (double) i0);
        return buffer[(size_t) i0]
             + frac * (buffer[(size_t) i1] - buffer[(size_t) i0]);
    }

    void processDelayPair (float input, int delaySamples, float feedback,
                           float wet, float& left, float& right) noexcept
    {
        if (delayL.empty())
        {
            left = right = input;
            return;
        }

        const int size = (int) delayL.size();
        delaySamples = std::clamp (delaySamples, 1, size - 2);
        int read = delayWrite - delaySamples;
        if (read < 0) read += size;

        const float dl = delayL[(size_t) read];
        const float dr = delayR[(size_t) read];
        delayL[(size_t) delayWrite] = input + dl * feedback;
        delayR[(size_t) delayWrite] = input + dr * feedback;

        left = input * 0.76f + dl * wet;
        right = input * 0.76f + dr * wet;
        delayWrite = (delayWrite + 1) % size;
    }

    void processTapeDelay (float input, float& left, float& right) noexcept
    {
        if (delayL.empty())
        {
            left = right = input;
            return;
        }

        const double base = sampleRate * 0.235;
        const double mod = sampleRate * 0.0045 * std::sin (lfoPhase);
        const double readPos = (double) delayWrite - base - mod;
        const float dl = readInterpolated (delayL, readPos);
        const float dr = readInterpolated (delayR, readPos - sampleRate * 0.003);

        tapeFeedbackLpL += 0.16f * (dl - tapeFeedbackLpL);
        tapeFeedbackLpR += 0.16f * (dr - tapeFeedbackLpR);
        delayL[(size_t) delayWrite] =
            std::tanh (input + tapeFeedbackLpL * 0.33f);
        delayR[(size_t) delayWrite] =
            std::tanh (input + tapeFeedbackLpR * 0.33f);

        left = input * 0.74f + tapeFeedbackLpL * 0.52f;
        right = input * 0.74f + tapeFeedbackLpR * 0.52f;
        delayWrite = (delayWrite + 1) % (int) delayL.size();
    }

    void processChorus (float input, float& left, float& right) noexcept
    {
        if (chorusBuffer.empty())
        {
            left = right = input;
            return;
        }

        chorusBuffer[(size_t) chorusWrite] = input;
        const double modL = sampleRate * (0.014 + 0.0045 * std::sin (lfoPhase));
        const double modR = sampleRate * (0.017 + 0.0050 * std::sin (lfoPhase + 1.57079632679));
        const float dl = readInterpolated (chorusBuffer, (double) chorusWrite - modL);
        const float dr = readInterpolated (chorusBuffer, (double) chorusWrite - modR);
        left = input * 0.76f + dl * 0.42f;
        right = input * 0.76f + dr * 0.42f;
        chorusWrite = (chorusWrite + 1) % (int) chorusBuffer.size();
    }

    void processReverb (float input, float& left, float& right) noexcept
    {
        if (reverbL.empty())
        {
            left = right = input;
            return;
        }

        const int size = (int) reverbL.size();
        auto tap = [size] (int write, int offset)
        {
            int p = write - offset;
            while (p < 0) p += size;
            return p % size;
        };

        const int aL = tap (reverbWrite, (int) std::lround (sampleRate * 0.037));
        const int bL = tap (reverbWrite, (int) std::lround (sampleRate * 0.071));
        const int aR = tap (reverbWrite, (int) std::lround (sampleRate * 0.043));
        const int bR = tap (reverbWrite, (int) std::lround (sampleRate * 0.089));

        const float wetL = reverbL[(size_t) aL] * 0.58f
                         + reverbL[(size_t) bL] * 0.42f;
        const float wetR = reverbR[(size_t) aR] * 0.58f
                         + reverbR[(size_t) bR] * 0.42f;

        reverbL[(size_t) reverbWrite] =
            input + (wetL * 0.38f + wetR * 0.14f);
        reverbR[(size_t) reverbWrite] =
            input + (wetR * 0.38f + wetL * 0.14f);

        left = input * 0.70f + wetL * 0.42f;
        right = input * 0.70f + wetR * 0.42f;
        reverbWrite = (reverbWrite + 1) % size;
    }

    double sampleRate = 48000.0;
    float probability = 0.0f;
    int activeEffect = none;
    std::vector<float> delayL, delayR;
    std::vector<float> chorusBuffer;
    std::vector<float> reverbL, reverbR;
    int delayWrite = 0;
    int chorusWrite = 0;
    int reverbWrite = 0;
    float tapeFeedbackLpL = 0.0f;
    float tapeFeedbackLpR = 0.0f;
    float tapeTone = 0.0f;
    float bitHeld = 0.0f;
    int bitCounter = 0;
    double lfoPhase = 0.0;
    uint32_t rng = 0x45464658u;
};
}
