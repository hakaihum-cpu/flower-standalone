#include "SynthVoice.h"
#include <cmath>

SineVoice::SineVoice()
{
#if JUCE_ANDROID
    stmlib::BufferAllocator allocator (
        plaitsWork.data(), plaitsWork.size());
    plaitsVoice.Init (&allocator);
#endif
}

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
    currentMidiNote = midiNoteNumber;
#if JUCE_ANDROID
    plaitsGate = true;
    plaitsStreamPhase = 1.0;
    plaitsFrameIndex = static_cast<int> (plaits::kBlockSize);
    plaitsPreviousSample = plaitsNextSample = 0.0f;
#endif
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
#if JUCE_ANDROID
    plaitsGate = false;
#endif
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

#if JUCE_ANDROID
float SineVoice::renderPlaitsOscillator (float midiPitch) noexcept
{
    // Original Plaits runs at its calibrated physical rate in fixed 12-frame
    // blocks.  Host-rate output is linearly interpolated, matching MIYAKO.
    plaitsStreamPhase +=
        static_cast<double> (plaits::kCorrectedSampleRate) / sampleRateHz;

    while (plaitsStreamPhase >= 1.0)
    {
        plaitsStreamPhase -= 1.0;
        plaitsPreviousSample = plaitsNextSample;

        if (plaitsFrameIndex >= static_cast<int> (plaits::kBlockSize))
        {
            plaits::Patch patch {};
            patch.engine = juce::jlimit (0, 23, params.plaitsModel);
            patch.harmonics =
                juce::jlimit (0.0f, 1.0f, params.plaitsHarmonics);
            patch.timbre =
                juce::jlimit (0.0f, 1.0f, params.plaitsTimbre);
            patch.morph =
                juce::jlimit (0.0f, 1.0f, params.plaitsMorph);
            patch.note = 0.0f;
            patch.decay = 0.5f;
            patch.lpg_colour = 0.5f;

            plaits::Modulations mod {};
            mod.note = juce::jlimit (0.0f, 127.0f, midiPitch);
            mod.level = 0.8f;
            mod.trigger = plaitsGate ? 1.0f : 0.0f;
            mod.trigger_patched = true;
            mod.level_patched = true;

            plaitsVoice.Render (
                patch, mod, plaitsFrames.data(), plaits::kBlockSize);
            plaitsFrameIndex = 0;
        }

        plaitsNextSample =
            static_cast<float> (
                plaitsFrames[static_cast<size_t> (plaitsFrameIndex++)].out)
            * (0.55f / 32768.0f);
    }

    return plaitsPreviousSample
        + (plaitsNextSample - plaitsPreviousSample)
            * static_cast<float> (plaitsStreamPhase);
}
#endif

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

        float oscillatorSample = 0.0f;
#if JUCE_ANDROID
        oscillatorSample = renderPlaitsOscillator (
            static_cast<float> (currentMidiNote) + pitchModSemitones);
#else
        oscillatorSample = std::sin (
            juce::MathConstants<float>::twoPi * static_cast<float> (phase));
        phase = wrap01 (phase + frequency / sampleRateHz);
#endif

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
