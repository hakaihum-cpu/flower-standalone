#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"
#include <cmath>

RealtimeChordFxAudioProcessor::RealtimeChordFxAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::mono(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHORDFX_STATE", createParameterLayout())
{
}

bool RealtimeChordFxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
        && out == juce::AudioChannelSet::stereo();
}

void RealtimeChordFxAudioProcessor::prepareToPlay (double sr, int block)
{
    currentSampleRate = sr > 1000.0 ? sr : 48000.0;
    pitchDetector.prepare (currentSampleRate, 1024, 256);
    pitchBank.prepare (currentSampleRate, block);
    theory.reset();
    pendingMidi = -1;
    stableCount = 0;
    haveChord = false;
    samplesUntilChange = 0.0;
    gateSamplesRemaining = 0.0;
}

void RealtimeChordFxAudioProcessor::toggleRunState() noexcept
{
    if (running.load (std::memory_order_relaxed)) stopAndClear();
    else running.store (true, std::memory_order_release);
}

void RealtimeChordFxAudioProcessor::stopAndClear() noexcept
{
    running.store (false, std::memory_order_release);
    clearRequested.store (true, std::memory_order_release);
}

juce::String RealtimeChordFxAudioProcessor::getChordLabel() const
{
    const juce::SpinLock::ScopedLockType lock (labelLock);
    return chordLabel;
}

void RealtimeChordFxAudioProcessor::acceptPitch (const chordfx::PitchEstimate& e)
{
    if (! e.valid) return;
    const float midiFloat = 69.0f + 12.0f * std::log2 (e.hz / 440.0f);
    const int midi = juce::jlimit (0, 127, juce::roundToInt (midiFloat));
    lastInputMidiFloat = midiFloat;

    if (midi == pendingMidi) ++stableCount;
    else { pendingMidi = midi; stableCount = 1; }

    if (haveChord) refreshPitchRatios();
    if (stableCount < 2) return;

    const int previous = detectedMidi.load (std::memory_order_relaxed);
    if (midi == previous) return;
    detectedMidi.store (midi, std::memory_order_relaxed);

    if (! running.load (std::memory_order_acquire)) return;

    theory.setComplexity (apvts.getRawParameterValue (ParamID::complex)->load());
    theory.setWidth (apvts.getRawParameterValue (ParamID::width)->load());
    applyChord (theory.noteOn (midi));
    samplesUntilChange = barIntervalSamples();
}

void RealtimeChordFxAudioProcessor::refreshPitchRatios()
{
    if (! haveChord || currentPlan.midiNotes.empty()) return;
    std::vector<float> ratios;
    ratios.reserve (currentPlan.midiNotes.size());
    for (int target : currentPlan.midiNotes)
        ratios.push_back (std::pow (2.0f, ((float) target - lastInputMidiFloat) / 12.0f));
    pitchBank.setTargetRatios (ratios);
}

void RealtimeChordFxAudioProcessor::applyChord (const chordfx::ChordPlan& plan)
{
    if (plan.midiNotes.empty()) return;
    currentPlan = plan;
    haveChord = true;
    refreshPitchRatios();
    {
        const juce::SpinLock::ScopedLockType lock (labelLock);
        chordLabel = plan.label;
    }
    visualFrame.store ((visualFrame.load (std::memory_order_relaxed) + 1) % 300,
                       std::memory_order_relaxed);

    const float length = apvts.getRawParameterValue (ParamID::length)->load();
    if (length >= 0.995f) gateSamplesRemaining = -1.0;
    else gateSamplesRemaining = std::max (1.0, barIntervalSamples() * (0.08 + 0.92 * length));
}

void RealtimeChordFxAudioProcessor::advanceProgression()
{
    if (! haveChord) return;
    theory.setComplexity (apvts.getRawParameterValue (ParamID::complex)->load());
    theory.setWidth (apvts.getRawParameterValue (ParamID::width)->load());
    applyChord (theory.advance());
    samplesUntilChange = barIntervalSamples();
}

double RealtimeChordFxAudioProcessor::barIntervalSamples() const
{
    static constexpr double factors[] { 0.25, 0.5, 1.0, 2.0 };
    const int index = juce::jlimit (0, 3, juce::roundToInt (apvts.getRawParameterValue (ParamID::bar)->load()));
    const double bpm = juce::jlimit (40.0, 240.0, (double) apvts.getRawParameterValue (ParamID::internalBpm)->load());
    return currentSampleRate * 60.0 / bpm * 4.0 * factors[index];
}

