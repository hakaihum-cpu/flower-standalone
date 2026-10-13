#include "PlaitsVoice.h"
#include <cmath>

PlaitsVoice::PlaitsVoice()
{
#if JUCE_ANDROID
    stmlib::BufferAllocator allocator (
        plaitsWork.data(), plaitsWork.size());
    plaitsVoice.Init (&allocator);
#endif
}

bool PlaitsVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<PlaitsSound*> (sound) != nullptr;
}

void PlaitsVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    sampleRateHz = newRate > 0.0 ? newRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRateHz;
    spec.maximumBlockSize = 8192;
    spec.numChannels = 1;
    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.reset();

    adsr.setSampleRate (sampleRateHz);
    deClick.reset (sampleRateHz, 0.003);

    cutoffSmoothed.reset (sampleRateHz, 0.020);
    resonanceSmoothed.reset (sampleRateHz, 0.020);
    levelSmoothed.reset (sampleRateHz, 0.015);
    cutoffSmoothed.setCurrentAndTargetValue (params.cutoff);
    resonanceSmoothed.setCurrentAndTargetValue (params.resonance);
    levelSmoothed.setCurrentAndTargetValue (params.level);
}

void PlaitsVoice::startNote (int midiNoteNumber,
                             float velocity,
                             juce::SynthesiserSound*,
                             int currentPitchWheelPosition)
{
    currentMidiNote = midiNoteNumber;
    velocityGain = juce::jlimit (0.0f, 1.0f, velocity);
    pitchWheelSemitones = pitchWheelToSemitones (currentPitchWheelPosition);

#if JUCE_ANDROID
    plaitsGate = true;
    plaitsStreamPhase = 1.0;
    plaitsFrameIndex = static_cast<int> (plaits::kBlockSize);
    plaitsPreviousSample = plaitsNextSample = 0.0f;
    activePlaitsModel = juce::jlimit (0, 23, params.model);
    pendingPlaitsModel = -1;
    plaitsModelGain = 1.0f;
    plaitsHarmonicsCurrent = juce::jlimit (0.0f, 1.0f, params.harmonics);
    plaitsTimbreCurrent = juce::jlimit (0.0f, 1.0f, params.timbre);
    plaitsMorphCurrent = juce::jlimit (0.0f, 1.0f, params.morph);
    plaitsParamsInitialised = true;
#endif

    filter.reset();
    adsr.noteOn();
    deClick.setCurrentAndTargetValue (0.0f);
    deClick.setTargetValue (1.0f);
}

void PlaitsVoice::stopNote (float, bool allowTailOff)
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

void PlaitsVoice::pitchWheelMoved (int newPitchWheelValue)
{
    pitchWheelSemitones = pitchWheelToSemitones (newPitchWheelValue);
}

void PlaitsVoice::setParams (const Params& newParams) noexcept
{
    params = newParams;

    adsrParams.attack = params.attack;
    adsrParams.decay = params.decay;
    adsrParams.sustain = params.sustain;
    adsrParams.release = params.release;
    adsr.setParameters (adsrParams);

    cutoffSmoothed.setTargetValue (
        juce::jlimit (20.0f, 20000.0f, params.cutoff));
    resonanceSmoothed.setTargetValue (
        juce::jlimit (0.1f, 12.0f, params.resonance));
    levelSmoothed.setTargetValue (
        juce::jlimit (0.0f, 1.0f, params.level));
}

