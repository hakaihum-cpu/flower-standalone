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
    activeChordMidiNoteCount = 0;
    activeChordMidiChannel = 1;
    chordMidiRefreshRequested = false;
    chordMidiStopRequested = false;
    chordMidiGateOpen = false;

    const int dreamySamples =
        juce::jmax (4096, juce::roundToInt (currentSampleRate * 2.5));
    dreamyBuffer.setSize (2, dreamySamples, false, true, false);
    dreamyBuffer.clear();
    dreamyWritePosition = 0;
    dreamySamplesFilled = 0;
    dreamyLoopStart = { 0, 0 };
    dreamyLoopLength = { 0, 0 };
    dreamyOutputPhase = { 0, 0 };
    dreamyLocalPosition = { 0.0f, 0.0f };
    dreamyPlaybackSpeed = { 1.3348398f, 2.0f };
    dreamyVoiceActive = { false, false };
    dreamyRandomState = 0x44524541u;

    inputPeakRaw.store (0.0f, std::memory_order_relaxed);
    inputRmsRaw.store (0.0f, std::memory_order_relaxed);
    inputNonZeroRatio.store (0.0f, std::memory_order_relaxed);
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

void RealtimeChordFxAudioProcessor::applyChord (const chordfx::ChordPlan& plan)
{
    if (plan.midiNotes.empty()) return;
    currentPlan = plan;
    haveChord = true;
    {
        const juce::SpinLock::ScopedLockType lock (labelLock);
        chordLabel = plan.label;
    }
    const float length = apvts.getRawParameterValue (ParamID::length)->load();
    if (length >= 0.995f) gateSamplesRemaining = -1.0;
    else gateSamplesRemaining = std::max (1.0, barIntervalSamples() * (0.08 + 0.92 * length));

    chordMidiGateOpen = true;
    chordMidiRefreshRequested = true;
    chordMidiStopRequested = false;
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
    const int availableInputChannels =
        juce::jmax (1, juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels()));
    const bool midiClockMode =
        apvts.getRawParameterValue (ParamID::clockMode)->load() >= 0.5f;

    handleMotionCommand();
    if (midiClockMode) handleMidiClock (midi);
    else processInternalMotionClock (n);

    midi.clear();
    processMidiController (midi, n);

    if (clearRequested.exchange (false, std::memory_order_acq_rel))
    {
        pitchDetector.reset();
        theory.reset();
        haveChord = false;
        pendingMidi = -1;
        stableCount = 0;
        detectedMidi.store (-1, std::memory_order_relaxed);

        visualFrame.store (controllerFrameForXY (controllerX.load(), controllerY.load()),
                           std::memory_order_relaxed);

        samplesUntilChange = gateSamplesRemaining = 0.0;
        midiClockTicks = 0;
        chordMidiGateOpen = false;
        chordMidiStopRequested = true;
        chordMidiRefreshRequested = false;
        const juce::SpinLock::ScopedLockType lock (labelLock);
        chordLabel = "--";
    }

    float blockPeak = 0.0f;
    double squareSum = 0.0;
    int nonZeroSamples = 0;

    // Build a clean stereo dry buffer from the selected live input. The audio
    // effect path no longer uses generated chord voices or GranularPitchBank.
    for (int i = 0; i < n; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < availableInputChannels; ++ch)
            mono += buffer.getSample (ch, i);
        mono /= (float) availableInputChannels;

        const float absolute = std::abs (mono);
        blockPeak = juce::jmax (blockPeak, absolute);
        squareSum += (double) mono * (double) mono;
        if (absolute > 1.0e-6f)
            ++nonZeroSamples;

        const auto estimate = pitchDetector.pushSample (mono);
        if (estimate.valid)
            acceptPitch (estimate);

        if (buffer.getNumChannels() >= 1) buffer.setSample (0, i, mono);
        if (buffer.getNumChannels() >= 2) buffer.setSample (1, i, mono);
    }

    inputPeakRaw.store (blockPeak, std::memory_order_relaxed);
    inputRmsRaw.store (n > 0 ? (float) std::sqrt (squareSum / (double) n) : 0.0f,
                       std::memory_order_relaxed);
    inputNonZeroRatio.store (n > 0 ? (float) nonZeroSamples / (float) n : 0.0f,
                             std::memory_order_relaxed);
    inputLevel.store (0.82f * inputLevel.load (std::memory_order_relaxed)
                      + 0.18f * blockPeak,
                      std::memory_order_relaxed);

    // Keep the theory engine only for optional CHORD MIDI OUT. It no longer
    // creates the audible signal.
    if (running.load (std::memory_order_relaxed) && haveChord)
    {
        if (gateSamplesRemaining > 0.0)
        {
            gateSamplesRemaining -= (double) n;
            if (gateSamplesRemaining <= 0.0)
            {
                gateSamplesRemaining = 0.0;
                chordMidiGateOpen = false;
                chordMidiStopRequested = true;
            }
        }

        const float length = apvts.getRawParameterValue (ParamID::length)->load();
        if (! midiClockMode && length < 0.995f)
        {
            samplesUntilChange -= (double) n;
            if (samplesUntilChange <= 0.0)
                advanceProgression();
        }
    }

    processChordMidi (midi);

    // Always feed live input into the Dreamy history. Running only controls
    // whether the wet micro-loop layer is heard, matching the accepted effect.
    processDreamy (buffer);
}