void RealtimeChordFxAudioProcessor::handleMidiClock (const juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto& m = meta.getMessage();
        if (m.isMidiStart()) { midiClockTicks = 0; midiClockRunning = true; }
        else if (m.isMidiContinue()) midiClockRunning = true;
        else if (m.isMidiStop()) midiClockRunning = false;
        else if (m.isMidiClock() && midiClockRunning && running.load (std::memory_order_relaxed))
        {
            ++midiClockTicks;
            const int barIndex = juce::jlimit (0, 3, juce::roundToInt (apvts.getRawParameterValue (ParamID::bar)->load()));
            static constexpr int ticks[] { 24, 48, 96, 192 }; // 4/4, 24 PPQN
            const float length = apvts.getRawParameterValue (ParamID::length)->load();
            if (length < 0.995f && midiClockTicks >= ticks[barIndex])
            {
                midiClockTicks = 0;
                advanceProgression();
            }
        }
    }
}

void RealtimeChordFxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inChannels = juce::jmax (1, getTotalNumInputChannels());
    const bool midiClockMode = apvts.getRawParameterValue (ParamID::clockMode)->load() >= 0.5f;
    if (midiClockMode) handleMidiClock (midi);

    if (clearRequested.exchange (false, std::memory_order_acq_rel))
    {
        pitchDetector.reset();
        pitchBank.reset();
        theory.reset();
        haveChord = false;
        pendingMidi = -1; stableCount = 0;
        detectedMidi.store (-1, std::memory_order_relaxed);
        visualFrame.store (0, std::memory_order_relaxed);
        samplesUntilChange = gateSamplesRemaining = 0.0;
        midiClockTicks = 0;
        const juce::SpinLock::ScopedLockType lock (labelLock);
        chordLabel = "--";
    }

    float blockPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < juce::jmin (inChannels, buffer.getNumChannels()); ++ch)
            mono += buffer.getSample (ch, i);
        mono /= (float) juce::jmin (inChannels, buffer.getNumChannels());
        blockPeak = juce::jmax (blockPeak, std::abs (mono));

        const auto estimate = pitchDetector.pushSample (mono);
        if (estimate.valid) acceptPitch (estimate);

        const bool active = running.load (std::memory_order_relaxed);
        float out = mono;
        if (active && haveChord)
        {
            const bool gated = gateSamplesRemaining < 0.0 || gateSamplesRemaining > 0.0;
            const float wet = pitchBank.processSample (mono);
            out = gated ? wet : 0.0f;
            if (gateSamplesRemaining > 0.0) gateSamplesRemaining -= 1.0;

            const float length = apvts.getRawParameterValue (ParamID::length)->load();
            if (! midiClockMode && length < 0.995f)
            {
                samplesUntilChange -= 1.0;
                if (samplesUntilChange <= 0.0) advanceProgression();
            }
        }
        else
        {
            // Stopped = clear/bypass, so the input remains audible while the generator is idle.
            pitchBank.processSample (mono);
        }

        if (buffer.getNumChannels() >= 1) buffer.setSample (0, i, out);
        if (buffer.getNumChannels() >= 2) buffer.setSample (1, i, out);
    }
    inputLevel.store (0.82f * inputLevel.load (std::memory_order_relaxed) + 0.18f * blockPeak,
                      std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout RealtimeChordFxAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::complex, "COMPLEX", 0.0f, 1.0f, 0.25f));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::bar, "BAR", juce::StringArray { "1/4", "1/2", "1", "2" }, 2));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::width, "WIDTH", 0.0f, 1.0f, 0.35f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::length, "LENGTH", 0.0f, 1.0f, 0.70f));
    p.add (std::make_unique<juce::AudioParameterInt> (ParamID::midiChannel, "MIDI CH", 1, 16, 1));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::clockMode, "CLOCK", juce::StringArray { "Internal", "MIDI" }, 0));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::internalBpm, "BPM", 40.0f, 240.0f, 120.0f));
    return p;
}

void RealtimeChordFxAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RealtimeChordFxAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* RealtimeChordFxAudioProcessor::createEditor()
{
    return new RealtimeChordFxAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RealtimeChordFxAudioProcessor();
}