#if JUCE_ANDROID
float PlaitsVoice::renderPlaitsOscillator (float midiPitch) noexcept
{
    const int targetModel = juce::jlimit (0, 23, params.model);

    if (activePlaitsModel < 0)
        activePlaitsModel = targetModel;

    if (targetModel != activePlaitsModel)
        pendingPlaitsModel = targetModel;
    else
        pendingPlaitsModel = -1;

    const float modelFadeStep = 1.0f / juce::jmax (
        1.0f, static_cast<float> (sampleRateHz * 0.006));

    if (pendingPlaitsModel >= 0)
    {
        plaitsModelGain = juce::jmax (
            0.0f, plaitsModelGain - modelFadeStep);

        if (plaitsModelGain <= 0.00001f)
        {
            activePlaitsModel = pendingPlaitsModel;
            pendingPlaitsModel = -1;
            plaitsFrameIndex = static_cast<int> (plaits::kBlockSize);
            plaitsStreamPhase = 1.0;
            plaitsPreviousSample = 0.0f;
            plaitsNextSample = 0.0f;
        }
    }
    else
    {
        plaitsModelGain = juce::jmin (
            1.0f, plaitsModelGain + modelFadeStep);
    }

    plaitsStreamPhase +=
        static_cast<double> (plaits::kCorrectedSampleRate) / sampleRateHz;

    while (plaitsStreamPhase >= 1.0)
    {
        plaitsStreamPhase -= 1.0;
        plaitsPreviousSample = plaitsNextSample;

        if (plaitsFrameIndex >= static_cast<int> (plaits::kBlockSize))
        {
            if (! plaitsParamsInitialised)
            {
                plaitsHarmonicsCurrent =
                    juce::jlimit (0.0f, 1.0f, params.harmonics);
                plaitsTimbreCurrent =
                    juce::jlimit (0.0f, 1.0f, params.timbre);
                plaitsMorphCurrent =
                    juce::jlimit (0.0f, 1.0f, params.morph);
                plaitsParamsInitialised = true;
            }
            else
            {
                constexpr float smooth = 0.22f;
                plaitsHarmonicsCurrent += (
                    juce::jlimit (0.0f, 1.0f, params.harmonics)
                    - plaitsHarmonicsCurrent) * smooth;
                plaitsTimbreCurrent += (
                    juce::jlimit (0.0f, 1.0f, params.timbre)
                    - plaitsTimbreCurrent) * smooth;
                plaitsMorphCurrent += (
                    juce::jlimit (0.0f, 1.0f, params.morph)
                    - plaitsMorphCurrent) * smooth;
            }

            plaits::Patch patch {};
            patch.engine = activePlaitsModel;
            patch.harmonics = plaitsHarmonicsCurrent;
            patch.timbre = plaitsTimbreCurrent;
            patch.morph = plaitsMorphCurrent;
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

    const float interpolated =
        plaitsPreviousSample
        + (plaitsNextSample - plaitsPreviousSample)
            * static_cast<float> (plaitsStreamPhase);

    return interpolated * plaitsModelGain;
}
#endif

void PlaitsVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                   int startSample,
                                   int numSamples)
{
    if (! isVoiceActive() || sampleRateHz <= 0.0)
        return;

    auto* left = outputBuffer.getWritePointer (0);
    auto* right = outputBuffer.getNumChannels() > 1
        ? outputBuffer.getWritePointer (1)
        : nullptr;

    const float cutoffTarget =
        juce::jlimit (20.0f,
                      static_cast<float> (sampleRateHz * 0.45),
                      cutoffSmoothed.getTargetValue());
    const float resonanceTarget =
        juce::jlimit (0.1f, 12.0f, resonanceSmoothed.getTargetValue());

    filter.setCutoffFrequency (cutoffTarget);
    filter.setResonance (resonanceTarget);

    for (int i = 0; i < numSamples; ++i)
    {
        const float cutoff = cutoffSmoothed.getNextValue();
        const float resonance = resonanceSmoothed.getNextValue();
        juce::ignoreUnused (cutoff, resonance);

        float oscillatorSample = 0.0f;
#if JUCE_ANDROID
        oscillatorSample = renderPlaitsOscillator (
            static_cast<float> (currentMidiNote) + pitchWheelSemitones);
#endif

        float sample = filter.processSample (0, oscillatorSample);
        sample *= adsr.getNextSample()
               * velocityGain
               * levelSmoothed.getNextValue()
               * deClick.getNextValue();

        left[startSample + i] += sample;
        if (right != nullptr)
            right[startSample + i] += sample;
    }

    filter.snapToZero();

    if (! adsr.isActive())
        clearCurrentNote();
}

float PlaitsVoice::pitchWheelToSemitones (int value) noexcept
{
    constexpr float bendRange = 2.0f;
    const auto normalized = juce::jlimit (
        -1.0f, 1.0f,
        (static_cast<float> (value) - 8192.0f) / 8192.0f);
    return normalized * bendRange;
}
