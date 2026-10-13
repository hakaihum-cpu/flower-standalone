#pragma once

#include <JuceHeader.h>
#include "Modulation.h"

class SineSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SineVoice final : public juce::SynthesiserVoice
{
public:
    struct Params
    {
        float level = 0.25f;
        float attack = 0.01f;
        float decay = 0.20f;
        float sustain = 0.80f;
        float release = 0.40f;
        float cutoff = 3000.0f;
        float resonance = 0.7071f;
        float lfoRate = 1.0f;
        float lfoDepth = 0.0f;
        int lfoTarget = static_cast<int> (LfoTarget::None);
    };

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void setCurrentPlaybackSampleRate (double newRate) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    void setParams (const Params& newParams) noexcept;

private:
    static float pitchWheelToSemitones (int value) noexcept;
    static double wrap01 (double value) noexcept;
    float nextLfo() noexcept;
    float modulationFor (LfoTarget target) const noexcept;

    Params params;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> deClick;

    double phase = 0.0;
    double lfoPhase = 0.0;
    float lfoCurrent = 0.0f;
    double baseFrequencyHz = 440.0;
    double sampleRateHz = 44100.0;
    float velocityGain = 0.0f;
    float pitchWheelSemitones = 0.0f;
};