void RealtimeChordFxAudioProcessor::processDreamy (juce::AudioBuffer<float>& buffer)
{
    const int capacity = dreamyBuffer.getNumSamples();
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), dreamyBuffer.getNumChannels());

    if (capacity <= 64 || numSamples <= 0 || channels <= 0)
        return;

    const bool enabled = running.load (std::memory_order_acquire);
    const float x = juce::jlimit (0.0f, 1.0f,
        (float) controllerX.load (std::memory_order_relaxed) / 127.0f);
    const float y = juce::jlimit (0.0f, 1.0f,
        (float) controllerY.load (std::memory_order_relaxed) / 127.0f);
    const float bpm = juce::jlimit (50.0f, 200.0f,
        apvts.getRawParameterValue (ParamID::internalBpm)->load());

    const auto wrapIndex = [capacity] (int position)
    {
        while (position < 0) position += capacity;
        while (position >= capacity) position -= capacity;
        return position;
    };

    auto nextRandomUnit = [this] ()
    {
        uint32_t value = dreamyRandomState;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        dreamyRandomState = value;
        return static_cast<float> (value & 0xffffu) / 65535.0f;
    };

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float dry[2] { 0.0f, 0.0f };
        for (int channel = 0; channel < channels; ++channel)
            dry[channel] = buffer.getSample (channel, sample);

        float dreamyWet[2] { 0.0f, 0.0f };
        float totalWindow = 0.0f;

        const int minimumHistory = juce::roundToInt (currentSampleRate * 0.75);

        if (enabled && dreamySamplesFilled > minimumHistory)
        {
            const float beatSamples =
                static_cast<float> (currentSampleRate)
                * 60.0f / juce::jmax (50.0f, bpm);
            const int baseLoopLength = juce::jlimit (
                juce::roundToInt (currentSampleRate * 0.070),
                juce::roundToInt (currentSampleRate * 0.340),
                juce::roundToInt (beatSamples / (2.0f + y * 2.0f)));

            for (int voice = 0; voice < dreamyVoiceCount; ++voice)
            {
                auto& active = dreamyVoiceActive[(size_t) voice];
                auto& loopStart = dreamyLoopStart[(size_t) voice];
                auto& loopLength = dreamyLoopLength[(size_t) voice];
                auto& outputPhase = dreamyOutputPhase[(size_t) voice];
                auto& localPosition = dreamyLocalPosition[(size_t) voice];
                auto& playbackSpeed = dreamyPlaybackSpeed[(size_t) voice];

                if (! active || loopLength <= 0
                    || outputPhase >= loopLength * (3 + voice))
                {
                    const float lengthScale = voice == 0 ? 0.82f : 1.18f;
                    loopLength = juce::jlimit (
                        256, capacity / 3,
                        juce::roundToInt ((float) baseLoopLength * lengthScale));

                    const int maxLookback = juce::jmax (
                        loopLength + 2,
                        juce::jmin (dreamySamplesFilled - 2, capacity - 2));
                    const int minLookback = juce::jmin (
                        maxLookback,
                        juce::jmax (loopLength + 2, loopLength * (2 + voice)));
                    const int spreadSamples = juce::jmax (1, maxLookback - minLookback);

                    const int lookback =
                        minLookback
                        + juce::roundToInt (
                            nextRandomUnit() * static_cast<float> (spreadSamples));

                    loopStart = wrapIndex (dreamyWritePosition - lookback);
                    outputPhase = 0;
                    localPosition =
                        voice == 0 ? 0.0f : static_cast<float> (loopLength) * 0.43f;

                    // Accepted Dreamy voices: +5 semitones and +12 semitones.
                    playbackSpeed = voice == 0 ? 1.3348398f : 2.0f;
                    active = true;
                }

                while (localPosition >= static_cast<float> (loopLength))
                    localPosition -= static_cast<float> (loopLength);

                const float phase = localPosition / static_cast<float> (loopLength);
                const float window =
                    0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);

                const float readPosition = static_cast<float> (loopStart) + localPosition;
                const int read0 = wrapIndex ((int) std::floor (readPosition));
                const int read1 = wrapIndex (read0 + 1);
                const float fraction = readPosition - std::floor (readPosition);

                for (int channel = 0; channel < channels; ++channel)
                {
                    const auto* source = dreamyBuffer.getReadPointer (channel);
                    const float fragment =
                        source[read0] + (source[read1] - source[read0]) * fraction;
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
                const float drift = 0.90f + x * 0.10f;

                for (int channel = 0; channel < channels; ++channel)
                {
                    const float wetSample = dreamyWet[channel] / totalWindow * drift;
                    buffer.setSample (
                        channel, sample,
                        dry[channel] + (wetSample - dry[channel]) * wetMix);
                }
            }
        }
        else
        {
            dreamyVoiceActive = { false, false };
        }

        for (int channel = 0; channel < channels; ++channel)
            dreamyBuffer.setSample (
                channel, dreamyWritePosition, dry[channel]);

        dreamyWritePosition = (dreamyWritePosition + 1) % capacity;
        dreamySamplesFilled = juce::jmin (capacity, dreamySamplesFilled + 1);
    }
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


