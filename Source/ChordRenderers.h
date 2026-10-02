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
        history.assign ((size_t) std::max (4096.0, sampleRate * 0.60), 0.0f);
        capture.assign ((size_t) std::max (4096.0, sampleRate * 0.28), 0.0f);
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

        // Capture a substantially longer real phrase (~240 ms), not one/two
        // pitch periods. The earlier ~90 ms loop repeated too quickly and
        // produced a metallic/comb-like character.
        const int wanted = std::clamp (
            (int) std::lround (sampleRate * 0.240),
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

        // Crossfade only across the loop seam (~20 ms). Most of the captured
        // phrase remains untouched while the longer seam suppresses metallic
        // repetition/clicks.
        const int xf = std::clamp (
            (int) std::lround (sampleRate * 0.020),
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
};
}
