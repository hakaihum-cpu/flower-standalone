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

    cutoffSmoothed.reset (sampleRateHz, 0.018);
    resonanceSmoothed.reset (sampleRateHz, 0.018);
    levelSmoothed.reset (sampleRateHz, 0.012);
    cutoffSmoothed.setCurrentAndTargetValue (params.cutoff);
    resonanceSmoothed.setCurrentAndTargetValue (params.resonance);
    levelSmoothed.setCurrentAndTargetValue (params.level);
    controlUpdateCounter = 0;
}

void SineVoice::startNote (int midiNoteNumber,
                           float velocity,
                           juce::SynthesiserSound*,
                           int currentPitchWheelPosition)
{
    baseFrequencyHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    heldFrequency = baseFrequencyHz;
    heldPitchModSemitones = pitchWheelToSemitones (currentPitchWheelPosition);
    currentMidiNote = midiNoteNumber;
#if JUCE_ANDROID
    plaitsGate = true;
    plaitsStreamPhase = 1.0;
    plaitsFrameIndex = static_cast<int> (plaits::kBlockSize);
    plaitsPreviousSample = plaitsNextSample = 0.0f;
    if (activePlaitsModel < 0)
        activePlaitsModel = juce::jlimit (0, 23, params.plaitsModel);
    pendingPlaitsModel = -1;
    plaitsModelGain = 1.0f;
    plaitsHarmonicsCurrent =
        juce::jlimit (0.0f, 1.0f, params.plaitsHarmonics);
    plaitsTimbreCurrent =
        juce::jlimit (0.0f, 1.0f, params.plaitsTimbre);
    plaitsMorphCurrent =
        juce::jlimit (0.0f, 1.0f, params.plaitsMorph);
    plaitsParamsInitialised = true;
#endif
    velocityGain = juce::jlimit (0.0f, 1.0f, velocity);
    pitchWheelSemitones = pitchWheelToSemitones (currentPitchWheelPosition);
    phase = 0.0;
    lfoPhase = 0.0;
    lfoCurrent = 0.0f;
    controlUpdateCounter = 0;
    lfoUpdateCounter = 0;
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

    cutoffSmoothed.setTargetValue (
        juce::jlimit (20.0f, 20000.0f, params.cutoff));
    resonanceSmoothed.setTargetValue (
        juce::jlimit (0.1f, 12.0f, params.resonance));
    levelSmoothed.setTargetValue (
        juce::jlimit (0.0f, 1.0f, params.level));
}

float SineVoice::nextLfo() noexcept
{
    // LFO tops out at 20 Hz.  A held 8-sample control value is transparent at
    // audio rates but avoids one sin() call per voice per sample on Android.
    if (lfoUpdateCounter <= 0)
    {
        lfoCurrent = std::sin (
            juce::MathConstants<float>::twoPi
            * static_cast<float> (lfoPhase));
        lfoUpdateCounter = controlUpdateInterval;
    }

    const auto increment =
        juce::jlimit (
            0.01, 20.0, static_cast<double> (params.lfoRate))
        / sampleRateHz;
    lfoPhase = wrap01 (lfoPhase + increment);
    --lfoUpdateCounter;
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
    const int targetModel = juce::jlimit (0, 23, params.plaitsModel);

    if (activePlaitsModel < 0)
        activePlaitsModel = targetModel;

    if (targetModel != activePlaitsModel)
        pendingPlaitsModel = targetModel;
    else
        pendingPlaitsModel = -1;

    // Original Plaits resets an engine when its model changes.  Do not expose
    // that discontinuity directly: fade the voice to zero, switch/reset while
    // silent, then fade it back in.  Six milliseconds is short enough to keep
    // XY movement immediate without producing a hard step.
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
            if (! plaitsParamsInitialised)
            {
                plaitsHarmonicsCurrent =
                    juce::jlimit (0.0f, 1.0f, params.plaitsHarmonics);
                plaitsTimbreCurrent =
                    juce::jlimit (0.0f, 1.0f, params.plaitsTimbre);
                plaitsMorphCurrent =
                    juce::jlimit (0.0f, 1.0f, params.plaitsMorph);
                plaitsParamsInitialised = true;
            }
            else
            {
                constexpr float smooth = 0.22f;
                plaitsHarmonicsCurrent += (
                    juce::jlimit (0.0f, 1.0f, params.plaitsHarmonics)
                    - plaitsHarmonicsCurrent) * smooth;
                plaitsTimbreCurrent += (
                    juce::jlimit (0.0f, 1.0f, params.plaitsTimbre)
                    - plaitsTimbreCurrent) * smooth;
                plaitsMorphCurrent += (
                    juce::jlimit (0.0f, 1.0f, params.plaitsMorph)
                    - plaitsMorphCurrent) * smooth;
            }

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

    const float interpolated = plaitsPreviousSample
        + (plaitsNextSample - plaitsPreviousSample)
            * static_cast<float> (plaitsStreamPhase);

    return interpolated * plaitsModelGain;
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

        const float baseCutoff = cutoffSmoothed.getNextValue();
        const float baseResonance = resonanceSmoothed.getNextValue();
        const float baseLevel = levelSmoothed.getNextValue();

        // Pitch, filter coefficients and slow modulation do not need
        // transcendental recalculation on every sample.  An 8-sample control
        // interval remains above 5 kHz at 44.1 kHz.
        if (controlUpdateCounter <= 0)
        {
            heldPitchModSemitones =
                pitchWheelSemitones
                + modulationFor (LfoTarget::Pitch) * 2.0f;
            heldFrequency =
                baseFrequencyHz
                * std::pow (
                    2.0,
                    static_cast<double> (heldPitchModSemitones) / 12.0);

            heldCutoff = juce::jlimit (
                20.0f,
                static_cast<float> (sampleRateHz * 0.45),
                baseCutoff
                    * std::pow (
                        2.0f,
                        modulationFor (LfoTarget::Cutoff) * 3.0f));
            heldResonance = juce::jlimit (
                0.1f, 12.0f,
                baseResonance
                    + modulationFor (LfoTarget::Resonance) * 5.0f);
            heldLevelMod = juce::jlimit (
                0.0f, 2.0f,
                1.0f + modulationFor (LfoTarget::Level) * 0.90f);

            filter.setCutoffFrequency (heldCutoff);
            filter.setResonance (heldResonance);
            controlUpdateCounter = controlUpdateInterval;
        }

        float oscillatorSample = 0.0f;
#if JUCE_ANDROID
        oscillatorSample = renderPlaitsOscillator (
            static_cast<float> (currentMidiNote)
            + heldPitchModSemitones);
#else
        oscillatorSample = std::sin (
            juce::MathConstants<float>::twoPi
            * static_cast<float> (phase));
        phase = wrap01 (phase + heldFrequency / sampleRateHz);
#endif

        --controlUpdateCounter;

        float sample = filter.processSample (0, oscillatorSample);
        sample *= adsr.getNextSample()
               * velocityGain
               * baseLevel
               * heldLevelMod
               * deClick.getNextValue();

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
