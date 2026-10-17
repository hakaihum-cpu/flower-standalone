#pragma once
#include <algorithm>
#include <cstdint>

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

// Pick the VIDEO played inside a tile (never an image quadrant).
// 0: full A frame, 1: full B frame, 2: full C frame,
// 3: full A at a different playback position.
// Some MIDI-triggered mosaics repeat A in every cell; others mix A/B/C.
// A 2x2 mixed tile arrangement is exactly:
//     A     B
//     C     A(offset)
inline int tileSourceVariant (int row, int col, int divisions,
                             std::uint32_t eventSequence) noexcept
{
    if (divisions < 2 || eventSequence % 5u == 0u)
        return 0;
    return (row * divisions + col) % 4;
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
