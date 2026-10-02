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
    motionState.store (0, std::memory_order_relaxed);
    motionCommand.store (0, std::memory_order_relaxed);
    motionLengthTicks = 0;
    motionPositionTicks = 0;
    motionTargetTicks = motionTicksPerBar;
    motionSamplesUntilNextTick = 0.0;
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
    if (apvts.getRawParameterValue (ParamID::midiControl)->load() < 0.5f)
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
        if (m.isMidiStart())
        {
            midiClockTicks = 0;
            midiClockRunning = true;
            if (motionState.load (std::memory_order_relaxed) == 2)
                motionPositionTicks = 0;
        }
        else if (m.isMidiContinue()) midiClockRunning = true;
        else if (m.isMidiStop()) midiClockRunning = false;
        else if (m.isMidiClock() && midiClockRunning)
        {
            processMotionTick();

            if (running.load (std::memory_order_relaxed))
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
}

void RealtimeChordFxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inChannels = juce::jmax (1, getTotalNumInputChannels());
    const bool midiClockMode = apvts.getRawParameterValue (ParamID::clockMode)->load() >= 0.5f;
    handleMotionCommand();
    if (midiClockMode) handleMidiClock (midi);
    else processInternalMotionClock (n);
    midi.clear();
    processMidiController (midi, n);

    if (clearRequested.exchange (false, std::memory_order_acq_rel))
    {
        pitchDetector.reset();
        pitchBank.reset();
        theory.reset();
        haveChord = false;
        pendingMidi = -1; stableCount = 0;
        detectedMidi.store (-1, std::memory_order_relaxed);
        if (apvts.getRawParameterValue (ParamID::midiControl)->load() < 0.5f)
            visualFrame.store (0, std::memory_order_relaxed);
        else
            visualFrame.store (controllerFrameForXY (controllerX.load(), controllerY.load()), std::memory_order_relaxed);
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


int RealtimeChordFxAudioProcessor::controllerFrameForXY (int x, int y) const noexcept
{
    x = juce::jlimit (0, 127, x);
    y = juce::jlimit (0, 127, y);
    return juce::jmin (14, y * 15 / 128) * 20 + juce::jmin (19, x * 20 / 128);
}

void RealtimeChordFxAudioProcessor::setMidiControllerXY (int x, int y, bool touchDown) noexcept
{
    x = juce::jlimit (0, 127, x);
    y = juce::jlimit (0, 127, y);
    controllerX.store (x, std::memory_order_relaxed);
    controllerY.store (y, std::memory_order_relaxed);
    controllerTouch.store (touchDown, std::memory_order_relaxed);
    controllerDirty.store (true, std::memory_order_release);
    if (apvts.getRawParameterValue (ParamID::midiControl)->load() >= 0.5f)
        visualFrame.store (controllerFrameForXY (x, y), std::memory_order_relaxed);
}

void RealtimeChordFxAudioProcessor::releaseMidiControllerTouch() noexcept
{
    controllerTouch.store (false, std::memory_order_relaxed);
    controllerDirty.store (true, std::memory_order_release);
}

int RealtimeChordFxAudioProcessor::quantiseControllerNote (int value) const
{
    value = juce::jlimit (0, 127, value);
    const int key = juce::jlimit (0, 11, juce::roundToInt (apvts.getRawParameterValue (ParamID::midiKey)->load()));
    const int scale = juce::jlimit (0, 4, juce::roundToInt (apvts.getRawParameterValue (ParamID::midiScale)->load()));
    auto allowed = [key, scale] (int note)
    {
        const int pc = (note - key + 120) % 12;
        if (scale == 0) return true;
        static constexpr int major[] = { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr int minor[] = { 0, 2, 3, 5, 7, 8, 10 };
        static constexpr int majorPent[] = { 0, 2, 4, 7, 9 };
        static constexpr int minorPent[] = { 0, 3, 5, 7, 10 };
        const int* notes = scale == 1 ? major : scale == 2 ? minor : scale == 3 ? majorPent : minorPent;
        const int count = scale <= 2 ? 7 : 5;
        for (int i = 0; i < count; ++i) if (pc == notes[i]) return true;
        return false;
    };
    if (allowed (value)) return value;
    for (int d = 1; d < 12; ++d)
    {
        if (value - d >= 0 && allowed (value - d)) return value - d;
        if (value + d <= 127 && allowed (value + d)) return value + d;
    }
    return value;
}

void RealtimeChordFxAudioProcessor::processMidiController (juce::MidiBuffer& out, int numSamples)
{
    const bool enabled = apvts.getRawParameterValue (ParamID::midiControl)->load() >= 0.5f;
    const int channel = juce::jlimit (1, 16, juce::roundToInt (apvts.getRawParameterValue (ParamID::midiChannel)->load()));
    const int xMode = juce::jlimit (0, 2, juce::roundToInt (apvts.getRawParameterValue (ParamID::midiXMode)->load()));
    const int yMode = juce::jlimit (0, 2, juce::roundToInt (apvts.getRawParameterValue (ParamID::midiYMode)->load()));
    const int x = controllerX.load (std::memory_order_relaxed);
    const int y = controllerY.load (std::memory_order_relaxed);
    const bool touching = controllerTouch.load (std::memory_order_relaxed);
    const bool dirty = controllerDirty.exchange (false, std::memory_order_acq_rel);

    auto stopNote = [&out] (int& note, int& noteChannel)
    {
        if (note >= 0) { out.addEvent (juce::MidiMessage::noteOff (noteChannel, note), 0); note = -1; }
    };

    if (! enabled)
    {
        stopNote (activeControllerNoteX, activeControllerChannelX);
        stopNote (activeControllerNoteY, activeControllerChannelY);
        if (controllerClockRunning) { out.addEvent (juce::MidiMessage::midiStop(), 0); controllerClockRunning = false; }
        return;
    }

    if (dirty)
    {
        auto axis = [&] (int mode, int value, const char* ccId, int& activeNote, int& activeChannel)
        {
            if (mode == 0)
            {
                stopNote (activeNote, activeChannel);
                const int cc = juce::jlimit (0, 127, juce::roundToInt (apvts.getRawParameterValue (ccId)->load()));
                out.addEvent (juce::MidiMessage::controllerEvent (channel, cc, value), 0);
            }
            else if (mode == 1)
            {
                const int next = quantiseControllerNote (value);
                if (! touching) stopNote (activeNote, activeChannel);
                else if (next != activeNote || channel != activeChannel)
                {
                    stopNote (activeNote, activeChannel);
                    out.addEvent (juce::MidiMessage::noteOn (channel, next, (juce::uint8) 127), 0);
                    activeNote = next;
                    activeChannel = channel;
                }
            }
            else stopNote (activeNote, activeChannel);
        };
        axis (xMode, x, ParamID::midiXCC, activeControllerNoteX, activeControllerChannelX);
        axis (yMode, y, ParamID::midiYCC, activeControllerNoteY, activeControllerChannelY);
    }

    const int clockValue = xMode == 2 ? x : (yMode == 2 ? y : -1);
    if (clockValue < 0)
    {
        if (controllerClockRunning) { out.addEvent (juce::MidiMessage::midiStop(), 0); controllerClockRunning = false; }
        return;
    }

    if (! controllerClockRunning)
    {
        out.addEvent (juce::MidiMessage::midiStart(), 0);
        controllerClockRunning = true;
        controllerClockSamplesUntilNext = 0.0;
    }

    const double bpm = 40.0 + 200.0 * (double) clockValue / 127.0;
    const double interval = currentSampleRate * 60.0 / (bpm * 24.0);
    while (controllerClockSamplesUntilNext < (double) numSamples)
    {
        out.addEvent (juce::MidiMessage::midiClock(), juce::jlimit (0, juce::jmax (0, numSamples - 1), juce::roundToInt (controllerClockSamplesUntilNext)));
        controllerClockSamplesUntilNext += interval;
    }
    controllerClockSamplesUntilNext -= numSamples;
}


void RealtimeChordFxAudioProcessor::applyMotionPoint (int x, int y)
{
    x = juce::jlimit (0, 127, x);
    y = juce::jlimit (0, 127, y);
    controllerX.store (x, std::memory_order_relaxed);
    controllerY.store (y, std::memory_order_relaxed);
    controllerTouch.store (true, std::memory_order_relaxed);
    controllerDirty.store (true, std::memory_order_release);
    if (apvts.getRawParameterValue (ParamID::midiControl)->load() >= 0.5f)
        visualFrame.store (controllerFrameForXY (x, y), std::memory_order_relaxed);
}

void RealtimeChordFxAudioProcessor::handleMotionCommand()
{
    const int command = motionCommand.exchange (0, std::memory_order_acq_rel);
    if (command == 0) return;

    if (command == 2)
    {
        motionState.store (0, std::memory_order_relaxed);
        motionLengthTicks = 0;
        motionPositionTicks = 0;
        motionSamplesUntilNextTick = 0.0;
        controllerTouch.store (false, std::memory_order_relaxed);
        controllerDirty.store (true, std::memory_order_release);
        return;
    }

    if (apvts.getRawParameterValue (ParamID::midiControl)->load() < 0.5f)
        return;

    const int state = motionState.load (std::memory_order_relaxed);
    if (state == 1)
    {
        if (motionPositionTicks > 0)
        {
            motionLengthTicks = motionPositionTicks;
            motionPositionTicks = 0;
            motionState.store (2, std::memory_order_relaxed);
            applyMotionPoint ((int) motionX[0], (int) motionY[0]);
            motionPositionTicks = motionLengthTicks > 1 ? 1 : 0;
        }
        else
        {
            motionState.store (0, std::memory_order_relaxed);
        }
        return;
    }

    motionTargetTicks = juce::jlimit (1, maxMotionBars,
        juce::roundToInt (apvts.getRawParameterValue (ParamID::motionBars)->load())) * motionTicksPerBar;
    controllerTouch.store (false, std::memory_order_relaxed);
    controllerDirty.store (true, std::memory_order_release);
    motionLengthTicks = 0;
    motionPositionTicks = 0;
    motionSamplesUntilNextTick = 0.0;
    motionState.store (1, std::memory_order_relaxed);
}

void RealtimeChordFxAudioProcessor::processMotionTick()
{
    if (apvts.getRawParameterValue (ParamID::midiControl)->load() < 0.5f)
        return;

    const int state = motionState.load (std::memory_order_relaxed);
    if (state == 1)
    {
        if (motionPositionTicks < motionTargetTicks && motionPositionTicks < maxMotionTicks)
        {
            motionX[(size_t) motionPositionTicks] = (juce::uint8) juce::jlimit (0, 127, controllerX.load (std::memory_order_relaxed));
            motionY[(size_t) motionPositionTicks] = (juce::uint8) juce::jlimit (0, 127, controllerY.load (std::memory_order_relaxed));
            ++motionPositionTicks;
        }

        if (motionPositionTicks >= motionTargetTicks || motionPositionTicks >= maxMotionTicks)
        {
            motionLengthTicks = motionPositionTicks;
            motionPositionTicks = 0;
            motionState.store (motionLengthTicks > 0 ? 2 : 0, std::memory_order_relaxed);
            if (motionLengthTicks > 0)
            {
                applyMotionPoint ((int) motionX[0], (int) motionY[0]);
                motionPositionTicks = motionLengthTicks > 1 ? 1 : 0;
            }
        }
        return;
    }

    if (state == 2 && motionLengthTicks > 0)
    {
        const int pos = juce::jlimit (0, motionLengthTicks - 1, motionPositionTicks);
        applyMotionPoint ((int) motionX[(size_t) pos], (int) motionY[(size_t) pos]);
        motionPositionTicks = (motionPositionTicks + 1) % motionLengthTicks;
    }
}

void RealtimeChordFxAudioProcessor::processInternalMotionClock (int numSamples)
{
    if (motionState.load (std::memory_order_relaxed) == 0)
        return;

    const double bpm = juce::jlimit (40.0, 240.0,
        (double) apvts.getRawParameterValue (ParamID::internalBpm)->load());
    const double interval = currentSampleRate * 60.0 / (bpm * 24.0);

    while (motionSamplesUntilNextTick < (double) numSamples)
    {
        processMotionTick();
        motionSamplesUntilNextTick += interval;
    }
    motionSamplesUntilNextTick -= numSamples;
}

void RealtimeChordFxAudioProcessor::setParameterActual (const char* id, float actual)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (actual));
}

