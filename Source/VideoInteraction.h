#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace effects_video
{
// EUREKA follows actual input energy instead of advancing at a permanent
// 4.5 fps. Quiet input settles to a still frame; stronger transients run the
// user's original video at most its previous 4.5 fps decode cadence.
// No image-overlay, texture, or extra frame decoding is introduced.
inline float eurekaReactiveFrameRate (float inputActivity) noexcept
{
    if (! std::isfinite (inputActivity)) return 0.0f;
    const float energy = std::clamp (
        (inputActivity - 0.002f) * 20.0f, 0.0f, 1.0f);
    return energy < 0.01f ? 0.0f : 1.0f + 3.5f * energy;
}

// Input audio pitch drives mosaic tile count; MIDI note gates are removed.
// The separate audible-onset gate below chooses split events at ~20%.
// Silence or invalid pitch returns one full image when not holding a split.
// Valid low notes start at 2x2; increasing frequency reaches 10x10.
// Frequency is measured in Hz by a low-rate, independent YIN detector.
inline int divisionsForPitchHz (float hz) noexcept
{
    if (! std::isfinite (hz) || hz < 75.0f)
        return 1;

    constexpr float lowestHz = 75.0f;
    constexpr float highestHz = 1100.0f;
    const float octaves = std::log2 (std::clamp (hz, lowestHz, highestHz) / lowestHz);
    const float range = std::log2 (highestHz / lowestHz);
    return std::clamp (2 + (int) std::lround (8.0f * octaves / range), 2, 10);
}

// Onset-latched, roughly 1-in-5 video split.  The GUI calls this at
// 30 Hz with the pre-FX input RMS and the independently detected pitch grid.
// A sustained note is ONE chance, not 30 chances per second.  Pitch changes
// within an accepted split still change the number of video tiles.
// Quiet for 3 ticks (~100 ms) rearms the next audible onset and clears tiles.
class AudioMosaicGate
{
public:
    struct Result
    {
        int divisions = 1;
        bool started = false;
    };

    void reset() noexcept
    {
        armed = true;
        quietTicks = 0;
        holdTicks = 0;
        heldDivisions = 1;
    }

    Result advance (float rms, int pitchDivisions) noexcept
    {
        constexpr int quietToRearm = 3;
        constexpr int holdDuration = 75; // Up to 2.5 seconds at 30 Hz.
        const float level = std::isfinite (rms) ? rms : 0.0f;
        pitchDivisions = std::clamp (pitchDivisions, 1, 10);

        quietTicks = level < 0.0018f
            ? std::min (quietTicks + 1, quietToRearm) : 0;
        if (quietTicks == quietToRearm)
        {
            armed = true;
            holdTicks = 0;
            heldDivisions = 1;
        }

        Result result;
        if (armed && level >= 0.003f && pitchDivisions > 1)
        {
            armed = false;
            randomState ^= randomState << 13;
            randomState ^= randomState >> 17;
            randomState ^= randomState << 5;
            if (randomState % 5u == 0u)
            {
                holdTicks = holdDuration;
                heldDivisions = pitchDivisions;
                result.started = true;
            }
        }

        if (holdTicks > 0)
        {
            if (pitchDivisions > 1)
                heldDivisions = pitchDivisions;
            result.divisions = heldDivisions;
            --holdTicks;
        }
        return result;
    }

private:
    std::uint32_t randomState = 0x45464658u;
    bool armed = true;
    int quietTicks = 0;
    int holdTicks = 0;
    int heldDivisions = 1;
};

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
