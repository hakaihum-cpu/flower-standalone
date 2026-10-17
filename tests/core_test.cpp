#include "../Source/TheoryEngine.h"
#include "../Source/YinPitchDetector.h"
#include "../Source/GranularPitchBank.h"
#include "../Source/PsolaHarmonyBank.h"
#include "../Source/ChordRenderers.h"
#include "../Source/VideoInteraction.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    // Actual audio pitch, not a MIDI note, drives the Vongon video grid.
    assert (effects_video::divisionsForPitchHz (0.0f) == 1);
    assert (effects_video::divisionsForPitchHz (74.0f) == 1);
    assert (effects_video::divisionsForPitchHz (75.0f) == 2);
    assert (effects_video::divisionsForPitchHz (1100.0f) == 10);
    assert (effects_video::divisionsForPitchHz (6000.0f) == 10);
    int previousDivisions = 1;
    for (float hz : {75.0f, 110.0f, 220.0f, 440.0f, 880.0f, 1100.0f})
    {
        const int divisions = effects_video::divisionsForPitchHz (hz);
        assert (divisions > previousDivisions);
        previousDivisions = divisions;
    }
    // The independent downsampled YIN path recognizes a real 220 Hz input.
    chordfx::YinPitchDetector videoPitch;
    videoPitch.prepare (12000.0, 1024, 512);
    int goodEstimates = 0;
    for (int i = 0; i < 16000; ++i)
    {
        const float audio = 0.5f * std::sin (
            2.0f * 3.14159265358979323846f * 220.0f * (float) i / 12000.0f);
        const auto estimate = videoPitch.pushSample (audio);
        if (estimate.valid)
        {
            assert (std::abs (estimate.hz - 220.0f) < 8.0f);
            assert (effects_video::divisionsForPitchHz (estimate.hz) >= 2);
            ++goodEstimates;
        }
    }
    assert (goodEstimates >= 2);
    assert (effects_video::vongonModeForXY (0.25f, 0.75f) == 5);
    assert (effects_video::vongonModeForXY (0.75f, 0.75f) == 6);
    assert (effects_video::vongonModeForXY (0.25f, 0.25f) == 7);
    assert (effects_video::vongonModeForXY (0.75f, 0.25f) == 8);
    assert (std::abs (effects_video::localX (0.75f) - 0.5f) < 1.0e-6f);
    assert (std::abs (effects_video::localY (0.25f) - 0.5f) < 1.0e-6f);
    // Each tile gets a complete video frame, not a quadrant of one
    // fullscreen image. Order is NOT hardcoded from a user's example.
    bool layoutsDiffer = false;
    for (unsigned int event = 1; event <= 32; ++event)
    {
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 2; ++col)
            {
                const int kind = effects_video::tileSourceVariant (
                    row, col, 2, event);
                assert (kind >= 0 && kind <= 3);
                if (effects_video::tileSourceVariant (
                        row, col, 2, 1u) != kind)
                    layoutsDiffer = true;
            }
    }
    assert (layoutsDiffer);
    // Occasional 2x2 mosaics repeat A in every cell.
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 2; ++col)
            assert (effects_video::tileSourceVariant (
                row, col, 2, 8u) == 0);
    // 10x10 has 100 independently selected destinations, with sources
    // in range; neither clip nor playback offsets are fixed.
    for (int row = 0; row < 10; ++row)
        for (int col = 0; col < 10; ++col)
            assert (effects_video::tileSourceVariant (
                row, col, 10, 3u) < 4);
    for (int slot = 0; slot < 2; ++slot)
    {
        const int clipOffset = effects_video::tileAlternateClipOffset (
            slot, 9, 3u);
        assert (clipOffset > 0 && clipOffset < 9);
    }
    assert (effects_video::tileAlternateSeek (2, 192, 3u) > 0);
    assert (effects_video::tileAlternateSeek (2, 192, 3u) < 192);

    // CHORD-B equal-temperament tuning regression.  The generated sine stays
    // exactly at A4=440 Hz when the source is in tune, and follows the live
    // source's fractional-semitone offset without changing chord pitch class.
    assert (std::abs (
        chordfx::SineArpeggiator::midiFrequency (69.0) - 440.0) < 1.0e-9);
    const double quarterSemitone =
        440.0 * std::pow (2.0, 0.25 / 12.0);
    assert (std::abs (
        chordfx::SineArpeggiator::midiFrequency (69.0, 0.25)
        - quarterSemitone) < 1.0e-9);

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

    // CHORDBOT: C MAJ seed produces eight distinct theory-ranked
    // candidates and includes the strong dominant V7 option.
    const auto botMajor =
        chordfx::TheoryEngine::chordBotSuggestions (0, 0);
    bool foundG7 = false;
    for (size_t i = 0; i < botMajor.size(); ++i)
    {
        assert (botMajor[i].rootPitchClass >= 0);
        assert (botMajor[i].rootPitchClass < 12);
        assert (botMajor[i].quality >= 0);
        assert (botMajor[i].quality < 8);
        assert (! (botMajor[i].rootPitchClass == 0
                   && botMajor[i].quality == 0));
        if (botMajor[i].rootPitchClass == 7
            && botMajor[i].quality == 2)
            foundG7 = true;

        for (size_t j = 0; j < i; ++j)
            assert (botMajor[i].rootPitchClass
                        != botMajor[j].rootPitchClass
                    || botMajor[i].quality
                        != botMajor[j].quality);
    }
    assert (foundG7);

    // A MIN seed exposes the harmonic-minor E7 dominant.
    const auto botMinor =
        chordfx::TheoryEngine::chordBotSuggestions (9, 1);
    bool foundE7 = false;
    for (const auto& choice : botMinor)
        if (choice.rootPitchClass == 4 && choice.quality == 2)
            foundE7 = true;
    assert (foundE7);

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