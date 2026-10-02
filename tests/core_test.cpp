#include "../Source/TheoryEngine.h"
#include "../Source/YinPitchDetector.h"
#include "../Source/GranularPitchBank.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    chordfx::TheoryEngine theory;
    theory.setRandomSeed (1);
    theory.setComplexity (1.0f);
    theory.setWidth (1.0f);
    auto p = theory.noteOn (60);
    assert (! p.midiNotes.empty());
    for (int i=0;i<1000;++i)
    {
        p = theory.advance();
        assert (! p.midiNotes.empty());
        for (int n : p.midiNotes) { assert(n >= 48); assert(n <= 96); }
    }

    // Regression: a low input must not be forced into a fixed C4/C5 voicing.
    chordfx::TheoryEngine lowTheory;
    lowTheory.setWidth (0.35f);
    auto low = lowTheory.noteOn (45); // A2
    assert (! low.midiNotes.empty());
    for (int n : low.midiNotes)
    {
        assert (n >= 48);              // existing C3 hard floor
        assert (n - 45 <= 16);         // no +24 semitone chipmunk jump
    }

    chordfx::YinPitchDetector yin;
    yin.prepare (48000.0, 1024, 256);
    chordfx::PitchEstimate est;
    for (int i=0;i<48000/2;++i)
    {
        float s=0.25f*std::sin(2.0*M_PI*220.0*i/48000.0);
        auto e=yin.pushSample(s); if(e.valid) est=e;
    }
    std::cout << "yin=" << est.hz << " conf=" << est.confidence << "\n";
    assert(est.valid && std::abs(est.hz-220.0f) < 3.0f);

    chordfx::GranularPitchBank bank;
    bank.prepare(48000.0,256);
    bank.setTargetRatios({2.0f});

    std::vector<float> shifted;
    shifted.reserve (48000);
    for(int i=0;i<96000;++i)
    {
        float s=0.25f*std::sin(2.0*M_PI*220.0*i/48000.0);
        float y=bank.processSample(s);
        if (i >= 48000) shifted.push_back(y);
    }

    auto toneMagnitude = [&shifted] (double hz)
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < shifted.size(); ++i)
        {
            const double phase = 2.0 * M_PI * hz * (double) i / 48000.0;
            re += shifted[i] * std::cos (phase);
            im -= shifted[i] * std::sin (phase);
        }
        return std::sqrt (re * re + im * im);
    };

    const double target440 = toneMagnitude (440.0);
    const double oldSideband408 = toneMagnitude (408.0);
    const double oldSideband502 = toneMagnitude (502.0);
    std::cout << "pitch target440=" << target440
              << " side408=" << oldSideband408
              << " side502=" << oldSideband502 << "\n";
    assert (target440 > oldSideband408 * 2.0);
    assert (target440 > oldSideband502 * 2.0);

    std::cout << "core tests passed\n";
}