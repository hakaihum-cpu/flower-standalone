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
        history.assign ((size_t) std::max (4096.0, sampleRate * 0.30), 0.0f);
        capture.assign (4096, 0.0f);
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
        capturePending = false;
    }

    void pushInput (float sample) noexcept
    {
        if (history.empty())
            return;

        history[(size_t) writePosition] = sample;
        writePosition = (writePosition + 1) % (int) history.size();
        historyFilled = std::min (historyFilled + 1, (int) history.size());

        if (capturePending)
            tryCapture();
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
        capturePending = targetCount > 0 && (recapture || captureLength < 32);
        if (capturePending)
            tryCapture();
    }

    float renderSample (bool gateOpen) noexcept
    {
        if (! gateOpen || captureLength < 32 || targetCount <= 0)
            return 0.0f;

        float sum = 0.0f;
        for (int voice = 0; voice < targetCount; ++voice)
        {
            sum += readCapture (phase[(size_t) voice]);
            phase[(size_t) voice] += ratio[(size_t) voice];
            while (phase[(size_t) voice] >= (double) captureLength)
                phase[(size_t) voice] -= (double) captureLength;
        }

        return sum / std::sqrt ((float) std::max (1, targetCount));
    }

    bool hasCapture() const noexcept { return captureLength >= 32; }

private:
    float historyAtAge (int age) const noexcept
    {
        if (history.empty() || age < 0 || age >= historyFilled)
            return 0.0f;
        int index = writePosition - 1 - age;
        while (index < 0) index += (int) history.size();
        return history[(size_t) index];
    }

    bool isPositiveCrossingAtAge (int age) const noexcept
    {
        if (age <= 0 || age >= historyFilled)
            return false;
        const float older = historyAtAge (age);
        const float newer = historyAtAge (age - 1);
        return older <= 0.0f && newer > 0.0f;
    }

    void tryCapture() noexcept
    {
        if (! capturePending || targetCount <= 0 || historyFilled < 96)
            return;

        const double hz = 440.0 * std::pow (2.0, ((double) sourceMidi - 69.0) / 12.0);
        const int period = std::clamp ((int) std::lround (sampleRate / std::max (40.0, hz)), 16, 1200);
        const int desired = std::clamp (period * 2, 64, (int) capture.size() - 2);

        if (historyFilled < desired + period / 2 + 8)
            return;

        int newestCrossing = -1;
        const int recentSearch = std::min (period * 2, historyFilled - 2);
        for (int age = 1; age <= recentSearch; ++age)
        {
            if (isPositiveCrossingAtAge (age))
            {
                newestCrossing = age;
                break;
            }
        }
        if (newestCrossing < 0)
            return;

        const int targetOlderAge = newestCrossing + desired;
        const int searchRadius = std::max (8, period / 2);
        const int begin = std::max (newestCrossing + 32, targetOlderAge - searchRadius);
        const int end = std::min (historyFilled - 2, targetOlderAge + searchRadius);

        int oldestCrossing = -1;
        int bestDistance = 1000000;
        for (int age = begin; age <= end; ++age)
        {
            if (! isPositiveCrossingAtAge (age))
                continue;
            const int distance = std::abs (age - targetOlderAge);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                oldestCrossing = age;
            }
        }
        if (oldestCrossing < 0)
            return;

        const int length = std::clamp (oldestCrossing - newestCrossing,
                                       32, (int) capture.size());
        double mean = 0.0;
        for (int i = 0; i < length; ++i)
        {
            const int age = oldestCrossing - 1 - i;
            capture[(size_t) i] = historyAtAge (age);
            mean += capture[(size_t) i];
        }
        mean /= (double) length;
        for (int i = 0; i < length; ++i)
            capture[(size_t) i] -= (float) mean;

        captureLength = length;
        phase.fill (0.0);
        capturePending = false;
    }

    float readCapture (double position) const noexcept
    {
        if (captureLength <= 1)
            return 0.0f;

        while (position < 0.0) position += (double) captureLength;
        while (position >= (double) captureLength) position -= (double) captureLength;

        const int i0 = (int) position;
        const int i1 = (i0 + 1) % captureLength;
        const float frac = (float) (position - (double) i0);
        return capture[(size_t) i0]
             + frac * (capture[(size_t) i1] - capture[(size_t) i0]);
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
    bool capturePending = false;
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
            const float out = std::sin (phase) * envelope * 0.32f;
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

        const float out = std::sin (phase) * envelope * 0.32f;
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
        phase = 0.0;
        envelope *= 0.25f;
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
