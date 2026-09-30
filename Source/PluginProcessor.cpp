#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"
#include <cmath>

namespace
{
    juce::NormalisableRange<float> skewedRange (float start, float end, float centre)
    {
        juce::NormalisableRange<float> range (start, end);
        range.setSkewForCentre (centre);
        return range;
    }

    float carnivalDefaultParam (int instrument, int param) noexcept
    {
        static constexpr float defaults[7][8]
        {
            { 0.84f, 0.50f, 0.72f, 0.48f, 0.36f, 0.08f, 0.00f, 0.46f }, // KICK
            { 0.74f, 0.52f, 0.82f, 0.50f, 0.34f, 0.12f, 0.02f, 0.60f }, // SNARE
            { 0.58f, 0.56f, 0.94f, 0.56f, 0.18f, 0.20f, 0.03f, 0.72f }, // HIHAT
            { 0.62f, 0.46f, 0.70f, 0.48f, 0.68f, 0.14f, 0.04f, 0.00f }, // CHORD
            { 0.66f, 0.50f, 0.78f, 0.50f, 0.55f, 0.18f, 0.05f, 0.42f }, // TONE
            { 0.72f, 0.48f, 0.68f, 0.46f, 0.44f, 0.10f, 0.02f, 0.36f }, // TOM
            { 0.74f, 0.50f, 0.58f, 0.42f, 0.72f, 0.10f, 0.04f, 0.58f }  // BASS
        };

        instrument = juce::jlimit (0, 6, instrument);
        param = juce::jlimit (0, 7, param);
        return defaults[instrument][param];
    }
}

FlowerStandaloneAudioProcessor::FlowerStandaloneAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FLOWER_STATE", createParameterLayout())
{
    for (int i = 0; i < 8; ++i)
        synthesiser.addVoice (new SineVoice());
    synthesiser.addSound (new SineSound());

    static constexpr int defaultMachines[carnivalTrackCount]
    {
        0, 1, 2, 3, 4, 5, 6, 0, 2, 4
    };

    for (int track = 0; track < carnivalTrackCount; ++track)
    {
        carnivalInstruments[static_cast<size_t> (track)].store (
            defaultMachines[track], std::memory_order_relaxed);

        for (int param = 0; param < carnivalParamCount; ++param)
        {
            carnivalBaseParams[static_cast<size_t> (
                carnivalBaseParamIndex (track, param))].store (
                    carnivalDefaultParam (defaultMachines[track], param),
                    std::memory_order_relaxed);
        }

        for (int step = 0; step < carnivalStepCount; ++step)
        {
            carnivalSteps[static_cast<size_t> (
                carnivalStepIndex (track, step))].store (
                    false, std::memory_order_relaxed);

            for (int param = 0; param < carnivalParamCount; ++param)
            {
                const auto lock = static_cast<size_t> (
                    carnivalLockIndex (track, step, param));
                carnivalLockEnabled[lock].store (
                    false, std::memory_order_relaxed);
                carnivalLockValues[lock].store (
                    carnivalDefaultParam (defaultMachines[track], param),
                    std::memory_order_relaxed);
            }
        }

        carnivalVoices[static_cast<size_t> (track)].noiseState =
            0x12345678u
            ^ (0x9e3779b9u * static_cast<uint32_t> (track + 1));
    }
}

bool FlowerStandaloneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void FlowerStandaloneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    synthesiser.setCurrentPlaybackSampleRate (currentSampleRate);

    const int flowerSamples = juce::jmax (4096, juce::roundToInt (currentSampleRate * 16.0));
    flowerLoopBuffer.setSize (2, flowerSamples, false, true, false);
    flowerLoopBuffer.clear();
    resetFlowerState();

    const int delaySamples = juce::jmax (2048, juce::roundToInt (currentSampleRate * 2.0));
    performanceDelayBuffer.setSize (2, delaySamples, false, true, false);
    performanceDelayBuffer.clear();
    performanceDelayWritePosition = 0;

    const int dreamySamples =
        juce::jmax (4096, juce::roundToInt (currentSampleRate * 2.5));
    performanceDreamyBuffer.setSize (2, dreamySamples, false, true, false);
    performanceDreamyBuffer.clear();
    performanceDreamyWritePosition = 0;
    performanceDreamySamplesFilled = 0;
    performanceDreamyLoopStart = { 0, 0 };
    performanceDreamyLoopLength = { 0, 0 };
    performanceDreamyOutputPhase = { 0, 0 };
    performanceDreamyLocalPosition = { 0.0f, 0.0f };
    performanceDreamyPlaybackSpeed = { 1.3348398f, 2.0f };
    performanceDreamyVoiceActive = { false, false };
    performanceDreamyRandomState = 0x44524541u;

    performanceDelaySamplesSmoothed.reset (currentSampleRate, 0.035);
    performanceDelayFeedbackSmoothed.reset (currentSampleRate, 0.025);
    performanceDelayWetSmoothed.reset (currentSampleRate, 0.025);

    const float initialDelaySeconds = 0.075f + 0.27f + 0.28f * 0.14f;
    performanceDelaySamplesSmoothed.setCurrentAndTargetValue (
        initialDelaySeconds * static_cast<float> (currentSampleRate));
    performanceDelayFeedbackSmoothed.setCurrentAndTargetValue (0.0f);
    performanceDelayWetSmoothed.setCurrentAndTargetValue (0.0f);

    juce::dsp::ProcessSpec limiterSpec;
    limiterSpec.sampleRate = currentSampleRate;
    limiterSpec.maximumBlockSize =
        static_cast<juce::uint32> (juce::jmax (1, samplesPerBlock));
    limiterSpec.numChannels =
        static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()));
    performanceOutputLimiter.prepare (limiterSpec);
    performanceOutputLimiter.setThreshold (-0.5f);
    performanceOutputLimiter.setRelease (80.0f);
    performanceOutputLimiter.reset();

    performanceSamplesUntilStep = 0.0;
    performanceStep = 0;
    performanceCurrentNote = -1;
    performanceRandomState = 0x46574C52u;

    carnivalSamplesUntilStep = 0.0;
    carnivalMidiClockCounter = 0;
    carnivalMidiRunning = false;
    carnivalCurrentStep.store (-1, std::memory_order_relaxed);
    for (auto& voice : carnivalVoices)
    {
        voice.active = false;
        voice.phase1 = voice.phase2 = voice.phase3 = 0.0;
        voice.lfoPhase = 0.0;
        voice.ageSeconds = 0.0f;
        voice.filterStateL = voice.filterStateR = 0.0f;
    }

    const auto initialise = [this] (juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& value,
                                    const char* id,
                                    double seconds)
    {
        value.reset (currentSampleRate, seconds);
        const float initial = apvts.getRawParameterValue (id)->load();
        value.setCurrentAndTargetValue (initial);
    };

    initialise (flowerPositionSmoothed, ParamIDs::flowerPosition, 0.020);
    initialise (flowerSizeSmoothed, ParamIDs::flowerSize, 0.030);
    initialise (flowerDensitySmoothed, ParamIDs::flowerDensity, 0.025);
    initialise (flowerSpreadSmoothed, ParamIDs::flowerSpread, 0.025);
    initialise (flowerHoldSmoothed, ParamIDs::flowerHold, 0.025);
    initialise (flowerPitchSmoothed, ParamIDs::flowerPitch, 0.020);
    initialise (flowerMixSmoothed, ParamIDs::flowerMix, 0.025);
}

void FlowerStandaloneAudioProcessor::updateSynthParams()
{
    SineVoice::Params params;
    params.level = apvts.getRawParameterValue (ParamIDs::level)->load();
    params.attack = apvts.getRawParameterValue (ParamIDs::attack)->load();
    params.decay = apvts.getRawParameterValue (ParamIDs::decay)->load();
    params.sustain = apvts.getRawParameterValue (ParamIDs::sustain)->load();
    params.release = apvts.getRawParameterValue (ParamIDs::release)->load();
    params.cutoff = apvts.getRawParameterValue (ParamIDs::cutoff)->load();
    params.resonance = apvts.getRawParameterValue (ParamIDs::resonance)->load();
    params.lfoRate = apvts.getRawParameterValue (ParamIDs::lfoRate)->load();
    params.lfoDepth = apvts.getRawParameterValue (ParamIDs::lfoDepth)->load();
    params.lfoTarget = juce::roundToInt (apvts.getRawParameterValue (ParamIDs::lfoTarget)->load());

    if (isPerformanceGateOpen())
    {
        const float x = performanceX.load (std::memory_order_relaxed);
        const float y = performanceY.load (std::memory_order_relaxed);

        // Keep enough headroom for arp release tails, granular wet signal and
        // feedback delay.  The previous 0.26 level could stack several release
        // tails above 0 dBFS at the faster arp divisions.
        params.level = 0.18f;
        params.attack = 0.003f;
        params.decay = 0.10f + (1.0f - y) * 0.16f;
        params.sustain = 0.58f;
        params.release = 0.045f + (1.0f - y) * 0.16f;

        const float filterCurve = std::pow (juce::jlimit (0.0f, 1.0f, y), 1.35f);
        params.cutoff = juce::jlimit (120.0f, 18000.0f,
                                     240.0f + filterCurve * 14500.0f + x * 1200.0f);
        params.resonance = 0.72f + y * 2.30f;

        params.lfoRate = 0.35f + x * 5.5f;
        params.lfoDepth = y * 0.10f;
        params.lfoTarget = static_cast<int> (LfoTarget::Cutoff);
    }

    for (int i = 0; i < synthesiser.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SineVoice*> (synthesiser.getVoice (i)))
            voice->setParams (params);
}

void FlowerStandaloneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (carnivalEnabled.load (std::memory_order_acquire))
    {
        handleCarnivalMidiClock (midiMessages);
        processCarnival (buffer);

        juce::dsp::AudioBlock<float> carnivalBlock (buffer);
        juce::dsp::ProcessContextReplacing<float> carnivalContext (carnivalBlock);
        performanceOutputLimiter.process (carnivalContext);
        return;
    }

    handlePerformanceMidiCC (midiMessages);
    generatePerformanceMidi (midiMessages, buffer.getNumSamples());

    updateSynthParams();
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);
    synthesiser.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // The XY performance path reuses FLOWER's proven granular core, but maps
    // pad gesture state directly to position/size/density/spread/hold/mix.
    processFlower (buffer);

    // DREAMY is a Mosaic-inspired overlapping micro-loop effect. Two short
    // captured loops run at +5 and +12 semitones while the dry signal remains.
    processPerformanceDreamy (buffer);

    // Delay is deliberately after the granular stage so one gesture moves
    // synthesis, granulation and echo as one performance surface.
    processPerformanceDelay (buffer);

    // Raise the final listening level without sacrificing the internal
    // headroom that removed the earlier crackle.  Make-up gain is applied only
    // after synth/granular/delay, immediately before the peak limiter.
    buffer.applyGain (juce::Decibels::decibelsToGain (4.0f));

    // Final peak protection only.  It is intentionally after the complete FX
    // chain so the Android output cannot receive > 0 dBFS bursts from stacked
    // arp tails or feedback.
    juce::dsp::AudioBlock<float> outputBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> outputContext (outputBlock);
    performanceOutputLimiter.process (outputContext);
}

void FlowerStandaloneAudioProcessor::resetFlowerState() noexcept
{
    flowerRecordWritePosition = 0;
    flowerLoopReadLength = 0;
    flowerLastRecordParam = false;
    flowerLastOverdubParam = false;
    flowerOverdubActive = false;
    flowerAnchorsInitialised = false;
    flowerPhase = { 0.0f, 0.25f, 0.5f, 0.75f };
    flowerAnchor = { 0.0f, 0.25f, 0.5f, 0.75f };
    flowerRepeatCounter = { 0, 0, 0, 0 };
    flowerRandomState = 0x514D4959u;
    flowerLoopLengthSamples.store (0, std::memory_order_relaxed);
    flowerRecordingActive.store (false, std::memory_order_relaxed);
    flowerRecordProgress.store (0.0f, std::memory_order_relaxed);
    flowerLoopValidFraction.store (0.0f, std::memory_order_relaxed);
    flowerBasePosition.store (0.0f, std::memory_order_relaxed);
    flowerActiveGrains.store (0, std::memory_order_relaxed);
    flowerClearRequested.store (false, std::memory_order_relaxed);
    flowerTransportRequest.store (0, std::memory_order_relaxed);
    flowerTransportState.store (0, std::memory_order_relaxed);

    for (auto& bin : flowerWaveform)
        bin.store (0.0f, std::memory_order_relaxed);
    for (auto& position : flowerGrainPositions)
        position.store (0.0f, std::memory_order_relaxed);
}

float FlowerStandaloneAudioProcessor::nextFlowerRandomBipolar() noexcept
{
    uint32_t x = flowerRandomState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    flowerRandomState = x;
    const float unit = static_cast<float> (x & 0x00ffffffu) / static_cast<float> (0x00ffffffu);
    return unit * 2.0f - 1.0f;
}

void FlowerStandaloneAudioProcessor::getFlowerWaveform (
    std::array<float, flowerWaveformBins>& destination) const noexcept
{
    for (int i = 0; i < flowerWaveformBins; ++i)
        destination[static_cast<size_t> (i)] =
            flowerWaveform[static_cast<size_t> (i)].load (std::memory_order_relaxed);
}

