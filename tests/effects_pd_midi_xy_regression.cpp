#include "z_libpd.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

constexpr int rate = 48000;
constexpr int tick = 64;

static void require (bool good, const char* message)
{
    if (! good)
        throw std::runtime_error (message);
}

static float renderPeak()
{
    std::array<float, tick * 2> input {};
    std::array<float, tick * 2> output {};
    float peak = 0.0f;
    for (int block = 0; block < 12; ++block)
    {
        require (libpd_process_float (1, input.data(), output.data()) == 0,
                 "Pd audio render failed");
        for (auto v : output)
        {
            require (std::isfinite (v), "Nonfinite MIDI synth audio");
            peak = std::max (peak, std::abs (v));
        }
    }
    return peak;
}

int main()
{
    try
    {
        require (libpd_init() == 0, "libpd_init failed");
        require (libpd_blocksize() == tick, "Pd block size must be 64");
        require (libpd_init_audio (2, 2, rate) == 0, "libpd_init_audio failed");
        libpd_start_message (1);
        libpd_add_float (1.0f);
        require (libpd_finish_message ("pd", "dsp") == 0, "Pd DSP enable failed");
        auto* patch = libpd_openfile (
            "Effects_MIDI_XY_Transport_Test_Synth.pd", "patches");
        require (patch != nullptr, "Could not load MIDI XY test Pd patch");

        // MIDI channels are 0-based in libpd. Pd [notein] uses channel 1.
        require (libpd_noteon (0, 69, 100) == 0, "Note On failed");
        require (libpd_float ("effects-x", 1.0f) == 0, "X send failed");
        require (libpd_float ("effects-y", 1.0f) == 0, "Y send failed");
        require (renderPeak() < 0.00001f, "Synth played before START");

        require (libpd_sysrealtime (0, 0xFA) == 0, "MIDI START failed");
        require (libpd_float ("effects-transport", 1.0f) == 0,
                 "Pd transport START send failed");
        require (libpd_bang ("effects-start") == 0, "START bang failed");
        require (renderPeak() > 0.1f, "Note On / START did not sound");

        require (libpd_float ("effects-x", 0.0f) == 0, "X zero send failed");
        require (renderPeak() < 0.00001f, "X did not control gain");
        require (libpd_float ("effects-x", 1.0f) == 0, "X restore failed");
        require (libpd_float ("effects-y", 0.0f) == 0, "Y zero send failed");
        require (renderPeak() < 0.00001f, "Y did not control gain");
        require (libpd_float ("effects-y", 1.0f) == 0, "Y restore failed");
        require (renderPeak() > 0.1f, "XY restore did not sound");

        require (libpd_sysrealtime (0, 0xFC) == 0, "MIDI STOP failed");
        require (libpd_float ("effects-transport", 0.0f) == 0,
                 "Pd transport STOP send failed");
        require (libpd_bang ("effects-stop") == 0, "STOP bang failed");
        require (renderPeak() < 0.00001f, "STOP did not silence synth");

        require (libpd_noteon (0, 69, 0) == 0, "Note Off failed");
        require (libpd_sysrealtime (0, 0xFB) == 0, "MIDI CONTINUE failed");
        require (libpd_float ("effects-transport", 1.0f) == 0,
                 "Pd transport CONTINUE send failed");
        require (libpd_bang ("effects-continue") == 0, "CONTINUE bang failed");
        require (renderPeak() < 0.00001f, "Note Off did not silence synth");

        require (libpd_sysrealtime (0, 0xF8) == 0, "MIDI CLOCK failed");
        require (libpd_bang ("effects-clock") == 0, "CLOCK bang failed");
        libpd_closefile (patch);
        std::puts ("[PASS] real libpd MIDI Note On/Off, transport, clock, XY");
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::fprintf (stderr, "[FAIL] Pd MIDI/XY native regression: %s\n",
                      ex.what());
        return 1;
    }
}
