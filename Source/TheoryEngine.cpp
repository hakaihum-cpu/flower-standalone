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

void baseChord (bool minorContext, int degree,
                std::vector<int>& intervals, std::string& suffix)
{
    if (! minorContext)
    {
        static constexpr int quality[7] = { 0, 1, 1, 0, 0, 1, 2 };
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
}
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

    // Do not treat the incoming note as an implied root. Prefer a chord degree
    // in which the live note is already a chord member, so the dry input can be
    // root/third/fifth/etc. while the generated voices fill the remaining tones.
    std::array<float, 7> weights {};
    bool haveCompatibleDegree = false;
    for (int d = 0; d < 7; ++d)
    {
        if (! degreeContainsAnchor (d))
            continue;
        haveCompatibleDegree = true;
        weights[(size_t) d] = transition[currentDegree][d] + 0.04f;
    }

    if (haveCompatibleDegree)
    {
        std::discrete_distribution<int> dist (weights.begin(), weights.end());
        currentDegree = dist (rng);
    }
    else
    {
        // Chromatic/non-diatonic anchor: stay musically close, then buildPlan()
        // adds that pitch class as a colour tone if the base chord lacks it.
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
    }

    return buildPlan (currentDegree, true);
}

bool TheoryEngine::degreeContainsAnchor (int degree) const
{
    const bool minorContext = mode == Mode::minor;
    const auto& scale = minorContext ? minorScale : majorScale;
    const int rootPc = pitchClass (tonicPitchClass + scale[(size_t) degree]);
    std::vector<int> intervals;
    std::string suffix;
    baseChord (minorContext, degree, intervals, suffix);
    const int anchorPc = pitchClass (lastInputMidi);

    for (const int interval : intervals)
        if (pitchClass (rootPc + interval) == anchorPc)
            return true;
    return false;
}

int TheoryEngine::chooseNextDegree()
{
    std::array<float, 7> weights {};
    bool anyCompatible = false;
    for (int i = 0; i < 7; ++i)
        anyCompatible = anyCompatible || degreeContainsAnchor (i);

    for (int i = 0; i < 7; ++i)
    {
        if (anyCompatible && ! degreeContainsAnchor (i))
        {
            weights[(size_t) i] = 0.0f;
            continue;
        }

        float w = transition[currentDegree][i];
        // COMPLEX increases the chance of less-common transitions, but the
        // current live pitch remains a hard chord-member constraint whenever
        // a compatible diatonic degree exists.
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

    const int anchor = std::clamp (lastInputMidi, 0, 127);
    const int anchorPc = pitchClass (anchor);

    for (size_t i = 0; i < intervals.size(); ++i)
    {
        const int targetPc = pitchClass (rootPc + intervals[i]);
        int delta = pitchClass (targetPc - anchorPc);
        if (delta > 6) delta -= 12;

        int n = anchor + delta;
        while (n < 48) n += 12;   // generated-note floor C3
        while (n > 96) n -= 12;

        // WIDTH opens the chord without forcing every voice into C4-C6 or
        // requiring two-octave pitch shifts from a low live note.
        if (i > 0 && width > 0.58f)
        {
            const bool spread = width > 0.82f || ((i & 1u) != 0u);
            const int candidate = n + (spread ? 12 : 0);
            if (candidate <= 96 && candidate - anchor <= 19)
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
    if (forceAnchorResponse
        && mode == Mode::ambiguous
        && pitchClass (lastInputMidi) == tonicPitchClass
        && degree == 0)
    {
        plan.label = noteName (degreeRootPc) + "5(add9)";
        plan.midiNotes = makeVoicing (degreeRootPc, { 0, 7, 14 });
        return plan;
    }

    // Diatonic quality.
    std::vector<int> intervals;
    std::string suffix;
    baseChord (minorContext, degree, intervals, suffix);
    const auto baseIntervals = intervals;
    const auto baseSuffix = suffix;
    const int baseRootPc = degreeRootPc;

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

    const int anchorPc = pitchClass (lastInputMidi);
    auto containsAnchor = [&] (int rootPc, const std::vector<int>& ivals)
    {
        for (const int interval : ivals)
            if (pitchClass (rootPc + interval) == anchorPc)
                return true;
        return false;
    };

    if (! containsAnchor (plan.rootPitchClass, intervals))
    {
        if (containsAnchor (baseRootPc, baseIntervals))
        {
            plan.rootPitchClass = baseRootPc;
            intervals = baseIntervals;
            suffix = baseSuffix;
            plan.substituted = false;
        }
        else
        {
            // Chromatic input: keep the chosen harmony but add the live pitch
            // class explicitly as a colour tone rather than dropping the anchor.
            intervals.push_back (pitchClass (anchorPc - plan.rootPitchClass));
        }
    }

    plan.label = noteName (plan.rootPitchClass) + suffix;
    plan.midiNotes = makeVoicing (plan.rootPitchClass, intervals);
    return plan;
}
}