float FlowerStandaloneAudioProcessor::getFlowerGrainPosition (int index) const noexcept
{
    if (index < 0 || index >= flowerGrainCount)
        return 0.0f;
    return flowerGrainPositions[static_cast<size_t> (index)].load (std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::clearFlowerLoop() noexcept
{
    flowerClearRequested.store (true, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::processFlower (juce::AudioBuffer<float>& buffer)
{
    const int capacity = flowerLoopBuffer.getNumSamples();
    const int numSamples = buffer.getNumSamples();
    if (capacity <= 64 || numSamples <= 0)
        return;

    if (flowerClearRequested.exchange (false, std::memory_order_acq_rel))
    {
        flowerLoopBuffer.clear();
        resetFlowerState();
    }

    int transportState = flowerTransportState.load (std::memory_order_relaxed);
    const int transportRequest =
        flowerTransportRequest.exchange (0, std::memory_order_acq_rel);

    if (transportRequest == 1)
    {
        if (transportState == 0) // EMPTY -> REC
        {
            flowerLoopBuffer.clear();
            flowerRecordWritePosition = 0;
            flowerLoopReadLength = 0;
            flowerLoopLengthSamples.store (0, std::memory_order_relaxed);
            flowerLoopValidFraction.store (0.0f, std::memory_order_relaxed);
            flowerRecordProgress.store (0.0f, std::memory_order_relaxed);
            flowerAnchorsInitialised = false;
            for (auto& bin : flowerWaveform)
                bin.store (0.0f, std::memory_order_relaxed);
            transportState = 1;
        }
        else if (transportState == 1) // REC -> STOP
        {
            transportState =
                flowerLoopLengthSamples.load (std::memory_order_relaxed) > 64 ? 2 : 0;
            flowerRecordWritePosition = 0;
        }
        else if (transportState == 2) // STOP -> OVERDUB
        {
            if (flowerLoopLengthSamples.load (std::memory_order_relaxed) > 64)
            {
                transportState = 3;
                flowerRecordWritePosition = 0;
            }
        }
        else if (transportState == 3) // OVERDUB -> STOP
        {
            transportState = 2;
            flowerRecordWritePosition = 0;
        }

        flowerTransportState.store (transportState, std::memory_order_relaxed);
    }

    const bool performanceGate = isPerformanceGateOpen();
    const float performancePadX = performanceX.load (std::memory_order_relaxed);
    const float performancePadY = performanceY.load (std::memory_order_relaxed);
    const float gestureSpeed = performanceSpeed.load (std::memory_order_relaxed);
    const float gestureDirection = performanceDirection.load (std::memory_order_relaxed);

    const bool legacyEnabled =
        apvts.getRawParameterValue (ParamIDs::flowerEnabled)->load() > 0.5f;
    const bool granularEnabled =
        performanceGranularEnabled.load (std::memory_order_relaxed)
        && ! performanceDreamyMode.load (std::memory_order_relaxed);
    const bool enabled =
        legacyEnabled || (granularEnabled && performanceGate && performancePadY > 0.025f);

    flowerRecordingActive.store (
        transportState == 1 || transportState == 3,
        std::memory_order_relaxed);
    flowerOverdubActive = transportState == 3;

    const float positionTarget = performanceGate
        ? performancePadX
        : apvts.getRawParameterValue (ParamIDs::flowerPosition)->load();

    const float sizeTarget = performanceGate
        ? juce::jmap (juce::jlimit (0.0f, 1.0f, gestureSpeed), 0.18f, 0.022f)
        : apvts.getRawParameterValue (ParamIDs::flowerSize)->load();

    const float densityTarget = performanceGate
        ? performancePadY
        : apvts.getRawParameterValue (ParamIDs::flowerDensity)->load();

    const float spreadTarget = performanceGate
        ? juce::jlimit (0.0f, 1.0f, 0.04f + performancePadY * 0.86f)
        : apvts.getRawParameterValue (ParamIDs::flowerSpread)->load();

    const float holdTarget = performanceGate
        ? juce::jlimit (0.0f, 1.0f, 0.10f + performancePadY * 0.82f)
        : apvts.getRawParameterValue (ParamIDs::flowerHold)->load();

    const float pitchTarget = performanceGate
        ? 0.0f
        : apvts.getRawParameterValue (ParamIDs::flowerPitch)->load();

    const float mixTarget = performanceGate
        ? juce::jlimit (0.0f, 0.88f, performancePadY * performancePadY * 0.88f)
        : apvts.getRawParameterValue (ParamIDs::flowerMix)->load();

    flowerPositionSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, positionTarget));
    flowerSizeSmoothed.setTargetValue (juce::jlimit (0.008f, 0.50f, sizeTarget));
    flowerDensitySmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, densityTarget));
    flowerSpreadSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, spreadTarget));
    flowerHoldSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, holdTarget));
    flowerPitchSmoothed.setTargetValue (juce::jlimit (-12.0f, 12.0f, pitchTarget));
    flowerMixSmoothed.setTargetValue (enabled ? mixTarget : 0.0f);

    const bool reverse = performanceGate
        ? (gestureSpeed > 0.12f && gestureDirection < -0.10f)
        : (apvts.getRawParameterValue (ParamIDs::flowerReverse)->load() > 0.5f);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    auto* loopL = flowerLoopBuffer.getWritePointer (0);
    auto* loopR = flowerLoopBuffer.getWritePointer (1);

    const auto wrapPosition = [] (float position, int length)
    {
        const float n = static_cast<float> (length);
        while (position >= n) position -= n;
        while (position < 0.0f) position += n;
        return position;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = outL[i];
        const float dryR = outR != nullptr ? outR[i] : dryL;

        int loopLength = flowerLoopLengthSamples.load (std::memory_order_relaxed);

        if (transportState == 1)
        {
            const int writePos = flowerRecordWritePosition;
            loopL[writePos] = dryL;
            loopR[writePos] = dryR;

            const int waveformBin = juce::jlimit (
                0, flowerWaveformBins - 1,
                static_cast<int> (
                    (static_cast<int64_t> (writePos) * flowerWaveformBins) / capacity));
            flowerWaveform[static_cast<size_t> (waveformBin)].store (
                juce::jlimit (0.0f, 1.0f,
                    juce::jmax (std::abs (dryL), std::abs (dryR))),
                std::memory_order_relaxed);

            ++flowerRecordWritePosition;
            loopLength = juce::jmin (capacity, juce::jmax (loopLength, flowerRecordWritePosition));
            flowerLoopLengthSamples.store (loopLength, std::memory_order_relaxed);
            flowerLoopReadLength = loopLength;

            if (flowerRecordWritePosition >= capacity)
            {
                flowerRecordWritePosition = 0;
                transportState = 2;
                flowerTransportState.store (transportState, std::memory_order_relaxed);
                flowerRecordingActive.store (false, std::memory_order_relaxed);
            }
        }
        else if (transportState == 3 && loopLength > 64)
        {
            const int writePos = flowerRecordWritePosition % loopLength;

            // Stable overdub: retain most of the previous loop while adding
            // the live synth at a lower gain so repeated passes do not run away.
            loopL[writePos] = loopL[writePos] * 0.82f + dryL * 0.58f;
            loopR[writePos] = loopR[writePos] * 0.82f + dryR * 0.58f;

            const int waveformBin = juce::jlimit (
                0, flowerWaveformBins - 1,
                static_cast<int> (
                    (static_cast<int64_t> (writePos) * flowerWaveformBins)
                    / juce::jmax (1, loopLength)));
            flowerWaveform[static_cast<size_t> (waveformBin)].store (
                juce::jlimit (0.0f, 1.0f,
                    juce::jmax (std::abs (loopL[writePos]), std::abs (loopR[writePos]))),
                std::memory_order_relaxed);

            flowerRecordWritePosition = (writePos + 1) % loopLength;
        }

        loopLength = flowerLoopLengthSamples.load (std::memory_order_relaxed);

        flowerLoopValidFraction.store (
            static_cast<float> (loopLength) / static_cast<float> (capacity),
            std::memory_order_relaxed);
        flowerRecordProgress.store (
            loopLength > 0
                ? static_cast<float> (flowerRecordWritePosition)
                    / static_cast<float> (juce::jmax (1, loopLength))
                : 0.0f,
            std::memory_order_relaxed);

        if (! enabled || loopLength <= 64)
        {
            flowerActiveGrains.store (0, std::memory_order_relaxed);
            continue;
        }

        const auto readLinear = [&] (const float* data, float logicalPosition)
        {
            logicalPosition = wrapPosition (logicalPosition, loopLength);
            const int i0 = juce::jlimit (
                0, loopLength - 1, static_cast<int> (logicalPosition));
            const int i1 = (i0 + 1) % loopLength;
            const float frac = logicalPosition - static_cast<float> (i0);
            return data[i0] + (data[i1] - data[i0]) * frac;
        };

        const float position = flowerPositionSmoothed.getNextValue();
        const float sizeSeconds = flowerSizeSmoothed.getNextValue();
        const float density = flowerDensitySmoothed.getNextValue();
        const float spread = flowerSpreadSmoothed.getNextValue();
        const float holdAmount = flowerHoldSmoothed.getNextValue();
        const float pitch = flowerPitchSmoothed.getNextValue();
        const float mix = flowerMixSmoothed.getNextValue();

        const int activeGrains = juce::jlimit (
            1, flowerGrainCount,
            1 + juce::roundToInt (
                density * static_cast<float> (flowerGrainCount - 1)));
        flowerActiveGrains.store (activeGrains, std::memory_order_relaxed);

        const float grainSamples = juce::jlimit (
            64.0f, static_cast<float> (juce::jmax (64, loopLength)),
            sizeSeconds * static_cast<float> (currentSampleRate));
        const float pitchRatio = std::pow (2.0f, pitch / 12.0f);
        const float direction = reverse ? -1.0f : 1.0f;
        const int repeats = 1 + juce::roundToInt (holdAmount * 15.0f);
        const float centreSample = position * static_cast<float> (loopLength - 1);
        const float grainTravel = direction * grainSamples * pitchRatio;

        if (! flowerAnchorsInitialised)
        {
            for (int grain = 0; grain < flowerGrainCount; ++grain)
            {
                const float scatter = nextFlowerRandomBipolar()
                                    * spread * 0.5f * static_cast<float> (loopLength);
                flowerAnchor[static_cast<size_t> (grain)] =
                    wrapPosition (
                        centreSample + scatter - 0.5f * grainTravel, loopLength);
            }
            flowerAnchorsInitialised = true;
        }

        flowerBasePosition.store (position, std::memory_order_relaxed);

        float wetL = 0.0f;
        float wetR = 0.0f;
        float weightSum = 0.0f;

        for (int grain = 0; grain < activeGrains; ++grain)
        {
            auto& phaseValue = flowerPhase[static_cast<size_t> (grain)];
            const float window =
                std::sin (juce::MathConstants<float>::pi * phaseValue);
            const float weight = window * window;
            const float readPos =
                flowerAnchor[static_cast<size_t> (grain)]
                + direction * phaseValue * grainSamples * pitchRatio;

            wetL += readLinear (loopL, readPos) * weight;
            wetR += readLinear (loopR, readPos) * weight;
            weightSum += weight;

            const float normalizedRead =
                wrapPosition (readPos, loopLength)
                / static_cast<float> (juce::jmax (1, loopLength - 1));
            flowerGrainPositions[static_cast<size_t> (grain)].store (
                juce::jlimit (0.0f, 1.0f, normalizedRead),
                std::memory_order_relaxed);

            phaseValue += 1.0f / grainSamples;
            if (phaseValue >= 1.0f)
            {
                phaseValue -= std::floor (phaseValue);
                auto& repeat = flowerRepeatCounter[static_cast<size_t> (grain)];
                ++repeat;

                if (repeat >= repeats)
                {
                    repeat = 0;
                    const float scatter = nextFlowerRandomBipolar()
                                        * spread * 0.5f
                                        * static_cast<float> (loopLength);
                    flowerAnchor[static_cast<size_t> (grain)] =
                        wrapPosition (
                            centreSample + scatter - 0.5f * grainTravel,
                            loopLength);
                }
            }
        }

        for (int grain = activeGrains; grain < flowerGrainCount; ++grain)
            flowerGrainPositions[static_cast<size_t> (grain)].store (
                position, std::memory_order_relaxed);

        if (weightSum > 0.0001f)
        {
            wetL /= weightSum;
            wetR /= weightSum;
        }

        outL[i] = dryL + (wetL - dryL) * mix;
        if (outR != nullptr)
            outR[i] = dryR + (wetR - dryR) * mix;
    }
}


