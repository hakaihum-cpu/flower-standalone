#include "../Source/TheoryEngine.h"
#include "../Source/YinPitchDetector.h"
#include "../Source/GranularPitchBank.h"
#include <cassert>
#include <cmath>
#include <iostream>

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
    bank.setTargetRatios({1.0f, 1.5f});
    double energy=0.0;
    for(int i=0;i<5000;++i)
    {
        float s=0.25f*std::sin(2.0*M_PI*220.0*i/48000.0);
        float y=bank.processSample(s); energy += y*y;
    }
    assert(energy > 0.01);
    std::cout << "core tests passed\n";
}