bool RealtimeChordFxAudioProcessor::saveMidiControllerPreset (int slot)
{
    if (slot < 1 || slot > 8) return false;
    juce::StringArray values;
    for (const char* id : { ParamID::midiChannel, ParamID::midiXMode, ParamID::midiYMode, ParamID::midiXCC, ParamID::midiYCC, ParamID::midiKey, ParamID::midiScale })
        values.add (juce::String (juce::roundToInt (apvts.getRawParameterValue (id)->load())));
    values.add (juce::String (controllerX.load()));
    values.add (juce::String (controllerY.load()));
    apvts.state.setProperty ("midiControllerPreset" + juce::String (slot), values.joinIntoString (","), nullptr);
    return true;
}

bool RealtimeChordFxAudioProcessor::loadMidiControllerPreset (int slot)
{
    if (slot < 1 || slot > 8) return false;
    const auto value = apvts.state.getProperty ("midiControllerPreset" + juce::String (slot)).toString();
    if (value.isEmpty()) return false;
    const auto values = juce::StringArray::fromTokens (value, ",", "");
    if (values.size() != 9) return false;
    const char* ids[] = { ParamID::midiChannel, ParamID::midiXMode, ParamID::midiYMode, ParamID::midiXCC, ParamID::midiYCC, ParamID::midiKey, ParamID::midiScale };
    for (int i = 0; i < 7; ++i) setParameterActual (ids[i], (float) values[i].getIntValue());
    setMidiControllerXY (values[7].getIntValue(), values[8].getIntValue(), false);
    notifyMidiControllerConfigChanged();
    return true;
}

bool RealtimeChordFxAudioProcessor::hasMidiControllerPreset (int slot) const
{
    return slot >= 1 && slot <= 8
        && apvts.state.getProperty ("midiControllerPreset" + juce::String (slot)).toString().isNotEmpty();
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
    p.add (std::make_unique<juce::AudioParameterBool> (ParamID::midiControl, "MIDI CONTROL", false));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::midiXMode, "MIDI X MODE", juce::StringArray { "CC", "NOTE", "CLOCK" }, 0));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::midiYMode, "MIDI Y MODE", juce::StringArray { "CC", "NOTE", "CLOCK" }, 0));
    p.add (std::make_unique<juce::AudioParameterInt> (ParamID::midiXCC, "MIDI X CC", 0, 127, 1));
    p.add (std::make_unique<juce::AudioParameterInt> (ParamID::midiYCC, "MIDI Y CC", 0, 127, 74));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::midiKey, "MIDI KEY", juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::midiScale, "MIDI SCALE", juce::StringArray { "Chromatic", "Major", "Natural Minor", "Major Pent", "Minor Pent" }, 0));
    p.add (std::make_unique<juce::AudioParameterInt> (ParamID::motionBars, "MOTION BARS", 1, maxMotionBars, 1));
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