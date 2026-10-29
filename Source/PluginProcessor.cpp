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
}

FlowerStandaloneAudioProcessor::FlowerStandaloneAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FLOWER_STATE", createParameterLayout())
{
   #if JUCE_ANDROID
    juce::Logger::writeToLog ("FLOWER_STARTUP P1 processor_ctor_begin");
   #endif

    for (int i = 0; i < 8; ++i)
        synthesiser.addVoice (new SineVoice());
    synthesiser.addSound (new SineSound());

   #if JUCE_ANDROID
    juce::Logger::writeToLog ("FLOWER_STARTUP P2 processor_ctor_end");
   #endif
}

bool FlowerStandaloneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void FlowerStandaloneAudioProcessor::prepareToPlay (double sampleRate, int)
{
   #if JUCE_ANDROID
    juce::Logger::writeToLog ("FLOWER_STARTUP P3 prepareToPlay_begin");
   #endif

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    synthesiser.setCurrentPlaybackSampleRate (currentSampleRate);

    const int flowerSamples = juce::jmax (4096, juce::roundToInt (currentSampleRate * 16.0));
    flowerLoopBuffer.setSize (2, flowerSamples, false, true, false);
    flowerLoopBuffer.clear();
    resetFlowerState();

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

   #if JUCE_ANDROID
    juce::Logger::writeToLog ("FLOWER_STARTUP P4 prepareToPlay_end");
   #endif
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

    for (int i = 0; i < synthesiser.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SineVoice*> (synthesiser.getVoice (i)))
            voice->setParams (params);
}

void FlowerStandaloneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    updateSynthParams();
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);
    synthesiser.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Actor-v3 parity: Flower always maintains the latest 16 seconds of the
    // generated standalone synth audio. The visible REC/DUB workflow from the
    // older written spec was no longer active in the current MIYAKO Actor-v3
    // implementation, so it is intentionally not reintroduced here.
    processFlower (buffer);
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

    const bool enabled = apvts.getRawParameterValue (ParamIDs::flowerEnabled)->load() > 0.5f;

    flowerRecordingActive.store (false, std::memory_order_relaxed);
    flowerOverdubActive = false;

    flowerPositionSmoothed.setTargetValue (
        juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParamIDs::flowerPosition)->load()));
    flowerSizeSmoothed.setTargetValue (
        juce::jlimit (0.008f, 0.50f, apvts.getRawParameterValue (ParamIDs::flowerSize)->load()));
    flowerDensitySmoothed.setTargetValue (
        juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParamIDs::flowerDensity)->load()));
    flowerSpreadSmoothed.setTargetValue (
        juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParamIDs::flowerSpread)->load()));
    flowerHoldSmoothed.setTargetValue (
        juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParamIDs::flowerHold)->load()));
    flowerPitchSmoothed.setTargetValue (
        juce::jlimit (-12.0f, 12.0f, apvts.getRawParameterValue (ParamIDs::flowerPitch)->load()));
    flowerMixSmoothed.setTargetValue (
        enabled ? juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParamIDs::flowerMix)->load()) : 0.0f);

    const bool reverse = apvts.getRawParameterValue (ParamIDs::flowerReverse)->load() > 0.5f;

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

        const int writePos = flowerRecordWritePosition;
        loopL[writePos] = dryL;
        loopR[writePos] = dryR;

        const int waveformBin = juce::jlimit (
            0, flowerWaveformBins - 1,
            static_cast<int> ((static_cast<int64_t> (writePos) * flowerWaveformBins) / capacity));
        const float magnitude = juce::jlimit (
            0.0f, 1.0f, juce::jmax (std::abs (dryL), std::abs (dryR)));
        flowerWaveform[static_cast<size_t> (waveformBin)].store (magnitude, std::memory_order_relaxed);

        flowerRecordWritePosition = (flowerRecordWritePosition + 1) % capacity;

        int loopLength = flowerLoopLengthSamples.load (std::memory_order_relaxed);
        if (loopLength < capacity)
            ++loopLength;
        flowerLoopLengthSamples.store (loopLength, std::memory_order_relaxed);
        flowerLoopReadLength = loopLength;

        flowerLoopValidFraction.store (
            static_cast<float> (loopLength) / static_cast<float> (capacity),
            std::memory_order_relaxed);
        flowerRecordProgress.store (
            static_cast<float> (flowerRecordWritePosition) / static_cast<float> (capacity),
            std::memory_order_relaxed);

        if (! enabled || loopLength <= 64)
        {
            flowerActiveGrains.store (0, std::memory_order_relaxed);
            continue;
        }

        const int oldest = loopLength >= capacity ? flowerRecordWritePosition : 0;

        const auto readLinear = [&] (const float* data, float logicalPosition)
        {
            logicalPosition = wrapPosition (logicalPosition, loopLength);
            const int i0 = juce::jlimit (0, loopLength - 1, static_cast<int> (logicalPosition));
            const int i1 = (i0 + 1) % loopLength;
            const float frac = logicalPosition - static_cast<float> (i0);

            const int p0 = (oldest + i0) % capacity;
            const int p1 = (oldest + i1) % capacity;
            return data[p0] + (data[p1] - data[p0]) * frac;
        };

        const float position = flowerPositionSmoothed.getNextValue();
        const float sizeSeconds = flowerSizeSmoothed.getNextValue();
        const float density = flowerDensitySmoothed.getNextValue();
        const float spread = flowerSpreadSmoothed.getNextValue();
        const float hold = flowerHoldSmoothed.getNextValue();
        const float pitch = flowerPitchSmoothed.getNextValue();
        const float mix = flowerMixSmoothed.getNextValue();

        const int activeGrains = juce::jlimit (1, flowerGrainCount,
            1 + juce::roundToInt (density * static_cast<float> (flowerGrainCount - 1)));
        flowerActiveGrains.store (activeGrains, std::memory_order_relaxed);

        const float grainSamples = juce::jlimit (
            64.0f, static_cast<float> (juce::jmax (64, loopLength)),
            sizeSeconds * static_cast<float> (currentSampleRate));
        const float pitchRatio = std::pow (2.0f, pitch / 12.0f);
        const float direction = reverse ? -1.0f : 1.0f;
        const int repeats = 1 + juce::roundToInt (hold * 15.0f);
        const float centreSample = position * static_cast<float> (loopLength - 1);
        const float grainTravel = direction * grainSamples * pitchRatio;

        if (! flowerAnchorsInitialised)
        {
            for (int grain = 0; grain < flowerGrainCount; ++grain)
            {
                const float scatter = nextFlowerRandomBipolar()
                                    * spread * 0.5f * static_cast<float> (loopLength);
                flowerAnchor[static_cast<size_t> (grain)] =
                    wrapPosition (centreSample + scatter - 0.5f * grainTravel, loopLength);
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
            const float window = std::sin (juce::MathConstants<float>::pi * phaseValue);
            const float weight = window * window;
            const float readPos = flowerAnchor[static_cast<size_t> (grain)]
                                + direction * phaseValue * grainSamples * pitchRatio;

            wetL += readLinear (loopL, readPos) * weight;
            wetR += readLinear (loopR, readPos) * weight;
            weightSum += weight;

            const float normalizedRead = wrapPosition (readPos, loopLength)
                                       / static_cast<float> (juce::jmax (1, loopLength - 1));
            flowerGrainPositions[static_cast<size_t> (grain)].store (
                juce::jlimit (0.0f, 1.0f, normalizedRead), std::memory_order_relaxed);

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
                                        * spread * 0.5f * static_cast<float> (loopLength);
                    flowerAnchor[static_cast<size_t> (grain)] =
                        wrapPosition (centreSample + scatter - 0.5f * grainTravel, loopLength);
                }
            }
        }

        for (int grain = activeGrains; grain < flowerGrainCount; ++grain)
            flowerGrainPositions[static_cast<size_t> (grain)].store (position, std::memory_order_relaxed);

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