void FlowerStandaloneAudioProcessor::setPerformancePad (float x,
                                                              float y,
                                                              float speed,
                                                              float horizontalDirection,
                                                              bool active) noexcept
{
    performanceX.store (juce::jlimit (0.0f, 1.0f, x), std::memory_order_relaxed);
    performanceY.store (juce::jlimit (0.0f, 1.0f, y), std::memory_order_relaxed);
    performanceSpeed.store (juce::jlimit (0.0f, 1.0f, speed), std::memory_order_relaxed);
    performanceDirection.store (
        juce::jlimit (-1.0f, 1.0f, horizontalDirection), std::memory_order_relaxed);
    performanceActive.store (active, std::memory_order_release);

    if (active)
        performanceLatched.store (true, std::memory_order_release);
    else if (! performanceHold.load (std::memory_order_acquire))
        performanceLatched.store (false, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::setPerformanceHold (bool shouldHold) noexcept
{
    performanceHold.store (shouldHold, std::memory_order_release);

    if (! shouldHold && ! performanceActive.load (std::memory_order_acquire))
        performanceLatched.store (false, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::setPerformanceRoot (int noteClass) noexcept
{
    performanceRootClass.store (juce::jlimit (0, 11, noteClass), std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::setPerformanceScale (int scaleIndex) noexcept
{
    performanceScaleIndex.store (juce::jlimit (0, 4, scaleIndex), std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::setPerformanceBpm (float bpm) noexcept
{
    performanceBpm.store (juce::jlimit (50.0f, 200.0f, bpm), std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::setPerformanceArpEnabled (bool enabled) noexcept
{
    performanceArpEnabled.store (enabled, std::memory_order_release);
    performanceStopRequested.store (true, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::setPerformanceDelayEnabled (bool enabled) noexcept
{
    performanceDelayEnabled.store (enabled, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::setPerformanceGranularEnabled (bool enabled) noexcept
{
    performanceGranularEnabled.store (enabled, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::setPerformanceDreamyMode (bool enabled) noexcept
{
    performanceDreamyMode.store (enabled, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::setPerformancePolyTouch (
    int touchId, float x, float y, bool down) noexcept
{
    if (touchId < 0 || touchId >= performancePolyTouchCount)
        return;

    const auto index = static_cast<size_t> (touchId);
    performancePolyTouchX[index].store (
        juce::jlimit (0.0f, 1.0f, x), std::memory_order_relaxed);
    performancePolyTouchY[index].store (
        juce::jlimit (0.0f, 1.0f, y), std::memory_order_relaxed);
    performancePolyTouchActive[index].store (
        down, std::memory_order_release);

    if (down)
    {
        performanceX.store (
            juce::jlimit (0.0f, 1.0f, x), std::memory_order_relaxed);
        performanceY.store (
            juce::jlimit (0.0f, 1.0f, y), std::memory_order_relaxed);
        performanceSpeed.store (0.0f, std::memory_order_relaxed);
        performanceDirection.store (0.0f, std::memory_order_relaxed);
    }

    bool anyActive = false;
    for (const auto& active : performancePolyTouchActive)
        anyActive = anyActive
            || active.load (std::memory_order_acquire);

    performanceActive.store (anyActive, std::memory_order_release);

    if (anyActive)
        performanceLatched.store (true, std::memory_order_release);
    else if (! performanceHold.load (std::memory_order_acquire))
        performanceLatched.store (false, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::cycleFlowerTransport() noexcept
{
    flowerTransportRequest.store (1, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::stopPerformance() noexcept
{
    performanceActive.store (false, std::memory_order_release);
    performanceHold.store (false, std::memory_order_release);
    performanceLatched.store (false, std::memory_order_release);

    for (auto& active : performancePolyTouchActive)
        active.store (false, std::memory_order_release);

    performanceStopRequested.store (true, std::memory_order_release);
}

int FlowerStandaloneAudioProcessor::getConfiguredRoot() const noexcept
{
    return juce::jlimit (
        0, 11,
        juce::roundToInt (
            apvts.getRawParameterValue (ParamIDs::performanceRootConfig)->load()));
}

int FlowerStandaloneAudioProcessor::getConfiguredScale() const noexcept
{
    return juce::jlimit (
        0, 4,
        juce::roundToInt (
            apvts.getRawParameterValue (ParamIDs::performanceScaleConfig)->load()));
}

bool FlowerStandaloneAudioProcessor::getDefaultEffectsEnabled() const noexcept
{
    return apvts.getRawParameterValue (ParamIDs::defaultEffectsEnabled)->load() > 0.5f;
}

bool FlowerStandaloneAudioProcessor::getConfiguredYEffectDreamy() const noexcept
{
    return apvts.getRawParameterValue (ParamIDs::performanceYEffectConfig)->load() > 0.5f;
}

int FlowerStandaloneAudioProcessor::getConfiguredMidiChannel() const noexcept
{
    return juce::jlimit (
        1, 16,
        juce::roundToInt (
            apvts.getRawParameterValue (ParamIDs::performanceMidiChannelConfig)->load()));
}

void FlowerStandaloneAudioProcessor::setConfiguredRoot (int noteClass)
{
    noteClass = juce::jlimit (0, 11, noteClass);
    setPerformanceRoot (noteClass);

    if (auto* parameter = apvts.getParameter (ParamIDs::performanceRootConfig))
        parameter->setValueNotifyingHost (
            parameter->convertTo0to1 (static_cast<float> (noteClass)));
}

void FlowerStandaloneAudioProcessor::setConfiguredScale (int scaleIndex)
{
    scaleIndex = juce::jlimit (0, 4, scaleIndex);
    setPerformanceScale (scaleIndex);

    if (auto* parameter = apvts.getParameter (ParamIDs::performanceScaleConfig))
        parameter->setValueNotifyingHost (
            parameter->convertTo0to1 (static_cast<float> (scaleIndex)));
}

void FlowerStandaloneAudioProcessor::setDefaultEffectsEnabled (bool enabled)
{
    setPerformanceDelayEnabled (enabled);
    setPerformanceGranularEnabled (enabled);

    if (auto* parameter = apvts.getParameter (ParamIDs::defaultEffectsEnabled))
        parameter->setValueNotifyingHost (enabled ? 1.0f : 0.0f);
}

void FlowerStandaloneAudioProcessor::setConfiguredYEffectDreamy (bool enabled)
{
    setPerformanceDreamyMode (enabled);

    if (auto* parameter = apvts.getParameter (ParamIDs::performanceYEffectConfig))
        parameter->setValueNotifyingHost (enabled ? 1.0f : 0.0f);
}

void FlowerStandaloneAudioProcessor::setConfiguredMidiChannel (int channel)
{
    channel = juce::jlimit (1, 16, channel);

    if (auto* parameter = apvts.getParameter (ParamIDs::performanceMidiChannelConfig))
        parameter->setValueNotifyingHost (
            parameter->convertTo0to1 (static_cast<float> (channel)));
}

void FlowerStandaloneAudioProcessor::handlePerformanceMidiCC (juce::MidiBuffer& midi)
{
    const int configuredChannel = getConfiguredMidiChannel();

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (! message.isController()
            || message.getChannel() != configuredChannel)
            continue;

        const int cc = message.getControllerNumber();
        const int value = message.getControllerValue();
        const float normalized = static_cast<float> (value) / 127.0f;
        const bool on = value >= 64;

        // FLOWER fixed MIDI CC map:
        // CC10 Y, CC11 X, CC22 DELAY, CC23 ARP, CC24 Y EFFECT,
        // CC25 LOOPER CYCLE, CC26 BPM, CC27 HOLD, CC28 STOP,
        // CC29 LOOPER CLEAR.
        switch (cc)
        {
            case 10:
                setPerformancePad (
                    performanceX.load (std::memory_order_relaxed),
                    normalized,
                    0.0f,
                    0.0f,
                    true);
                break;

            case 11:
            {
                const float previousX =
                    performanceX.load (std::memory_order_relaxed);
                const float direction =
                    normalized < previousX ? -1.0f
                    : normalized > previousX ? 1.0f
                    : 0.0f;

                setPerformancePad (
                    normalized,
                    performanceY.load (std::memory_order_relaxed),
                    0.0f,
                    direction,
                    true);
                break;
            }

            case 22:
                setPerformanceDelayEnabled (on);
                break;

            case 23:
                setPerformanceArpEnabled (on);
                break;

            case 24:
                setPerformanceGranularEnabled (on);
                break;

            case 25:
                if (on && ! performanceCcGate[25])
                    cycleFlowerTransport();
                performanceCcGate[25] = on;
                break;

            case 26:
                setPerformanceBpm (50.0f + normalized * 150.0f);
                break;

            case 27:
                setPerformanceHold (on);
                break;

            case 28:
                if (on && ! performanceCcGate[28])
                    stopPerformance();
                performanceCcGate[28] = on;
                break;

            case 29:
                if (on && ! performanceCcGate[29])
                    clearFlowerLoop();
                performanceCcGate[29] = on;
                break;

            default:
                break;
        }
    }
}


void FlowerStandaloneAudioProcessor::setCarnivalEnabled (bool enabled) noexcept
{
    carnivalEnabled.store (enabled, std::memory_order_release);
    carnivalResetRequested.store (true, std::memory_order_release);

    if (enabled)
        stopPerformance();
    else
        carnivalPlaying.store (false, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::setCarnivalPlaying (bool playing) noexcept
{
    carnivalPlaying.store (playing, std::memory_order_release);
    if (playing)
        carnivalResetRequested.store (true, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::setCarnivalClockMidi (bool midiClock) noexcept
{
    carnivalClockMidi.store (midiClock, std::memory_order_release);
    carnivalResetRequested.store (true, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::setCarnivalBpm (float bpm) noexcept
{
    carnivalBpm.store (
        juce::jlimit (40.0f, 240.0f, bpm),
        std::memory_order_relaxed);
}

bool FlowerStandaloneAudioProcessor::getCarnivalStepEnabled (
    int track, int step) const noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || step < 0 || step >= carnivalStepCount)
        return false;

    return carnivalSteps[static_cast<size_t> (
        carnivalStepIndex (track, step))].load (std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::setCarnivalStepEnabled (
    int track, int step, bool enabled) noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || step < 0 || step >= carnivalStepCount)
        return;

    carnivalSteps[static_cast<size_t> (
        carnivalStepIndex (track, step))].store (
            enabled, std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::toggleCarnivalStep (
    int track, int step) noexcept
{
    const bool next = ! getCarnivalStepEnabled (track, step);
    setCarnivalStepEnabled (track, step, next);

    if (next)
        previewCarnivalTrack (track);
}

int FlowerStandaloneAudioProcessor::getCarnivalInstrument (int track) const noexcept
{
    if (track < 0 || track >= carnivalTrackCount)
        return 0;

    return juce::jlimit (
        0, static_cast<int> (CarnivalInstrument::Count) - 1,
        carnivalInstruments[static_cast<size_t> (track)].load (
            std::memory_order_relaxed));
}

void FlowerStandaloneAudioProcessor::setCarnivalInstrument (
    int track, int instrument) noexcept
{
    if (track < 0 || track >= carnivalTrackCount)
        return;

    const int count = static_cast<int> (CarnivalInstrument::Count);
    instrument %= count;
    if (instrument < 0)
        instrument += count;

    carnivalInstruments[static_cast<size_t> (track)].store (
        instrument, std::memory_order_relaxed);

    for (int param = 0; param < carnivalParamCount; ++param)
        carnivalBaseParams[static_cast<size_t> (
            carnivalBaseParamIndex (track, param))].store (
                carnivalDefaultParam (instrument, param),
                std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::cycleCarnivalInstrument (
    int track, int delta) noexcept
{
    setCarnivalInstrument (
        track, getCarnivalInstrument (track) + delta);
    previewCarnivalTrack (track);
}

float FlowerStandaloneAudioProcessor::getCarnivalBaseParam (
    int track, int param) const noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || param < 0 || param >= carnivalParamCount)
        return 0.0f;

    return carnivalBaseParams[static_cast<size_t> (
        carnivalBaseParamIndex (track, param))].load (
            std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::setCarnivalBaseParam (
    int track, int param, float value) noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || param < 0 || param >= carnivalParamCount)
        return;

    carnivalBaseParams[static_cast<size_t> (
        carnivalBaseParamIndex (track, param))].store (
            juce::jlimit (0.0f, 1.0f, value),
            std::memory_order_relaxed);
}

bool FlowerStandaloneAudioProcessor::getCarnivalParamLockEnabled (
    int track, int step, int param) const noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || step < 0 || step >= carnivalStepCount
        || param < 0 || param >= carnivalParamCount)
        return false;

    return carnivalLockEnabled[static_cast<size_t> (
        carnivalLockIndex (track, step, param))].load (
            std::memory_order_relaxed);
}

float FlowerStandaloneAudioProcessor::getCarnivalParamLockValue (
    int track, int step, int param) const noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || step < 0 || step >= carnivalStepCount
        || param < 0 || param >= carnivalParamCount)
        return 0.0f;

    return carnivalLockValues[static_cast<size_t> (
        carnivalLockIndex (track, step, param))].load (
            std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::setCarnivalParamLock (
    int track, int step, int param,
    bool enabled, float value) noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || step < 0 || step >= carnivalStepCount
        || param < 0 || param >= carnivalParamCount)
        return;

    const auto index = static_cast<size_t> (
        carnivalLockIndex (track, step, param));

    carnivalLockValues[index].store (
        juce::jlimit (0.0f, 1.0f, value),
        std::memory_order_relaxed);
    carnivalLockEnabled[index].store (
        enabled, std::memory_order_release);
}

bool FlowerStandaloneAudioProcessor::carnivalStepHasLocks (
    int track, int step) const noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || step < 0 || step >= carnivalStepCount)
        return false;

    for (int param = 0; param < carnivalParamCount; ++param)
        if (getCarnivalParamLockEnabled (track, step, param))
            return true;

    return false;
}

void FlowerStandaloneAudioProcessor::clearCarnivalStepLocks (
    int track, int step) noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || step < 0 || step >= carnivalStepCount)
        return;

    for (int param = 0; param < carnivalParamCount; ++param)
    {
        const auto index = static_cast<size_t> (
            carnivalLockIndex (track, step, param));
        carnivalLockEnabled[index].store (
            false, std::memory_order_release);
    }
}

void FlowerStandaloneAudioProcessor::clearCarnivalPattern() noexcept
{
    for (auto& step : carnivalSteps)
        step.store (false, std::memory_order_relaxed);

    for (auto& lock : carnivalLockEnabled)
        lock.store (false, std::memory_order_relaxed);
}

void FlowerStandaloneAudioProcessor::previewCarnivalTrack (int track) noexcept
{
    if (track >= 0 && track < carnivalTrackCount)
    {
        carnivalPreviewStepRequested.store (-1, std::memory_order_relaxed);
        carnivalPreviewTrackRequested.store (
            track, std::memory_order_release);
    }
}

void FlowerStandaloneAudioProcessor::previewCarnivalTrigger (
    int track, int step) noexcept
{
    if (track < 0 || track >= carnivalTrackCount
        || step < 0 || step >= carnivalStepCount)
        return;

    carnivalPreviewStepRequested.store (step, std::memory_order_relaxed);
    carnivalPreviewTrackRequested.store (
        track, std::memory_order_release);
}

void FlowerStandaloneAudioProcessor::handleCarnivalMidiClock (
    const juce::MidiBuffer& midi)
{
    carnivalMidiTriggerCount = 0;

    const bool midiClockMode =
        carnivalClockMidi.load (std::memory_order_acquire);

    const auto queueTrigger =
        [this] (int sampleOffset, int step)
        {
            if (carnivalMidiTriggerCount >= carnivalMidiTriggerCapacity)
                return;

            const int index = carnivalMidiTriggerCount++;
            carnivalMidiTriggerSamples[static_cast<size_t> (index)] =
                juce::jmax (0, sampleOffset);
            carnivalMidiTriggerSteps[static_cast<size_t> (index)] =
                juce::jlimit (0, carnivalStepCount - 1, step);
        };

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        // MIDI transport is honoured in both INTERNAL and MIDI clock modes.
        // Clock pulses themselves are consumed only when CLOCK SOURCE = MIDI.
        if (message.isMidiStart())
        {
            carnivalMidiRunning = true;
            carnivalMidiClockCounter = 0;
            carnivalPlaying.store (true, std::memory_order_release);
            carnivalCurrentStep.store (0, std::memory_order_relaxed);
            queueTrigger (metadata.samplePosition, 0);

            if (! midiClockMode)
            {
                const double bpm = static_cast<double> (
                    carnivalBpm.load (std::memory_order_relaxed));
                const double stepSamples =
                    juce::jmax (1.0, currentSampleRate)
                    * 60.0
                    / juce::jlimit (40.0, 240.0, bpm)
                    / 2.0;

                carnivalSamplesUntilStep =
                    static_cast<double> (
                        juce::jmax (0, metadata.samplePosition))
                    + stepSamples;
            }
        }
        else if (message.isMidiContinue())
        {
            carnivalMidiRunning = true;
            carnivalPlaying.store (true, std::memory_order_release);
        }
        else if (message.isMidiStop())
        {
            carnivalMidiRunning = false;
            carnivalPlaying.store (false, std::memory_order_release);
        }
        else if (message.isMidiClock()
                 && midiClockMode
                 && carnivalMidiRunning)
        {
            ++carnivalMidiClockCounter;

            if (carnivalMidiClockCounter >= 12)
            {
                carnivalMidiClockCounter = 0;
                const int next =
                    (juce::jmax (0, carnivalCurrentStep.load (
                        std::memory_order_relaxed)) + 1)
                    % carnivalStepCount;

                carnivalCurrentStep.store (
                    next, std::memory_order_relaxed);
                queueTrigger (metadata.samplePosition, next);
            }
        }
    }
}

float FlowerStandaloneAudioProcessor::nextCarnivalNoise (int track) noexcept
{
    auto& state = carnivalVoices[static_cast<size_t> (track)].noiseState;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    const float unit =
        static_cast<float> (state & 0x00ffffffu)
        / static_cast<float> (0x00ffffffu);
    return unit * 2.0f - 1.0f;
}

void FlowerStandaloneAudioProcessor::triggerCarnivalTrack (
    int track, int step) noexcept
{
    if (track < 0 || track >= carnivalTrackCount)
        return;

    auto& voice = carnivalVoices[static_cast<size_t> (track)];
    voice.active = true;
    voice.instrument = getCarnivalInstrument (track);
    voice.phase1 = voice.phase2 = voice.phase3 = 0.0;
    voice.lfoPhase = 0.0;
    voice.ageSeconds = 0.0f;
    voice.filterStateL = voice.filterStateR = 0.0f;

    auto readParam = [this, track, step] (int param)
    {
        if (step >= 0
            && step < carnivalStepCount
            && getCarnivalParamLockEnabled (track, step, param))
            return getCarnivalParamLockValue (track, step, param);

        return getCarnivalBaseParam (track, param);
    };

    voice.volume = readParam (static_cast<int> (CarnivalParam::Volume));
    voice.pan = readParam (static_cast<int> (CarnivalParam::Pan));
    voice.filter = readParam (static_cast<int> (CarnivalParam::Filter));
    voice.pitch = readParam (static_cast<int> (CarnivalParam::Pitch));
    voice.decay = readParam (static_cast<int> (CarnivalParam::Decay));
    voice.lfoRate = readParam (static_cast<int> (CarnivalParam::LfoRate));
    voice.lfoDepth = readParam (static_cast<int> (CarnivalParam::LfoDepth));
    voice.character = readParam (static_cast<int> (CarnivalParam::Character));

    static constexpr float baseFrequencies[]
    {
        55.0f, 180.0f, 5200.0f, 220.0f,
        330.0f, 120.0f, 65.0f
    };

    const float semitones = (voice.pitch - 0.5f) * 48.0f;
    voice.frequency =
        baseFrequencies[voice.instrument]
        * std::pow (2.0f, semitones / 12.0f);
}

void FlowerStandaloneAudioProcessor::triggerCarnivalStep (int step) noexcept
{
    if (step < 0 || step >= carnivalStepCount)
        return;

    for (int track = 0; track < carnivalTrackCount; ++track)
        if (getCarnivalStepEnabled (track, step))
            triggerCarnivalTrack (track, step);
}

void FlowerStandaloneAudioProcessor::processCarnival (
    juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    if (carnivalResetRequested.exchange (
            false, std::memory_order_acq_rel))
    {
        carnivalSamplesUntilStep = 0.0;
        carnivalMidiClockCounter = 0;

        if (! carnivalClockMidi.load (std::memory_order_acquire))
            carnivalCurrentStep.store (-1, std::memory_order_relaxed);
    }

    const int preview =
        carnivalPreviewTrackRequested.exchange (
            -1, std::memory_order_acq_rel);
    if (preview >= 0)
    {
        const int previewStep =
            carnivalPreviewStepRequested.exchange (
                -1, std::memory_order_acq_rel);
        triggerCarnivalTrack (preview, previewStep);
    }

    const bool internalClock =
        ! carnivalClockMidi.load (std::memory_order_acquire);

    int midiTriggerIndex = 0;

    const double sampleRate = juce::jmax (1.0, currentSampleRate);
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (midiTriggerIndex < carnivalMidiTriggerCount
               && carnivalMidiTriggerSamples[
                      static_cast<size_t> (midiTriggerIndex)] <= sample)
        {
            triggerCarnivalStep (
                carnivalMidiTriggerSteps[
                    static_cast<size_t> (midiTriggerIndex)]);
            ++midiTriggerIndex;
        }

        if (internalClock
            && carnivalPlaying.load (std::memory_order_acquire))
        {
            if (carnivalSamplesUntilStep <= 0.0)
            {
                const int previous =
                    carnivalCurrentStep.load (std::memory_order_relaxed);
                const int next =
                    previous < 0 ? 0 : (previous + 1) % carnivalStepCount;

                carnivalCurrentStep.store (
                    next, std::memory_order_relaxed);
                triggerCarnivalStep (next);

                const double bpm = static_cast<double> (
                    carnivalBpm.load (std::memory_order_relaxed));
                carnivalSamplesUntilStep +=
                    sampleRate * 60.0
                    / juce::jlimit (40.0, 240.0, bpm)
                    / 2.0;
            }

            carnivalSamplesUntilStep -= 1.0;
        }

        float left = 0.0f;
        float right = 0.0f;

        for (int track = 0; track < carnivalTrackCount; ++track)
        {
            auto& voice = carnivalVoices[static_cast<size_t> (track)];
            if (! voice.active)
                continue;

            const float baseDecay =
                voice.instrument == static_cast<int> (CarnivalInstrument::Hihat)
                    ? 0.025f + voice.decay * 0.38f
                    : voice.instrument == static_cast<int> (CarnivalInstrument::Kick)
                        ? 0.055f + voice.decay * 0.72f
                        : 0.08f + voice.decay * 1.25f;

            if (voice.ageSeconds >= baseDecay)
            {
                voice.active = false;
                continue;
            }

            const float envelope =
                std::exp (-6.0f * voice.ageSeconds / baseDecay);

            const float lfoHz = 0.10f + voice.lfoRate * 15.9f;
            const float lfo =
                std::sin (static_cast<float> (voice.lfoPhase))
                * voice.lfoDepth;

            voice.lfoPhase += twoPi * lfoHz / sampleRate;
            if (voice.lfoPhase >= twoPi)
                voice.lfoPhase -= twoPi;

            const float vibrato =
                std::pow (2.0f, (lfo * 2.0f) / 12.0f);
            float frequency = voice.frequency * vibrato;
            float mono = 0.0f;

            const auto advancePhase =
                [sampleRate] (double& phase, float hz)
                {
                    phase += juce::MathConstants<double>::twoPi
                           * static_cast<double> (hz) / sampleRate;
                    if (phase >= juce::MathConstants<double>::twoPi)
                        phase -= juce::MathConstants<double>::twoPi;
                };

            switch (voice.instrument)
            {
                case static_cast<int> (CarnivalInstrument::Kick):
                {
                    const float sweep =
                        1.0f + (2.2f + voice.character * 2.0f)
                        * std::exp (-voice.ageSeconds * 30.0f);
                    frequency *= sweep;

                    advancePhase (voice.phase1, frequency);
                    advancePhase (voice.phase2, frequency * 2.0f);

                    const float body =
                        std::sin (static_cast<float> (voice.phase1));
                    const float harmonic =
                        std::sin (static_cast<float> (voice.phase2));
                    const float click =
                        nextCarnivalNoise (track)
                        * std::exp (-voice.ageSeconds * 115.0f);

                    mono =
                        body * 0.84f
                        + harmonic * (0.06f + voice.character * 0.10f)
                        + click * (0.05f + voice.character * 0.16f);
                    mono = std::tanh (mono * (1.10f + voice.character * 0.75f));
                    break;
                }

                case static_cast<int> (CarnivalInstrument::Snare):
                {
                    advancePhase (voice.phase1, frequency);
                    advancePhase (voice.phase2, frequency * 1.47f);

                    const float body =
                        std::sin (static_cast<float> (voice.phase1)) * 0.62f
                        + std::sin (static_cast<float> (voice.phase2)) * 0.38f;
                    const float noise = nextCarnivalNoise (track);
                    const float noiseMix = 0.46f + voice.character * 0.34f;

                    mono =
                        body * (1.0f - noiseMix)
                        + noise * noiseMix;
                    mono = std::tanh (mono * 1.35f);
                    break;
                }

                case static_cast<int> (CarnivalInstrument::Hihat):
                {
                    const float noise = nextCarnivalNoise (track);
                    advancePhase (voice.phase1, frequency);
                    advancePhase (voice.phase2, frequency * 1.417f);
                    advancePhase (voice.phase3, frequency * 1.731f);

                    const float metal =
                        (std::sin (static_cast<float> (voice.phase1))
                       * std::sin (static_cast<float> (voice.phase2))
                       + std::sin (static_cast<float> (voice.phase3)) * 0.55f)
                        * 0.64f;
                    const float noiseMix = 0.34f + voice.character * 0.42f;
                    mono = metal * (1.0f - noiseMix) + noise * noiseMix;
                    mono = std::tanh (mono * 1.55f);
                    break;
                }

                case static_cast<int> (CarnivalInstrument::Chord):
                {
                    static constexpr int intervals[6][2]
                    {
                        { 4, 7 },   // MAJ
                        { 3, 7 },   // MIN
                        { 2, 7 },   // SUS2
                        { 5, 7 },   // SUS4
                        { 7, 12 },  // 5TH
                        { 12, 19 }  // OCT
                    };

                    const int chord =
                        juce::jlimit (
                            0, 5,
                            juce::roundToInt (voice.character * 5.0f));
                    const float ratio2 =
                        std::pow (
                            2.0f,
                            static_cast<float> (intervals[chord][0]) / 12.0f);
                    const float ratio3 =
                        std::pow (
                            2.0f,
                            static_cast<float> (intervals[chord][1]) / 12.0f);

                    advancePhase (voice.phase1, frequency);
                    advancePhase (voice.phase2, frequency * ratio2);
                    advancePhase (voice.phase3, frequency * ratio3);

                    const float a =
                        std::sin (static_cast<float> (voice.phase1));
                    const float b =
                        std::sin (static_cast<float> (voice.phase2));
                    const float c =
                        std::sin (static_cast<float> (voice.phase3));

                    mono = (a + b * 0.92f + c * 0.82f) / 2.74f;
                    mono = std::tanh (mono * 1.28f);
                    break;
                }

                case static_cast<int> (CarnivalInstrument::Tone):
                {
                    const float ratio =
                        1.0f + std::floor (voice.character * 4.0f);
                    advancePhase (voice.phase2, frequency * ratio);
                    const float mod =
                        std::sin (static_cast<float> (voice.phase2))
                        * (0.15f + voice.character * 2.85f);
                    advancePhase (voice.phase1, frequency);
                    mono =
                        std::sin (
                            static_cast<float> (voice.phase1) + mod);
                    break;
                }

                case static_cast<int> (CarnivalInstrument::Tom):
                {
                    const float sweep =
                        1.0f
                        + (0.55f + voice.character * 0.85f)
                        * std::exp (-voice.ageSeconds * 20.0f);
                    advancePhase (voice.phase1, frequency * sweep);
                    advancePhase (voice.phase2, frequency * sweep * 1.5f);
                    mono =
                        std::sin (static_cast<float> (voice.phase1)) * 0.88f
                        + std::sin (static_cast<float> (voice.phase2))
                            * (0.05f + voice.character * 0.12f);
                    mono = std::tanh (mono * 1.18f);
                    break;
                }

                default: // BASS
                {
                    advancePhase (voice.phase1, frequency);
                    advancePhase (voice.phase2, frequency * 0.5f);

                    const float phase =
                        static_cast<float> (voice.phase1 / twoPi);
                    const float saw = phase * 2.0f - 1.0f;
                    const float sine =
                        std::sin (static_cast<float> (voice.phase1));
                    const float sub =
                        std::sin (static_cast<float> (voice.phase2));

                    mono =
                        saw * (0.18f + voice.character * 0.28f)
                        + sine * 0.52f
                        + sub * 0.30f;
                    mono = std::tanh (mono * (1.20f + voice.character * 1.25f));
                    break;
                }
            }

            mono *= envelope * voice.volume * 0.38f;

            const float cutoff =
                180.0f
                + std::pow (voice.filter, 2.0f) * 15500.0f;
            const float coefficient =
                std::exp (
                    -2.0f * juce::MathConstants<float>::pi
                    * cutoff / static_cast<float> (sampleRate));
            const float filteredL =
                (1.0f - coefficient) * mono
                + coefficient * voice.filterStateL;
            const float filteredR =
                (1.0f - coefficient) * mono
                + coefficient * voice.filterStateR;
            voice.filterStateL = filteredL;
            voice.filterStateR = filteredR;

            const float pan =
                juce::jlimit (0.0f, 1.0f, voice.pan + lfo * 0.08f);
            const float gainL = std::sqrt (1.0f - pan);
            const float gainR = std::sqrt (pan);

            left += filteredL * gainL;
            right += filteredR * gainR;

            voice.ageSeconds +=
                1.0f / static_cast<float> (sampleRate);
        }

        buffer.setSample (0, sample, left);
        if (numChannels > 1)
            buffer.setSample (1, sample, right);
    }
}

bool FlowerStandaloneAudioProcessor::isPerformanceGateOpen() const noexcept
{
    return performanceActive.load (std::memory_order_acquire)
        || (performanceHold.load (std::memory_order_acquire)
            && performanceLatched.load (std::memory_order_acquire));
}

uint32_t FlowerStandaloneAudioProcessor::nextPerformanceRandom() noexcept
{
    uint32_t x = performanceRandomState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    performanceRandomState = x;
    return x;
}

int FlowerStandaloneAudioProcessor::performanceScaleLength() const noexcept
{
    switch (performanceScaleIndex.load (std::memory_order_relaxed))
    {
        case 0: return 5; // minor pentatonic
        default: return 7;
    }
}

int FlowerStandaloneAudioProcessor::performanceScaleSemitone (int degree) const noexcept
{
    static constexpr int minorPent[] { 0, 3, 5, 7, 10 };
    static constexpr int naturalMinor[] { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr int major[] { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int dorian[] { 0, 2, 3, 5, 7, 9, 10 };

    const int scale = performanceScaleIndex.load (std::memory_order_relaxed);
    const int length = performanceScaleLength();
    const int wrapped = ((degree % length) + length) % length;

    switch (scale)
    {
        case 1: return naturalMinor[wrapped];
        case 2: return major[wrapped];
        case 3: return dorian[wrapped];
        default: return minorPent[wrapped];
    }
}

int FlowerStandaloneAudioProcessor::performanceNoteForTouch (
    float x, float y) const noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);
    y = juce::jlimit (0.0f, 1.0f, y);

    const int root =
        48 + performanceRootClass.load (std::memory_order_relaxed);
    const int scale =
        performanceScaleIndex.load (std::memory_order_relaxed);

    if (scale == 4) // RANDOM scale config becomes chromatic in direct poly play.
    {
        const int semitone =
            juce::jlimit (0, 23, static_cast<int> (std::floor (x * 24.0f)));
        const int octaveShift = y > 0.78f ? 12 : (y < 0.22f ? -12 : 0);
        return juce::jlimit (24, 96, root + semitone + octaveShift);
    }

    const int length = performanceScaleLength();
    const int span = juce::jmax (1, length * 2);
    const int index =
        juce::jlimit (
            0, span - 1,
            static_cast<int> (std::floor (x * static_cast<float> (span))));
    const int degree = index % length;
    const int octave = index / length;
    const int octaveShift = y > 0.78f ? 12 : (y < 0.22f ? -12 : 0);

    return juce::jlimit (
        24, 96,
        root
        + performanceScaleSemitone (degree)
        + octave * 12
        + octaveShift);
}

int FlowerStandaloneAudioProcessor::nextPerformanceNote (int patternIndex)
{
    const int scaleLength = performanceScaleLength();
    int degree = 0;
    int octave = 0;

    switch (patternIndex)
    {
        case 0: // SINGLE
            degree = 0;
            break;

        case 1: // UP
            degree = performanceStep % scaleLength;
            break;

        case 2: // DOWN
            degree = scaleLength - 1 - (performanceStep % scaleLength);
            break;

        case 3: // UP / DOWN
        {
            const int span = juce::jmax (2, scaleLength * 2 - 2);
            const int p = performanceStep % span;
            degree = p < scaleLength ? p : span - p;
            break;
        }

        case 4: // SKIP
            degree = (performanceStep * 2) % scaleLength;
            octave = ((performanceStep / juce::jmax (1, scaleLength)) & 1);
            break;

        case 5: // OCTAVE
            degree = performanceStep % scaleLength;
            octave = (performanceStep / scaleLength) & 1;
            break;

        case 6: // RANDOM
            degree = static_cast<int> (nextPerformanceRandom() % static_cast<uint32_t> (scaleLength));
            octave = static_cast<int> ((nextPerformanceRandom() >> 8) & 1u);
            break;

        default: // CHAOS
            degree = static_cast<int> (nextPerformanceRandom() % static_cast<uint32_t> (scaleLength));
            octave = static_cast<int> ((nextPerformanceRandom() >> 8) % 3u) - 1;
            break;
    }

    const float y = performanceY.load (std::memory_order_relaxed);
    if (y > 0.72f && (performanceStep & 1) != 0 && patternIndex >= 4)
        ++octave;

    const int root = 48 + performanceRootClass.load (std::memory_order_relaxed);
    const bool randomScale =
        performanceScaleIndex.load (std::memory_order_relaxed) == 4;
    const int semitone = randomScale
        ? static_cast<int> (nextPerformanceRandom() % 12u)
        : performanceScaleSemitone (degree);
    const int note = root + semitone + octave * 12;

    ++performanceStep;
    return juce::jlimit (24, 96, note);
}

void FlowerStandaloneAudioProcessor::generatePerformanceMidi (
    juce::MidiBuffer& midi, int numSamples)
{
    if (numSamples <= 0)
        return;

    const auto releasePolyNotes =
        [this, &midi] ()
        {
            for (int touch = 0; touch < performancePolyTouchCount; ++touch)
            {
                auto& current =
                    performancePolyCurrentNote[static_cast<size_t> (touch)];

                if (current >= 0)
                {
                    midi.addEvent (
                        juce::MidiMessage::noteOff (touch + 1, current), 0);
                    current = -1;
                }
            }
        };

    if (performanceStopRequested.exchange (false, std::memory_order_acq_rel))
    {
        if (performanceCurrentNote >= 0)
            midi.addEvent (
                juce::MidiMessage::noteOff (1, performanceCurrentNote), 0);

        performanceCurrentNote = -1;
        releasePolyNotes();
        performanceSamplesUntilStep = 0.0;
        performanceStep = 0;
    }

    if (! isPerformanceGateOpen())
    {
        if (performanceCurrentNote >= 0)
            midi.addEvent (
                juce::MidiMessage::noteOff (1, performanceCurrentNote), 0);

        performanceCurrentNote = -1;
        releasePolyNotes();
        performanceSamplesUntilStep = 0.0;
        performanceStep = 0;
        return;
    }

    const float velocity = juce::jlimit (
        0.25f, 1.0f,
        0.58f + performanceY.load (std::memory_order_relaxed) * 0.32f);

    if (! performanceArpEnabled.load (std::memory_order_acquire))
    {
        bool anyPolyTouch = false;

        for (int touch = 0; touch < performancePolyTouchCount; ++touch)
        {
            const auto index = static_cast<size_t> (touch);
            const bool active =
                performancePolyTouchActive[index].load (
                    std::memory_order_acquire);
            auto& current = performancePolyCurrentNote[index];

            if (! active)
            {
                if (current >= 0)
                {
                    midi.addEvent (
                        juce::MidiMessage::noteOff (touch + 1, current), 0);
                    current = -1;
                }

                continue;
            }

            anyPolyTouch = true;

            const float x =
                performancePolyTouchX[index].load (
                    std::memory_order_relaxed);
            const float y =
                performancePolyTouchY[index].load (
                    std::memory_order_relaxed);
            const int desiredNote = performanceNoteForTouch (x, y);
            const float touchVelocity =
                juce::jlimit (0.28f, 1.0f, 0.48f + y * 0.48f);

            if (current != desiredNote)
            {
                if (current >= 0)
                    midi.addEvent (
                        juce::MidiMessage::noteOff (
                            touch + 1, current), 0);

                current = desiredNote;
                midi.addEvent (
                    juce::MidiMessage::noteOn (
                        touch + 1, current, touchVelocity), 0);
            }
        }

        if (performanceCurrentNote >= 0)
        {
            midi.addEvent (
                juce::MidiMessage::noteOff (1, performanceCurrentNote), 0);
            performanceCurrentNote = -1;
        }

        if (! anyPolyTouch)
        {
            // Physical-key/HOLD control stays playable when ARP is disabled.
            const int desiredNote = juce::jlimit (
                24, 96,
                48 + performanceRootClass.load (std::memory_order_relaxed));

            if (performanceCurrentNote != desiredNote)
            {
                performanceCurrentNote = desiredNote;
                midi.addEvent (
                    juce::MidiMessage::noteOn (
                        1, performanceCurrentNote, velocity), 0);
            }
        }

        performanceSamplesUntilStep = 0.0;
        performanceStep = 0;
        return;
    }

    releasePolyNotes();

    const float x = performanceX.load (std::memory_order_relaxed);
    const int patternIndex = juce::jlimit (
        0, 7, static_cast<int> (std::floor (x * 8.0f)));

    constexpr int stepsPerBeat = 2;

    const double bpm = static_cast<double> (
        performanceBpm.load (std::memory_order_relaxed));
    const double stepSamples =
        currentSampleRate * 60.0
        / juce::jmax (1.0, bpm)
        / static_cast<double> (stepsPerBeat);

    double eventPosition = performanceSamplesUntilStep;

    while (eventPosition < static_cast<double> (numSamples))
    {
        const int sampleOffset = juce::jlimit (
            0, numSamples - 1, juce::roundToInt (eventPosition));

        if (performanceCurrentNote >= 0)
            midi.addEvent (
                juce::MidiMessage::noteOff (1, performanceCurrentNote),
                sampleOffset);

        performanceCurrentNote = nextPerformanceNote (patternIndex);

        midi.addEvent (
            juce::MidiMessage::noteOn (
                1, performanceCurrentNote, velocity),
            sampleOffset);

        eventPosition += stepSamples;
    }

    performanceSamplesUntilStep =
        eventPosition - static_cast<double> (numSamples);
}


void FlowerStandaloneAudioProcessor::processPerformanceDreamy (
    juce::AudioBuffer<float>& buffer)
{
    const int capacity = performanceDreamyBuffer.getNumSamples();
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin (
        buffer.getNumChannels(), performanceDreamyBuffer.getNumChannels());

    if (capacity <= 64 || numSamples <= 0 || channels <= 0)
        return;

    const bool enabled =
        performanceGranularEnabled.load (std::memory_order_acquire)
        && performanceDreamyMode.load (std::memory_order_acquire)
        && isPerformanceGateOpen();

    const float x = performanceX.load (std::memory_order_relaxed);
    const float y = performanceY.load (std::memory_order_relaxed);
    const float bpm = performanceBpm.load (std::memory_order_relaxed);

    const auto wrapIndex = [capacity] (int position)
    {
        while (position < 0)
            position += capacity;
        while (position >= capacity)
            position -= capacity;
        return position;
    };

    auto nextRandomUnit = [this] ()
    {
        uint32_t value = performanceDreamyRandomState;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        performanceDreamyRandomState = value;
        return static_cast<float> (value & 0xffffu) / 65535.0f;
    };

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float dry[2] { 0.0f, 0.0f };
        for (int channel = 0; channel < channels; ++channel)
            dry[channel] = buffer.getSample (channel, sample);

        float dreamyWet[2] { 0.0f, 0.0f };
        float totalWindow = 0.0f;

        const int minimumHistory =
            juce::roundToInt (currentSampleRate * 0.75);

        if (enabled && performanceDreamySamplesFilled > minimumHistory)
        {
            const float beatSamples =
                static_cast<float> (currentSampleRate)
                * 60.0f / juce::jmax (50.0f, bpm);
            const int baseLoopLength = juce::jlimit (
                juce::roundToInt (currentSampleRate * 0.070),
                juce::roundToInt (currentSampleRate * 0.340),
                juce::roundToInt (beatSamples / (2.0f + y * 2.0f)));

            for (int voice = 0; voice < performanceDreamyVoiceCount; ++voice)
            {
                auto& active = performanceDreamyVoiceActive[static_cast<size_t> (voice)];
                auto& loopStart = performanceDreamyLoopStart[static_cast<size_t> (voice)];
                auto& loopLength = performanceDreamyLoopLength[static_cast<size_t> (voice)];
                auto& outputPhase = performanceDreamyOutputPhase[static_cast<size_t> (voice)];
                auto& localPosition = performanceDreamyLocalPosition[static_cast<size_t> (voice)];
                auto& playbackSpeed = performanceDreamyPlaybackSpeed[static_cast<size_t> (voice)];

                if (! active || loopLength <= 0
                    || outputPhase >= loopLength * (3 + voice))
                {
                    const float lengthScale = voice == 0 ? 0.82f : 1.18f;
                    loopLength = juce::jlimit (
                        256, capacity / 3,
                        juce::roundToInt (
                            static_cast<float> (baseLoopLength) * lengthScale));

                    const int maxLookback = juce::jmax (
                        loopLength + 2,
                        juce::jmin (
                            performanceDreamySamplesFilled - 2,
                            capacity - 2));
                    const int minLookback = juce::jmin (
                        maxLookback,
                        juce::jmax (
                            loopLength + 2,
                            loopLength * (2 + voice)));
                    const int spreadSamples = juce::jmax (
                        1, maxLookback - minLookback);

                    const int lookback =
                        minLookback
                        + juce::roundToInt (
                            nextRandomUnit()
                            * static_cast<float> (spreadSamples));

                    loopStart = wrapIndex (
                        performanceDreamyWritePosition - lookback);
                    outputPhase = 0;
                    localPosition =
                        voice == 0
                            ? 0.0f
                            : static_cast<float> (loopLength) * 0.43f;

                    // Voice 1 is a perfect fifth (+5); voice 2 is an octave (+12).
                    playbackSpeed = voice == 0 ? 1.3348398f : 2.0f;
                    active = true;
                }

                while (localPosition >= static_cast<float> (loopLength))
                    localPosition -= static_cast<float> (loopLength);

                const float phase =
                    localPosition / static_cast<float> (loopLength);
                const float window =
                    0.5f - 0.5f * std::cos (
                        juce::MathConstants<float>::twoPi * phase);

                const float readPosition =
                    static_cast<float> (loopStart) + localPosition;
                const int read0 = wrapIndex (
                    static_cast<int> (std::floor (readPosition)));
                const int read1 = wrapIndex (read0 + 1);
                const float fraction =
                    readPosition - std::floor (readPosition);

                for (int channel = 0; channel < channels; ++channel)
                {
                    const auto* source =
                        performanceDreamyBuffer.getReadPointer (channel);
                    const float fragment =
                        source[read0]
                        + (source[read1] - source[read0]) * fraction;
                    dreamyWet[channel] += fragment * window;
                }

                totalWindow += window;
                localPosition += playbackSpeed;
                ++outputPhase;
            }

            if (totalWindow > 0.0001f)
            {
                const float wetMix =
                    juce::jlimit (0.20f, 0.58f, 0.24f + y * 0.34f);
                const float drift =
                    0.90f + x * 0.10f;

                for (int channel = 0; channel < channels; ++channel)
                {
                    const float wetSample =
                        dreamyWet[channel] / totalWindow * drift;
                    buffer.setSample (
                        channel, sample,
                        dry[channel] + (wetSample - dry[channel]) * wetMix);
                }
            }
        }
        else
        {
            performanceDreamyVoiceActive = { false, false };
        }

        for (int channel = 0; channel < channels; ++channel)
            performanceDreamyBuffer.setSample (
                channel, performanceDreamyWritePosition, dry[channel]);

        performanceDreamyWritePosition =
            (performanceDreamyWritePosition + 1) % capacity;
        performanceDreamySamplesFilled =
            juce::jmin (capacity, performanceDreamySamplesFilled + 1);
    }
}

void FlowerStandaloneAudioProcessor::processPerformanceDelay (
    juce::AudioBuffer<float>& buffer)
{
    const int capacity = performanceDelayBuffer.getNumSamples();
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(),
                                     performanceDelayBuffer.getNumChannels());

    if (capacity <= 16 || numSamples <= 0 || channels <= 0)
        return;

    const bool gate = isPerformanceGateOpen();
    const bool delayEnabled =
        performanceDelayEnabled.load (std::memory_order_acquire);
    const float x = performanceX.load (std::memory_order_relaxed);
    const float y = performanceY.load (std::memory_order_relaxed);
    const float speed = performanceSpeed.load (std::memory_order_relaxed);

    const float delaySeconds = juce::jlimit (
        0.04f, 0.65f,
        0.075f + (1.0f - speed) * 0.27f + x * 0.14f);
    const float targetDelaySamples = juce::jlimit (
        1.0f, static_cast<float> (capacity - 2),
        delaySeconds * static_cast<float> (currentSampleRate));

    // The original MVP jumped the delay read head to a new integer sample
    // whenever swipe speed/X changed.  That produces a discontinuity (zipper/
    // crackle) during movement.  Smooth the read head and interpolate between
    // adjacent delay samples instead.
    performanceDelaySamplesSmoothed.setTargetValue (targetDelaySamples);
    performanceDelayFeedbackSmoothed.setTargetValue (
        gate && delayEnabled
            ? juce::jlimit (0.0f, 0.65f, 0.10f + y * 0.55f)
            : 0.0f);
    performanceDelayWetSmoothed.setTargetValue (
        gate && delayEnabled
            ? juce::jlimit (0.0f, 0.40f, 0.02f + y * 0.38f)
            : 0.0f);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float delaySamples = performanceDelaySamplesSmoothed.getNextValue();
        const float feedback = performanceDelayFeedbackSmoothed.getNextValue();
        const float wet = performanceDelayWetSmoothed.getNextValue();

        float readPosition =
            static_cast<float> (performanceDelayWritePosition) - delaySamples;
        while (readPosition < 0.0f)
            readPosition += static_cast<float> (capacity);
        while (readPosition >= static_cast<float> (capacity))
            readPosition -= static_cast<float> (capacity);

        const int read0 = static_cast<int> (std::floor (readPosition));
        const int read1 = (read0 + 1) % capacity;
        const float fraction = readPosition - static_cast<float> (read0);

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* output = buffer.getWritePointer (channel);
            auto* delay = performanceDelayBuffer.getWritePointer (channel);

            const float input = output[sample];
            const float delayed =
                delay[read0] + (delay[read1] - delay[read0]) * fraction;

            // feedback < 1.0 keeps the delay stable.  Do not hard-clip the
            // feedback buffer; hard clipping was another source of harshness.
            delay[performanceDelayWritePosition] = input + delayed * feedback;
            output[sample] = input + (delayed - input) * wet;
        }

        performanceDelayWritePosition = (performanceDelayWritePosition + 1) % capacity;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout FlowerStandaloneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::level, 1 }, "Level", 0.0f, 1.0f, 0.25f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::attack, 1 }, "Attack", skewedRange (0.001f, 5.0f, 0.12f), 0.01f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::decay, 1 }, "Decay", skewedRange (0.001f, 5.0f, 0.25f), 0.20f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::sustain, 1 }, "Sustain", 0.0f, 1.0f, 0.80f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::release, 1 }, "Release", skewedRange (0.001f, 8.0f, 0.40f), 0.40f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::cutoff, 1 }, "Cutoff", skewedRange (20.0f, 20000.0f, 3000.0f), 3000.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::resonance, 1 }, "Resonance", 0.1f, 12.0f, 0.7071f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::lfoRate, 1 }, "LFO Rate", skewedRange (0.01f, 20.0f, 1.0f), 1.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::lfoDepth, 1 }, "LFO Depth", 0.0f, 1.0f, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::lfoTarget, 1 }, "LFO Target",
        juce::StringArray { "OFF", "PITCH", "CUTOFF", "RESONANCE", "LEVEL" }, 0));

    parameters.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIDs::performanceRootConfig, 1 },
        "Performance Root", 0, 11, 0));
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::performanceScaleConfig, 1 },
        "Performance Scale",
        juce::StringArray { "MINOR PENT", "NATURAL MINOR", "MAJOR", "DORIAN", "RANDOM" },
        0));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::defaultEffectsEnabled, 1 },
        "Default Effects", true));
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::performanceYEffectConfig, 1 },
        "Y Effect",
        juce::StringArray { "GRANULAR", "DREAMY" }, 0));
    parameters.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIDs::performanceMidiChannelConfig, 1 },
        "MIDI Channel", 1, 16, 1));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::flowerEnabled, 1 }, "Flower On", false));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::flowerRecord, 1 }, "Flower Record", false));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::flowerOverdub, 1 }, "Flower Overdub", false));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flowerPosition, 1 }, "Flower Position", 0.0f, 1.0f, 0.5f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flowerSize, 1 }, "Flower Size", skewedRange (0.008f, 0.50f, 0.08f), 0.08f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flowerDensity, 1 }, "Flower Density", 0.0f, 1.0f, 0.55f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flowerSpread, 1 }, "Flower Spread", 0.0f, 1.0f, 0.20f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flowerHold, 1 }, "Flower Hold", 0.0f, 1.0f, 0.30f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flowerPitch, 1 }, "Flower Pitch", -12.0f, 12.0f, 0.0f));
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::flowerReverse, 1 }, "Flower Reverse", false));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flowerMix, 1 }, "Flower Mix", 0.0f, 1.0f, 0.65f));
    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flowerFeedback, 1 }, "Flower Feedback", 0.0f, 1.0f, 0.72f));

    return { parameters.begin(), parameters.end() };
}

void FlowerStandaloneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FlowerStandaloneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FlowerStandaloneAudioProcessor::createEditor()
{
    return new FlowerStandaloneAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlowerStandaloneAudioProcessor();
}
