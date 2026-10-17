#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <array>

namespace effects_video
{
// Only one out of approximately five MIDI note-ons changes the picture.
// The caller advances its existing xorshift RNG on each note-on.
inline bool shouldDivideOnNote (std::uint32_t randomState) noexcept
{
    return randomState % 5u == 0u;
}

// C4..C5 stays a single picture. Rising from C#5 to C7 progressively
// increases the number of repeated cells; practical notes above C7 cap at 10x10.
inline int divisionsForNote (int midiNote) noexcept
{
    if (midiNote <= 72) return 1;
    const int aboveC5 = std::min (24, midiNote - 72);
    return std::min (10, 1 + (aboveC5 * 9 + 23) / 24);
}

// A lightweight pitch reader for the VISUAL layer. Input samples are
// decimated and copied from a lock-free audio tap; YIN operates only on the
// 30Hz UI thread so the expensive Vongon audio callback is untouched.
// Returns -1 on silence, noise, or ambiguous pitch.
inline int visualPitchMidi (const float* samples, int length, float sampleRate) noexcept
{
    if (samples == nullptr || length < 256 || sampleRate < 1000.0f)
        return -1;
    constexpr int maximumLag = 128;
    std::array<float, maximumLag + 1> diff {};
    double rmsSum = 0.0;
    for (int i = 0; i < length; ++i)
        rmsSum += (double) samples[i] * (double) samples[i];
    if (std::sqrt (rmsSum / (double) length) < 0.006)
        return -1;

    const int firstLag = std::max (2, (int) (sampleRate / 2300.0f));
    const int lastLag = std::min (maximumLag,
        std::min (length / 4, (int) (sampleRate / 200.0f)));
    if (lastLag <= firstLag)
        return -1;
    double running = 0.0;
    int selected = -1;
    float best = 1.0f;
    for (int lag = 1; lag <= lastLag; ++lag)
    {
        double energy = 0.0;
        for (int i = 0; i < length - lag; ++i)
        {
            const float error = samples[i] - samples[i + lag];
            energy += (double) error * (double) error;
        }
        running += energy;
        diff[(size_t) lag] = running > 0.0
            ? (float) (energy * (double) lag / running) : 1.0f;
        if (lag >= firstLag && diff[(size_t) lag] < best)
        {
            best = diff[(size_t) lag];
            selected = lag;
        }
    }
    if (selected < 0 || best > 0.20f)
        return -1;
    for (int i = firstLag; i < selected; ++i)
        if (diff[(size_t) i] < 0.16f)
        {
            selected = i;
            break;
        }

    float lag = (float) selected;
    if (selected > 1 && selected < lastLag)
    {
        const float a = diff[(size_t) (selected - 1)];
        const float b = diff[(size_t) selected];
        const float c = diff[(size_t) (selected + 1)];
        const float denom = 2.0f * (2.0f * b - c - a);
        if (std::abs (denom) > 1.0e-9f)
            lag += (c - a) / denom;
    }
    if (lag <= 1.0f)
        return -1;
    const float hz = sampleRate / lag;
    if (hz < 240.0f || hz > 2300.0f)
        return -1;
    const float note = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    return std::clamp ((int) std::lround (note), 0, 127);
}

// MULTI: upper left/right = ULTRASHEER/ROSETONES,
// lower left/right = SOLARFAZE/POLYPHRASE.
inline int vongonModeForXY (float x, float y) noexcept
{
    return 5 + (x >= 0.5f ? 1 : 0) + (y < 0.5f ? 2 : 0);
}
inline float localX (float x) noexcept
{
    return std::clamp (x < 0.5f ? x * 2.0f : (x - 0.5f) * 2.0f, 0.0f, 1.0f);
}
inline float localY (float y) noexcept
{
    return std::clamp (y < 0.5f ? y * 2.0f : (y - 0.5f) * 2.0f, 0.0f, 1.0f);
}
}
