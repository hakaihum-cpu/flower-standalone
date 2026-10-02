#include "../Source/TheoryEngine.h"
#include "../Source/YinPitchDetector.h"
#include "../Source/GranularPitchBank.h"
#include "../Source/PsolaHarmonyBank.h"
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
    auto containsPc = [] (const chordfx::ChordPlan& plan, int pc)
    {
        for (int n : plan.midiNotes)
            if (((n % 12) + 12) % 12 == pc) return true;
        return false;
    };

    assert (containsPc (p, 0)); // live C is a chord member
    for (int i=0;i<1000;++i)
    {
        p = theory.advance();
        assert (! p.midiNotes.empty());
        assert (containsPc (p, 0));
        for (int n : p.midiNotes) { assert(n >= 48); assert(n <= 96); }
    }

    // New live note becomes the new hard anchor without being forced to root.
    // While tonality is still ambiguous, a changed note must still survive
    // the bootstrap path as an actual chord member.
    p = theory.noteOn (62); // D
    assert (containsPc (p, 2));

    p = theory.noteOn (64); // E
    assert (containsPc (p, 4));
    for (int i=0;i<200;++i)
    {
        p = theory.advance();
        assert (! p.midiNotes.empty());
        assert (containsPc (p, 4));
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

    // TD-PSOLA must be finite, allocation-free after prepare, and produce
    // useful energy on harmonic-rich monophonic material.
    chordfx::PsolaVoice psola;
    psola.prepare (800);
    double psolaEnergy = 0.0;
    for (int i=0;i<48000;++i)
    {
        double saw = 0.0;
        for (int h=1; h<=12; ++h)
            saw += std::sin (2.0*M_PI*150.0*h*i/48000.0) / h;
        const float x = (float) (saw * 0.12);
        const float y = psola.process (x, 48000.0f/150.0f, 1.5f);
        assert (std::isfinite (y));
        if (i > 12000) psolaEnergy += (double) y * (double) y;
    }
    assert (psolaEnergy > 0.01);

    std::cout << "core tests passed\n";
}