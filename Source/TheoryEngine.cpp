#include "TheoryEngine.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace chordfx
{
namespace
{
constexpr std::array<int, 7> majorScale { 0, 2, 4, 5, 7, 9, 11 };
constexpr std::array<int, 7> minorScale { 0, 2, 3, 5, 7, 8, 10 };

constexpr float transition[7][7] = {
    { 0.05f, 0.08f, 0.07f, 0.22f, 0.28f, 0.22f, 0.08f }, // I
    { 0.08f, 0.04f, 0.08f, 0.18f, 0.40f, 0.14f, 0.08f }, // ii
    { 0.12f, 0.08f, 0.04f, 0.16f, 0.22f, 0.28f, 0.10f }, // iii
    { 0.22f, 0.14f, 0.06f, 0.05f, 0.32f, 0.15f, 0.06f }, // IV
    { 0.48f, 0.10f, 0.04f, 0.14f, 0.04f, 0.16f, 0.04f }, // V
    { 0.18f, 0.14f, 0.10f, 0.28f, 0.20f, 0.04f, 0.06f }, // vi
    { 0.34f, 0.08f, 0.04f, 0.12f, 0.30f, 0.08f, 0.04f }  // vii
};

float clamp01 (float v) { return std::max (0.0f, std::min (1.0f, v)); }
}

TheoryEngine::TheoryEngine() : rng (0x43485244u) {}

void TheoryEngine::reset()
{
    tonalCentreValid = false;
    tonicPitchClass = 0;
    currentDegree = 0;
    lastInputMidi = 60;
    mode = Mode::ambiguous;
    majorEvidence = 0.0f;
    minorEvidence = 0.0f;
}

void TheoryEngine::setRandomSeed (std::uint32_t seed) { rng.seed (seed); }
void TheoryEngine::setComplexity (float v) { complexity = clamp01 (v); }
void TheoryEngine::setWidth (float v) { width = clamp01 (v); }
int TheoryEngine::pitchClass (int m) noexcept { return (m % 12 + 12) % 12; }

std::string TheoryEngine::noteName (int pc)
{
    static constexpr const char* names[] = {
        "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
    };
    return names[pitchClass (pc)];
}

void TheoryEngine::updateModeEvidence (int midiNote)
{
    if (! tonalCentreValid)
        return;

    const int rel = pitchClass (midiNote - tonicPitchClass);
    if (rel == 4 || rel == 9 || rel == 11) majorEvidence += 1.0f;
    if (rel == 3 || rel == 8 || rel == 10) minorEvidence += 1.0f;
    if (rel == 2 || rel == 5 || rel == 7) { majorEvidence += 0.12f; minorEvidence += 0.12f; }

    if (majorEvidence > minorEvidence + 0.75f) mode = Mode::major;
    else if (minorEvidence > majorEvidence + 0.75f) mode = Mode::minor;
}

ChordPlan TheoryEngine::noteOn (int detectedMidiNote)
{
    lastInputMidi = std::clamp (detectedMidiNote, 0, 127);
    if (! tonalCentreValid)
    {
        tonicPitchClass = pitchClass (lastInputMidi);
        tonalCentreValid = true;
        currentDegree = 0;
        mode = Mode::ambiguous;
        return buildPlan (0, true);
    }

    updateModeEvidence (lastInputMidi);

    const auto& scale = mode == Mode::minor ? minorScale : majorScale;
    const int rel = pitchClass (lastInputMidi - tonicPitchClass);
    int bestDegree = 0;
    int bestDistance = 99;
    for (int d = 0; d < 7; ++d)
    {
        int dist = std::abs (scale[(size_t) d] - rel);
        dist = std::min (dist, 12 - dist);
        if (dist < bestDistance) { bestDistance = dist; bestDegree = d; }
    }
    currentDegree = bestDegree;
    return buildPlan (currentDegree, true);
}

int TheoryEngine::chooseNextDegree()
{
    std::array<float, 7> weights {};
    for (int i = 0; i < 7; ++i)
    {
        float w = transition[currentDegree][i];
        // More complex settings permit less conventional jumps without making them equiprobable.
        const float adventurous = 0.035f + 0.11f * complexity;
        weights[(size_t) i] = w * (1.0f - 0.28f * complexity) + adventurous;
    }
    std::discrete_distribution<int> dist (weights.begin(), weights.end());
    return dist (rng);
}

ChordPlan TheoryEngine::advance()
{
    if (! tonalCentreValid)
        return {};
    currentDegree = chooseNextDegree();
    return buildPlan (currentDegree, false);
}

std::vector<int> TheoryEngine::makeVoicing (int rootPc, const std::vector<int>& intervals) const
{
    std::vector<int> notes;
    notes.reserve (intervals.size());

    // Build the chord around the actual input register instead of forcing every
    // harmony into the C4-C6 area. Large upward transpositions were driving the
    // granular shifter into its extreme ratio limit and producing chipmunk/robotic
    // output. Keep each pitch class near the live input, while preserving the
    // existing hard floor at C3.
    const int anchor = std::clamp (lastInputMidi, 0, 127);
    const int anchorPc = pitchClass (anchor);

    for (size_t i = 0; i < intervals.size(); ++i)
    {
        const int targetPc = pitchClass (rootPc + intervals[i]);
        int delta = pitchClass (targetPc - anchorPc);
        if (delta > 6)
            delta -= 12;

        int n = anchor + delta;

        while (n < 48) n += 12; // hard floor C3
        while (n > 96) n -= 12;

        // WIDTH may spread upper voices by one octave, but never by more than
        // one octave from the live input when the C3 floor allows it.
        if (width > 0.55f && i >= 1)
        {
            const bool spreadThisVoice = (width > 0.82f) || ((i & 1u) != 0u);
            const int candidate = n + (spreadThisVoice ? 12 : 0);
            if (candidate <= 96 && candidate - anchor <= 12)
                n = candidate;
        }

        notes.push_back (std::clamp (n, 48, 96));
    }

    std::sort (notes.begin(), notes.end());
    notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
    return notes;
}

ChordPlan TheoryEngine::buildPlan (int degree, bool forceAnchorResponse)
{
    const bool minorContext = mode == Mode::minor;
    const auto& scale = minorContext ? minorScale : majorScale;
    const int degreeRootPc = pitchClass (tonicPitchClass + scale[(size_t) degree]);

    ChordPlan plan;
    plan.scaleDegree = degree;
    plan.rootPitchClass = degreeRootPc;

    // First note is intentionally third-less: one note cannot establish major/minor.
    if (forceAnchorResponse && mode == Mode::ambiguous)
    {
        plan.label = noteName (degreeRootPc) + "5(add9)";
        plan.midiNotes = makeVoicing (degreeRootPc, { 0, 7, 14 });
        return plan;
    }

    // Diatonic quality.
    std::vector<int> intervals;
    std::string suffix;
    if (! minorContext)
    {
        static constexpr int quality[7] = { 0, 1, 1, 0, 0, 1, 2 }; // maj,min,dim
        const int q = quality[degree];
        intervals = q == 0 ? std::vector<int>{0,4,7}
                           : q == 1 ? std::vector<int>{0,3,7}
                                    : std::vector<int>{0,3,6};
        suffix = q == 0 ? "" : q == 1 ? "m" : "dim";
    }
    else
    {
        static constexpr int quality[7] = { 1, 2, 0, 1, 1, 0, 0 };
        const int q = quality[degree];
        intervals = q == 0 ? std::vector<int>{0,4,7}
                           : q == 1 ? std::vector<int>{0,3,7}
                                    : std::vector<int>{0,3,6};
        suffix = q == 0 ? "" : q == 1 ? "m" : "dim";
    }

    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    const float r = uni (rng);

    // Complexity layers: seventh/add9 -> borrowed/secondary -> substitutions/alterations.
    if (complexity > 0.22f && r < 0.50f + complexity * 0.25f)
    {
        if (suffix == "m") { intervals.push_back (10); suffix = "m7"; }
        else if (suffix.empty()) { intervals.push_back (degree == 4 ? 10 : 11); suffix = degree == 4 ? "7" : "maj7"; }
    }

    if (complexity > 0.42f && uni (rng) < 0.32f * complexity)
    {
        if (intervals.size() < 4) intervals.push_back (14);
        suffix += "(add9)";
    }

    // Borrowed iv / bVI / bVII colours.
    if (complexity > 0.62f && uni (rng) < (complexity - 0.55f) * 0.42f)
    {
        if (degree == 3 && ! minorContext)
        {
            intervals = {0,3,7,9}; suffix = "m6"; plan.substituted = true;
        }
        else if (degree == 5 && ! minorContext)
        {
            plan.rootPitchClass = pitchClass (tonicPitchClass + 8);
            intervals = {0,4,7,11}; suffix = "maj7"; plan.substituted = true;
        }
    }

    // At the top end, dominant-function chords may become V7alt or tritone substitute.
    if (complexity > 0.82f && degree == 4 && uni (rng) < (complexity - 0.75f) * 1.5f)
    {
        if (uni (rng) < 0.5f)
        {
            intervals = {0,4,10,13}; suffix = "7(b9)"; plan.substituted = true;
        }
        else
        {
            plan.rootPitchClass = pitchClass (tonicPitchClass + 1); // bII7 tritone substitute
            intervals = {0,4,7,10}; suffix = "7"; plan.substituted = true;
        }
    }

    plan.label = noteName (plan.rootPitchClass) + suffix;
    plan.midiNotes = makeVoicing (plan.rootPitchClass, intervals);
    return plan;
}
}