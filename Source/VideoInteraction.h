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

// Hash event+cell into a reproducible pseudorandom value.
// A change of event alters the arrangement, not a permanent A/B/C order.
inline std::uint32_t tileHash (std::uint32_t seed) noexcept
{
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    return seed ^ (seed >> 16);
}

// Every tile contains a COMPLETE rescaled video frame, never an image
// quadrant. Most split events independently pick one of four video streams
// per cell. Occasionally all cells repeat video A, as requested.
// 0=main video, 1/2=other clips, 3=main clip at a different seek point.
inline int tileSourceVariant (int row, int col, int divisions,
                             std::uint32_t eventSequence) noexcept
{
    if (divisions < 2 || eventSequence % 8u == 0u)
        return 0;
    const auto cell = (std::uint32_t) (row * divisions + col);
    const auto seed = eventSequence * 0x9e3779b9u
                    + (cell + 1u) * 0x85ebca6bu;
    return (int) (tileHash (seed) % 4u);
}

// Other-video choices vary per split event, too. No positional identity
// (such as B always being the top right) is assigned to a particular clip.
inline int tileAlternateClipOffset (int slot, int clipCount,
                                    std::uint32_t eventSequence) noexcept
{
    if (clipCount <= 1)
        return 0;
    const auto seed = eventSequence ^ (
        (std::uint32_t) (slot + 1) * 0x517cc1b7u);
    return 1 + (int) (tileHash (seed) % (std::uint32_t) (clipCount - 1));
}

inline int tileAlternateSeek (int slot, int framesPerClip,
                              std::uint32_t eventSequence) noexcept
{
    if (framesPerClip <= 1)
        return 0;
    const auto seed = eventSequence * 0x94d049bbu
                    + (std::uint32_t) (slot + 1) * 0x27d4eb2du;
    return 1 + (int) (tileHash (seed) % (std::uint32_t) (framesPerClip - 1));
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
