#include "../Source/TheoryEngine.h"
#include "../Source/YinPitchDetector.h"
#include "../Source/GranularPitchBank.h"
#include "../Source/PsolaHarmonyBank.h"
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
        for (int n : p.midiNotes) { assert(n >= 60); assert(n <= 83); }
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

    // Fresh G must anchor to G, never a fixed +1-semitone G#/Ab plan.
    chordfx::TheoryEngine theoryG;
    theoryG.setRandomSeed (1);
    theoryG.setComplexity (0.25f);
    theoryG.setWidth (0.35f);
    auto gPlan = theoryG.noteOn (67); // G
    assert (containsPc (gPlan, 7));
    assert (! containsPc (gPlan, 8));

    // Chromatic anchor must also remain present as a colour tone.
    p = theory.noteOn (61); // Db
    assert (containsPc (p, 1));
    for (int i=0;i<80;++i)
    {
        p = theory.advance();
        assert (containsPc (p, 1));
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

    yin.reset();
    chordfx::PitchEstimate gEst;
    constexpr double g4 = 391.99543598174927;
    for (int i=0;i<48000/2;++i)
    {
        float s=0.25f*std::sin(2.0*M_PI*g4*i/48000.0);
        auto e=yin.pushSample(s); if(e.valid) gEst=e;
    }
    assert(gEst.valid && std::abs(gEst.hz-(float)g4) < 4.0f);
    const int gMidi = (int) std::lround (
        69.0 + 12.0 * std::log2 ((double) gEst.hz / 440.0));
    assert (gMidi == 67);

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
    std::vector<float> psolaTail;
    psolaTail.reserve (24000);
    for (int i=0;i<72000;++i)
    {
        double saw = 0.0;
        for (int h=1; h<=12; ++h)
            saw += std::sin (2.0*M_PI*150.0*h*i/48000.0) / h;
        const float x = (float) (saw * 0.12);
        const float y = psola.process (x, 48000.0f/150.0f, 1.5f);
        assert (std::isfinite (y));
        if (i >= 48000)
        {
            psolaEnergy += (double) y * (double) y;
            psolaTail.push_back (y);
        }
    }
    assert (psolaEnergy > 0.01);

    auto toneMagnitude = [&psolaTail] (double hz)
    {
        double re = 0.0, im = 0.0;
        for (size_t i=0; i<psolaTail.size(); ++i)
        {
            const double ph = 2.0*M_PI*hz*(double)i/48000.0;
            re += psolaTail[i] * std::cos (ph);
            im -= psolaTail[i] * std::sin (ph);
        }
        return std::sqrt (re*re + im*im);
    };
    assert (toneMagnitude (225.0) > toneMagnitude (150.0) * 1.2);

    std::cout << "core tests passed\n";
}