void RealtimeChordFxAudioProcessor::stopActiveChordMidi (juce::MidiBuffer& out)
{
    for (int i = 0; i < activeChordMidiNoteCount; ++i)
        out.addEvent (juce::MidiMessage::noteOff (activeChordMidiChannel, activeChordMidiNotes[(size_t) i]), 0);

    activeChordMidiNoteCount = 0;
}

void RealtimeChordFxAudioProcessor::processChordMidi (juce::MidiBuffer& out)
{
    const bool enabled = apvts.getRawParameterValue (ParamID::chordMidiOut)->load() >= 0.5f;
    const int channel = juce::jlimit (1, 16,
        juce::roundToInt (apvts.getRawParameterValue (ParamID::chordMidiChannel)->load()));
    const bool shouldSound = enabled
                          && running.load (std::memory_order_relaxed)
                          && haveChord
                          && chordMidiGateOpen
                          && ! currentPlan.midiNotes.empty();

    if (chordMidiStopRequested || ! shouldSound)
    {
        stopActiveChordMidi (out);
        chordMidiStopRequested = false;
        if (! shouldSound)
        {
            chordMidiRefreshRequested = false;
            return;
        }
    }

    const bool channelChanged = activeChordMidiNoteCount > 0 && activeChordMidiChannel != channel;
    if (channelChanged || chordMidiRefreshRequested || activeChordMidiNoteCount == 0)
    {
        stopActiveChordMidi (out);

        activeChordMidiChannel = channel;
        activeChordMidiNoteCount = juce::jmin ((int) currentPlan.midiNotes.size(), maxChordMidiNotes);

        for (int i = 0; i < activeChordMidiNoteCount; ++i)
        {
            const int note = juce::jlimit (0, 127, currentPlan.midiNotes[(size_t) i]);
            activeChordMidiNotes[(size_t) i] = note;
            out.addEvent (juce::MidiMessage::noteOn (activeChordMidiChannel, note, (juce::uint8) 127), 0);
        }

        chordMidiRefreshRequested = false;
    }
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
    p.add (std::make_unique<juce::AudioParameterBool> (ParamID::chordMidiOut, "CHORD MIDI OUT", false));
    p.add (std::make_unique<juce::AudioParameterInt> (ParamID::chordMidiChannel, "CHORD MIDI CH", 1, 16, 1));
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