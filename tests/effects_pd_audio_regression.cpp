#include "z_libpd.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

static constexpr int framesPerTick = 64;
static constexpr int rate = 48000;
static constexpr int seconds = 1;
static constexpr int total = rate * seconds;

static void require (bool okay, const char* why)
{
    if (! okay)
        throw std::runtime_error (why);
}

struct Result
{
    float direct = 0.0f;
    float echo = 0.0f;
    float leak = 0.0f;
    float peak = 0.0f;
};

static Result run (int sourceChannel)
{
    auto* patch = libpd_openfile (
        "Effects_Simple_Stereo_Delay.pd", "patches");
    require (patch != nullptr, "Pd Vanilla patch did not load");
    std::array<float, framesPerTick * 2> input {};
    std::array<float, framesPerTick * 2> output {};
    Result result;
    for (int tick = 0; tick < total / framesPerTick; ++tick)
    {
        input.fill (0.0f);
        if (tick == 0)
            input[(size_t) sourceChannel] = 0.75f;
        require (libpd_process_float (1, input.data(), output.data()) == 0,
                 "libpd DSP render failed");

        for (int i = 0; i < framesPerTick; ++i)
        {
            const int index = tick * framesPerTick + i;
            const float same = output[(size_t) 2 * i + (size_t) sourceChannel];
            const float opposite = output[(size_t) 2 * i
                + (size_t) (1 - sourceChannel)];
            require (std::isfinite (same) && std::isfinite (opposite),
                     "Pd generated nonfinite samples");
            result.peak = std::max (result.peak, std::abs (same));
            result.leak = std::max (result.leak, std::abs (opposite));
            if (index < 256)
                result.direct = std::max (result.direct, std::abs (same));
            // Allow block-boundary compensation in Pd delay line timing.
            if (index > 16800 && index < 18000)
                result.echo = std::max (result.echo, std::abs (same));
        }
    }
    libpd_closefile (patch);
    return result;
}

int main()
{
    try
    {
        require (libpd_init() == 0, "libpd_init failed");
        require (libpd_blocksize() == framesPerTick,
                 "Unexpected libpd tick size");
        require (libpd_init_audio (2, 2, rate) == 0,
                 "libpd_init_audio failed");
        // Pd needs [; pd dsp 1( after libpd_init_audio.
        libpd_start_message (1);
        libpd_add_float (1.0f);
        require (libpd_finish_message ("pd", "dsp") == 0,
                 "Could not turn Pure Data DSP on");
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto r = run (ch);
            std::printf (
                "PATCH Pd INPUT %c: direct=%.7f echo=%.7f "
                "oppositeLeak=%.8f peak=%.6f\n",
                ch == 0 ? 'L' : 'R',
                r.direct, r.echo, r.leak, r.peak);
            require (r.direct > 0.10f, "Pd direct component silent");
            require (r.echo > 0.025f, "Pd stereo delay tail silent");
            require (r.leak < 0.00001f, "Pd stereo channels cross-leak");
        }
        std::puts ("[PASS] actual libpd DSP: L-only and R-only input");
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf (stderr, "[FAIL] actual libpd DSP: %s\n", e.what());
        return 1;
    }
}
