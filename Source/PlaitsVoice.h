#pragma once

#include <JuceHeader.h>
#include <array>

#if JUCE_ANDROID
 #include "plaits/dsp/voice.h"
#endif

class PlaitsSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class PlaitsVoice final : public juce::SynthesiserVoice
{
public:
    struct Params
    {
        float level = 0.18f;
        float attack = 0.003f;
        float decay = 0.16f;
        float sustain = 0.58f;
        float release = 0.12f;
        float cutoff = 6000.0f;
        float resonance = 0.8f;
        int model = 0;
        float harmonics = 0.5f;
        float timbre = 0.5f;
        float morph = 0.5f;
    };

    PlaitsVoice();

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void setCurrentPlaybackSampleRate (double newRate) override;
    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;
    void setParams (const Params& newParams) noexcept;

private:
    static float pitchWheelToSemitones (int value) noexcept;

#if JUCE_ANDROID
    float renderPlaitsOscillator (float midiPitch) noexcept;
#endif

    Params params;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> deClick;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> cutoffSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> resonanceSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> levelSmoothed;

    double sampleRateHz = 44100.0;
    int currentMidiNote = 60;
    float velocityGain = 0.0f;
    float pitchWheelSemitones = 0.0f;

#if JUCE_ANDROID
    alignas (16) std::array<uint8_t, 65536> plaitsWork {};
    plaits::Voice plaitsVoice;
    std::array<plaits::Voice::Frame, plaits::kBlockSize> plaitsFrames {};
    int plaitsFrameIndex = static_cast<int> (plaits::kBlockSize);
    double plaitsStreamPhase = 1.0;
    float plaitsPreviousSample = 0.0f;
    float plaitsNextSample = 0.0f;
    bool plaitsGate = false;
    int activePlaitsModel = -1;
    int pendingPlaitsModel = -1;
    float plaitsModelGain = 1.0f;
    float plaitsHarmonicsCurrent = 0.5f;
    float plaitsTimbreCurrent = 0.5f;
    float plaitsMorphCurrent = 0.5f;
    bool plaitsParamsInitialised = false;
#endif
};
