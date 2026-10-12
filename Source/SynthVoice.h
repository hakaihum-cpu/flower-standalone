#pragma once

#include <JuceHeader.h>
#include "Modulation.h"
#include <array>
#if JUCE_ANDROID
 #include "plaits/dsp/voice.h"
#endif

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

        // FLOWER performance oscillator: original Mutable Instruments Plaits.
        int plaitsModel = 0;
        float plaitsHarmonics = 0.5f;
        float plaitsTimbre = 0.5f;
        float plaitsMorph = 0.5f;
    };

    SineVoice();
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
#if JUCE_ANDROID
    float renderPlaitsOscillator (float midiPitch) noexcept;
#endif

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
    int currentMidiNote = 60;
    float velocityGain = 0.0f;
    float pitchWheelSemitones = 0.0f;

#if JUCE_ANDROID
    // Same preallocated hosted Plaits strategy as accepted MIYAKO.
    alignas (16) std::array<uint8_t, 65536> plaitsWork {};
    plaits::Voice plaitsVoice;
    std::array<plaits::Voice::Frame, plaits::kBlockSize> plaitsFrames {};
    int plaitsFrameIndex = static_cast<int> (plaits::kBlockSize);
    double plaitsStreamPhase = 1.0;
    float plaitsPreviousSample = 0.0f;
    float plaitsNextSample = 0.0f;
    bool plaitsGate = false;
#endif
};
