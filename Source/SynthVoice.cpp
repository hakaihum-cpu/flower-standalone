#include "SynthVoice.h"
#include <cmath>

bool SineVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SineSound*> (sound) != nullptr;
}

void SineVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    sampleRateHz = newRate > 0.0 ? newRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRateHz;
    spec.maximumBlockSize = 2048;
    spec.numChannels = 1;
    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.reset();

    adsr.setSampleRate (sampleRateHz);
    deClick.reset (sampleRateHz, 0.003);
}

void SineVoice::startNote (int midiNoteNumber,
                           float velocity,
                           juce::SynthesiserSound*,
                           int currentPitchWheelPosition)
{
    baseFrequencyHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    velocityGain = juce::jlimit (0.0f, 1.0f, velocity);
    pitchWheelSemitones = pitchWheelToSemitones (currentPitchWheelPosition);
    phase = 0.0;
    lfoPhase = 0.0;
    lfoCurrent = 0.0f;
    filter.reset();
    adsr.noteOn();
    deClick.setCurrentAndTargetValue (0.0f);
    deClick.setTargetValue (1.0f);
}

void SineVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
        adsr.noteOff();
    else
    {
        adsr.reset();
        deClick.setCurrentAndTargetValue (0.0f);
        clearCurrentNote();
    }
}

void SineVoice::pitchWheelMoved (int newPitchWheelValue)
{
    pitchWheelSemitones = pitchWheelToSemitones (newPitchWheelValue);
}

void SineVoice::setParams (const Params& newParams) noexcept
{
    params = newParams;
    adsrParams.attack = params.attack;
    adsrParams.decay = params.decay;
    adsrParams.sustain = params.sustain;
    adsrParams.release = params.release;
    adsr.setParameters (adsrParams);
}

float SineVoice::nextLfo() noexcept
{
    lfoCurrent = std::sin (juce::MathConstants<float>::twoPi * static_cast<float> (lfoPhase));
    const auto increment = juce::jlimit (0.01, 20.0, static_cast<double> (params.lfoRate)) / sampleRateHz;
    lfoPhase = wrap01 (lfoPhase + increment);
    return lfoCurrent;
}

float SineVoice::modulationFor (LfoTarget target) const noexcept
{
    if (params.lfoTarget != static_cast<int> (target))
        return 0.0f;
    return juce::jlimit (-1.0f, 1.0f, lfoCurrent * params.lfoDepth);
}

void SineVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                 int startSample,
                                 int numSamples)
{
    if (! isVoiceActive() || sampleRateHz <= 0.0)
        return;

    auto* left = outputBuffer.getWritePointer (0);
    auto* right = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        nextLfo();

        const float pitchModSemitones = pitchWheelSemitones + modulationFor (LfoTarget::Pitch) * 2.0f;
        const double frequency = baseFrequencyHz * std::pow (2.0, pitchModSemitones / 12.0);

        const float oscillatorSample = std::sin (juce::MathConstants<float>::twoPi * static_cast<float> (phase));
        phase = wrap01 (phase + frequency / sampleRateHz);

        float cutoff = params.cutoff * std::pow (2.0f, modulationFor (LfoTarget::Cutoff) * 3.0f);
        cutoff = juce::jlimit (20.0f, static_cast<float> (sampleRateHz * 0.45), cutoff);
        const float resonance = juce::jlimit (0.1f, 12.0f,
                                              params.resonance + modulationFor (LfoTarget::Resonance) * 5.0f);
        filter.setCutoffFrequency (cutoff);
        filter.setResonance (resonance);

        float sample = filter.processSample (0, oscillatorSample);
        const float levelMod = juce::jlimit (0.0f, 2.0f,
                                             1.0f + modulationFor (LfoTarget::Level) * 0.90f);
        sample *= adsr.getNextSample() * velocityGain * params.level * levelMod * deClick.getNextValue();

        left[startSample + i] += sample;
        if (right != nullptr)
            right[startSample + i] += sample;
    }

    filter.snapToZero();

    if (! adsr.isActive())
        clearCurrentNote();
}

float SineVoice::pitchWheelToSemitones (int value) noexcept
{
    constexpr float bendRange = 2.0f;
    const auto normalized = juce::jlimit (-1.0f, 1.0f,
        (static_cast<float> (value) - 8192.0f) / 8192.0f);
    return normalized * bendRange;
}

double SineVoice::wrap01 (double value) noexcept
{
    value -= std::floor (value);
    return value;
}
