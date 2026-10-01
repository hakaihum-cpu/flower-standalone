#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace chordfx
{
struct ChordPlan
{
    std::string label;
    std::vector<int> midiNotes;
    int rootPitchClass = 0;
    int scaleDegree = 0;
    bool substituted = false;
};

class TheoryEngine
{
public:
    TheoryEngine();

    void reset();
    void setRandomSeed (std::uint32_t seed);
    void setComplexity (float value01);
    void setWidth (float value01);

    ChordPlan noteOn (int detectedMidiNote);
    ChordPlan advance();

    int getTonicPitchClass() const noexcept { return tonicPitchClass; }
    bool hasTonalCentre() const noexcept { return tonalCentreValid; }

private:
    enum class Mode { ambiguous, major, minor };

    ChordPlan buildPlan (int degree, bool forceAnchorResponse);
    int chooseNextDegree();
    void updateModeEvidence (int midiNote);
    std::vector<int> makeVoicing (int rootPc, const std::vector<int>& intervals) const;
    static int pitchClass (int midi) noexcept;
    static std::string noteName (int pc);

    float complexity = 0.25f;
    float width = 0.35f;
    bool tonalCentreValid = false;
    int tonicPitchClass = 0;
    int currentDegree = 0;
    int lastInputMidi = 60;
    Mode mode = Mode::ambiguous;
    float majorEvidence = 0.0f;
    float minorEvidence = 0.0f;
    std::mt19937 rng;
};
}