#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"
#include <cmath>

namespace
{
float softProtectSample (float sample) noexcept
{
    if (! std::isfinite (sample))
        return 0.0f;

    constexpr float threshold = 0.90f;
    constexpr float ceiling = 0.995f;
    const float magnitude = std::abs (sample);
    if (magnitude <= threshold)
        return sample;

    const float knee = ceiling - threshold;
    const float shaped =
        threshold + knee * std::tanh ((magnitude - threshold) / knee);
    return std::copysign (shaped, sample);
}

void softProtectBuffer (juce::AudioBuffer<float>& buffer) noexcept
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            data[sample] = softProtectSample (data[sample]);
    }
}
}

RealtimeChordFxAudioProcessor::RealtimeChordFxAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHORDFX_STATE", createParameterLayout())
{
    initialiseChordBotDefaults();
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
    sampleChordRenderer.prepare (currentSampleRate);
    sineArpeggiator.prepare (currentSampleRate);
    chordBRandomFx.prepare (currentSampleRate);
    hazeProcessor.prepare (currentSampleRate);
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
    lastEffectMode = juce::jlimit (0, 3, juce::roundToInt (
        apvts.getRawParameterValue (ParamID::effectMode)->load()));
    lastChordMode = juce::jlimit (0, 1, juce::roundToInt (
        apvts.getRawParameterValue (ParamID::chordMode)->load()));
    pitchSamplesSinceValid = 1000000;
    outputSafetyGain = 0.0f;
    outputSafetyWasActive = false;

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
    dreamyDelayLowpass = { 0.0f, 0.0f };
    const int dreamyDelaySamples = juce::jmax (4096, juce::roundToInt (currentSampleRate * 2.0));
    dreamyDelayBuffer.setSize (2, dreamyDelaySamples, false, true, false);
    dreamyDelayBuffer.clear();
    dreamyDelayWritePosition = 0;
    {
        const float y0 = juce::jlimit (0.0f, 1.0f,
            (float) controllerY.load (std::memory_order_relaxed) / 127.0f);
        dreamyDelaySamplesSmoothed =
            (float) (currentSampleRate * (0.16 + 0.36 * y0));
    }
    dreamyAmbienceSmoothed = 0.0f;
    dreamyVisualEnvelope = 0.0f;
    dreamyVisualPreviousMono = 0.0f;
    dreamyVisualSamplesUntilUpdate = 0;
    dreamyVisualSequence = 0;
    dreamyPostWasEnabled = false;
    dreamyReverb.setSampleRate (currentSampleRate);
    dreamyReverb.reset();

    chordReverb.setSampleRate (currentSampleRate);
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.48f;
        rp.damping = 0.38f;
        rp.wetLevel = 0.22f;
        rp.dryLevel = 0.82f;
        rp.width = 0.88f;
        rp.freezeMode = 0.0f;
        chordReverb.setParameters (rp);
    }
    chordReverb.reset();

    wetDryScratch.setSize (
        juce::jmax (2, getTotalNumOutputChannels()),
        juce::jmax (1, block), false, true, false);
    wetDryScratch.clear();

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
    applyChord (theory.noteOn (midi), true);
    samplesUntilChange = barIntervalSamples();
}

void RealtimeChordFxAudioProcessor::applyChord (const chordfx::ChordPlan& plan, bool recaptureSample)
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

    // Both CHORD renderers consume the exact same TheoryEngine plan.
    // HOLD affects only CHORD-A sample/crossfade duration.
    sampleChordRenderer.setHold (
        apvts.getRawParameterValue (ParamID::hold)->load());
    sampleChordRenderer.setPlan (plan.midiNotes, lastInputMidiFloat, recaptureSample);
    sineArpeggiator.setPlan (plan.midiNotes);
}

void RealtimeChordFxAudioProcessor::advanceProgression()
{
    if (! haveChord) return;
    theory.setComplexity (apvts.getRawParameterValue (ParamID::complex)->load());
    theory.setWidth (apvts.getRawParameterValue (ParamID::width)->load());
    applyChord (theory.advance(), false);
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

void RealtimeChordFxAudioProcessor::resetModeAudioState (int mode)
{
    // Mode changes must not reset Dreamy history/state. The accepted effect
    // keeps collecting input continuously and only changes whether its wet
    // layer is audible.
    if (mode == 0)
    {
        sineArpeggiator.reset();
        chordBRandomFx.reset();
        chordReverb.reset();

        if (haveChord)
        {
            // CHORD-A keeps a private raw-input history even while DREAMY is
            // selected, then captures the latest phrase when CHORD returns.
            sampleChordRenderer.setPlan (currentPlan.midiNotes, lastInputMidiFloat, true);
            sineArpeggiator.setPlan (currentPlan.midiNotes);
        }
    }
}

void RealtimeChordFxAudioProcessor::processChordAudio (juce::AudioBuffer<float>& buffer)
{
    // CHORD-A: a recent real input phrase is sampled, then the same source is
    // resampled simultaneously to the C4..B5 ChordPlan voices.
    const int n = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();
    if (n <= 0 || channels <= 0)
        return;

    const bool audible =
        running.load (std::memory_order_acquire)
        && haveChord
        && chordMidiGateOpen;

    for (int i = 0; i < n; ++i)
    {
        const float dryLeft = buffer.getSample (0, i);
        const float dryRight = channels > 1 ? buffer.getSample (1, i) : dryLeft;
        const float chord = sampleChordRenderer.renderSample (audible);

        if (audible)
        {
            buffer.setSample (0, i, dryLeft * 0.78f + chord * 0.46f);
            if (channels > 1)
                buffer.setSample (1, i, dryRight * 0.78f + chord * 0.46f);
        }
    }

    if (running.load (std::memory_order_relaxed) && haveChord)
        processChordReverb (buffer);
}

void RealtimeChordFxAudioProcessor::processChordB (juce::AudioBuffer<float>& buffer)
{
    // CHORD-B: the detected note selects/advances the chord, while a random
    // 1/8-note sine arpeggiator plays only ChordPlan tones in C4..B5.
    const int n = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();
    if (n <= 0 || channels <= 0)
        return;

    const float bpm = apvts.getRawParameterValue (ParamID::internalBpm)->load();
    const float length = apvts.getRawParameterValue (ParamID::length)->load();
    const float effectAmount = apvts.getRawParameterValue (ParamID::effect)->load();
    sineArpeggiator.setTiming (bpm, length);
    chordBRandomFx.setProbability (effectAmount);

    const bool audible =
        running.load (std::memory_order_acquire)
        && haveChord
        && chordMidiGateOpen;

    for (int i = 0; i < n; ++i)
    {
        const float dryLeft = buffer.getSample (0, i);
        const float dryRight = channels > 1 ? buffer.getSample (1, i) : dryLeft;
        const float arp = sineArpeggiator.renderSample (audible);

        if (sineArpeggiator.consumeNoteTrigger())
            chordBRandomFx.chooseForNote();

        float effectedLeft = arp;
        float effectedRight = arp;
        chordBRandomFx.processSample (arp, effectedLeft, effectedRight);

        if (audible)
        {
            buffer.setSample (0, i, dryLeft * 0.68f + effectedLeft * 0.52f);
            if (channels > 1)
                buffer.setSample (1, i, dryRight * 0.68f + effectedRight * 0.52f);
        }
    }

    if (running.load (std::memory_order_relaxed) && haveChord)
    {
        processChordReverb (buffer);
        buffer.applyGain (0.80f); // extra CHORD-B output headroom
        softProtectBuffer (buffer);
    }
}

void RealtimeChordFxAudioProcessor::processChordReverb (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() <= 0)
        return;

    if (buffer.getNumChannels() >= 2)
        chordReverb.processStereo (buffer.getWritePointer (0),
                                   buffer.getWritePointer (1), n);
    else
        chordReverb.processMono (buffer.getWritePointer (0), n);
}

void RealtimeChordFxAudioProcessor::applyOutputSafety (
    juce::AudioBuffer<float>& buffer, bool active)
{
    const int samples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();
    if (samples <= 0 || channels <= 0)
        return;

    if (active && ! outputSafetyWasActive)
        outputSafetyGain = 0.0f;

    const float rampStep =
        1.0f / juce::jmax (1.0f, (float) currentSampleRate * 0.008f);

    for (int sample = 0; sample < samples; ++sample)
    {
        if (active)
            outputSafetyGain = juce::jmin (1.0f, outputSafetyGain + rampStep);
        else
            outputSafetyGain = 1.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            float value = buffer.getSample (channel, sample);
            if (! std::isfinite (value))
                value = 0.0f;

            if (active)
                value *= outputSafetyGain;

            buffer.setSample (channel, sample, softProtectSample (value));
        }
    }

    outputSafetyWasActive = active;
}


void RealtimeChordFxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int availableInputChannels =
        juce::jmax (1, juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels()));

    if (wetDryScratch.getNumChannels() < buffer.getNumChannels()
        || wetDryScratch.getNumSamples() < n)
    {
        wetDryScratch.setSize (
            buffer.getNumChannels(), n, true, true, false);
    }
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        wetDryScratch.copyFrom (ch, 0, buffer, ch, 0, n);

    const bool midiClockMode =
        apvts.getRawParameterValue (ParamID::clockMode)->load() >= 0.5f;
    const int effectMode = juce::jlimit (0, 3, juce::roundToInt (
        apvts.getRawParameterValue (ParamID::effectMode)->load()));
    const int chordMode = juce::jlimit (0, 1, juce::roundToInt (
        apvts.getRawParameterValue (ParamID::chordMode)->load()));

    if (effectMode != lastEffectMode)
    {
        lastEffectMode = effectMode;
        resetModeAudioState (effectMode);
    }
    if (effectMode == 0 && chordMode != lastChordMode)
    {
        lastChordMode = chordMode;
        sineArpeggiator.reset();
        chordBRandomFx.reset();
        chordReverb.reset();
        if (haveChord)
        {
            sampleChordRenderer.setPlan (currentPlan.midiNotes, lastInputMidiFloat, true);
            sineArpeggiator.setPlan (currentPlan.midiNotes);
        }
    }

    handleMotionCommand();
    if (midiClockMode) handleMidiClock (midi);
    else processInternalMotionClock (n);

    midi.clear();
    processMidiController (midi, n);

    if (clearRequested.exchange (false, std::memory_order_acq_rel))
    {
        pitchDetector.reset();
        theory.reset();
        sampleChordRenderer.reset();
        sineArpeggiator.reset();
        chordBRandomFx.reset();
        hazeProcessor.clear();
        chordReverb.reset();
        haveChord = false;
        pendingMidi = -1;
        stableCount = 0;
        pitchSamplesSinceValid = 1000000;
        detectedMidi.store (-1, std::memory_order_relaxed);

        visualFrame.store (controllerFrameForXY (controllerX.load(), controllerY.load()),
                           std::memory_order_relaxed);

        samplesUntilChange = gateSamplesRemaining = 0.0;
        midiClockTicks = 0;
        chordMidiGateOpen = false;
        chordMidiStopRequested = true;
        chordMidiRefreshRequested = false;
        dreamyVoiceActive = { false, false };
        dreamyDelayLowpass = { 0.0f, 0.0f };
        dreamyDelayBuffer.clear();
        dreamyDelayWritePosition = 0;
        dreamyAmbienceSmoothed = 0.0f;
        dreamyVisualEnvelope = 0.0f;
        dreamyVisualPreviousMono = 0.0f;
        dreamyVisualSamplesUntilUpdate = 0;
        dreamyVisualSequence = 0;
        outputSafetyGain = 0.0f;
        outputSafetyWasActive = false;
        dreamyPostWasEnabled = false;
        dreamyReverb.reset();

        const juce::SpinLock::ScopedLockType lock (labelLock);
        chordLabel = "--";
    }

    float blockPeak = 0.0f;
    double squareSum = 0.0;
    int nonZeroSamples = 0;
    int measuredSamples = 0;

    for (int i = 0; i < n; ++i)
    {
        const float left = buffer.getSample (0, i);
        const float right = availableInputChannels > 1 ? buffer.getSample (1, i) : left;
        const float mono = 0.5f * (left + right);
        sampleChordRenderer.pushInput (mono);

        for (int ch = 0; ch < availableInputChannels; ++ch)
        {
            const float sampleValue = buffer.getSample (ch, i);
            const float absolute = std::abs (sampleValue);
            blockPeak = juce::jmax (blockPeak, absolute);
            squareSum += (double) sampleValue * (double) sampleValue;
            if (absolute > 1.0e-6f)
                ++nonZeroSamples;
            ++measuredSamples;
        }

        if (pitchSamplesSinceValid < 100000000)
            ++pitchSamplesSinceValid;

        const auto estimate = pitchDetector.pushSample (mono);
        if (estimate.valid)
        {
            pitchSamplesSinceValid = 0;
            acceptPitch (estimate);
        }

        // Preserve discrete stereo input. Mono devices are duplicated only as
        // a compatibility fallback.
        buffer.setSample (0, i, left);
        if (buffer.getNumChannels() >= 2)
            buffer.setSample (1, i, right);
    }

    inputPeakRaw.store (blockPeak, std::memory_order_relaxed);
    inputRmsRaw.store (measuredSamples > 0
                       ? (float) std::sqrt (squareSum / (double) measuredSamples)
                       : 0.0f,
                       std::memory_order_relaxed);
    inputNonZeroRatio.store (measuredSamples > 0
                             ? (float) nonZeroSamples / (float) measuredSamples
                             : 0.0f,
                             std::memory_order_relaxed);
    inputLevel.store (0.82f * inputLevel.load (std::memory_order_relaxed)
                      + 0.18f * blockPeak,
                      std::memory_order_relaxed);

    // HAZE continuously records the raw input, even when another EFFECTS mode
    // is visible. Output bypass does not stop the record heads; LOCK does.
    hazeProcessor.setParams (hazeParamsFromState());

    if (hazeClearRequested.exchange (false, std::memory_order_acq_rel))
        hazeProcessor.clear();
    if (hazeShuffleRequested.exchange (false, std::memory_order_acq_rel))
        hazeProcessor.forceShuffle();

    hazeProcessor.captureBlock (buffer);

    if (chordAHoldRefreshRequested.exchange (false, std::memory_order_acq_rel)
        && haveChord)
    {
        sampleChordRenderer.setHold (
            apvts.getRawParameterValue (ParamID::hold)->load());
        sampleChordRenderer.setPlan (
            currentPlan.midiNotes, lastInputMidiFloat, true);
    }

    // Theory keeps running in either audio mode because CHORD MIDI OUT may be
    // used independently from the audible CHORD/DREAMY selection.
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

    // CHORDBOT owns MIDI only in mode 3. Stop the outgoing mode before
    // starting the incoming one so a same-note Note Off cannot cancel Note On.
    if (effectMode == 3)
    {
        processChordMidi (midi);
        processChordBotMidi (midi);
    }
    else
    {
        processChordBotMidi (midi);
        processChordMidi (midi);
    }

    // Dreamy always receives the raw live input so its history behaves like
    // the accepted standalone effect. It is wet only when DREAMY is selected.
    processDreamy (buffer);

    if (effectMode == 2)
        hazeProcessor.renderBlock (buffer);
    else if (effectMode == 0)
    {
        if (chordMode == 0)
            processChordAudio (buffer);
        else
            processChordB (buffer);
    }

    if (effectMode == 2 || effectMode == 3)
    {
        // EUREKA owns its saturation/level behaviour. CHORDBOT is MIDI-only.
        // Both therefore keep the dry audio path outside legacy protection.
        outputSafetyGain = 1.0f;
        outputSafetyWasActive = false;
    }
    else
    {
        const bool outputActive =
            running.load (std::memory_order_relaxed)
            && (effectMode == 1 || (haveChord && chordMidiGateOpen));
        applyOutputSafety (buffer, outputActive);
    }

    // Global WET is intentionally applied after the legacy effect protection so
    // WET=0 remains the untouched live input in every audible EFFECTS mode.
    if (effectMode != 3)
    {
        const float wet = juce::jlimit (
            0.0f, 1.0f,
            apvts.getRawParameterValue (ParamID::wet)->load());
        if (wet < 0.9999f)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* out = buffer.getWritePointer (ch);
                const auto* dry = wetDryScratch.getReadPointer (ch);
                for (int i = 0; i < n; ++i)
                    out[i] = dry[i] + (out[i] - dry[i]) * wet;
            }
        }
    }

    const float boostDb = juce::jlimit (
        0.0f, 14.0f,
        apvts.getRawParameterValue (ParamID::boostDb)->load());
    if (boostDb > 0.001f)
    {
        buffer.applyGain (juce::Decibels::decibelsToGain (boostDb));
        softProtectBuffer (buffer);
    }
}

void RealtimeChordFxAudioProcessor::processDreamy (juce::AudioBuffer<float>& buffer)
{
    const int capacity = dreamyBuffer.getNumSamples();
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin (
        buffer.getNumChannels(), dreamyBuffer.getNumChannels());

    if (capacity <= 64 || numSamples <= 0 || channels <= 0)
        return;

    // Map the accepted performance effect gate directly:
    // effect enabled -> running, Dreamy mode -> effectMode, performance gate
    // -> current XY touch / Motion playback.
    // XY values are latched on touch release. Dreamy continues using the
    // last X/Y position until the next touch; touch state itself is only for
    // interaction/MIDI note-off handling.
    const int selectedEffectMode =
        juce::jlimit (0, 3, juce::roundToInt (
            apvts.getRawParameterValue (ParamID::effectMode)->load()));
    const bool enabled =
        running.load (std::memory_order_acquire)
        && selectedEffectMode == 1;
    const float x = juce::jlimit (0.0f, 1.0f,
        (float) controllerX.load (std::memory_order_relaxed) / 127.0f);
    const float y = juce::jlimit (0.0f, 1.0f,
        (float) controllerY.load (std::memory_order_relaxed) / 127.0f);
    const float bpm = juce::jlimit (50.0f, 200.0f,
        apvts.getRawParameterValue (ParamID::internalBpm)->load());
    const float ambienceTarget =
        enabled ? std::pow (x * y, 1.35f) : 0.0f;
    const float ambienceAlpha =
        1.0f - std::exp (
            -(float) numSamples
            / juce::jmax (1.0f, (float) currentSampleRate * 0.050f));
    dreamyAmbienceSmoothed +=
        ambienceAlpha * (ambienceTarget - dreamyAmbienceSmoothed);
    const float ambience =
        juce::jlimit (0.0f, 1.0f, dreamyAmbienceSmoothed);

    // DREAMY visuals: XY remains the centre position. Audible activity
    // moves around that point in both axes, so the image visibly changes
    // instead of staying on one fixed row/column.
    float changePeak = 0.0f;
    float levelPeak = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float left = buffer.getSample (0, sample);
        const float right = channels > 1 ? buffer.getSample (1, sample) : left;
        const float mono = 0.5f * (left + right);
        changePeak = juce::jmax (
            changePeak, std::abs (mono - dreamyVisualPreviousMono));
        levelPeak = juce::jmax (levelPeak, std::abs (mono));
        dreamyVisualPreviousMono = mono;
    }

    const float visualTarget = juce::jlimit (
        0.0f, 1.0f, changePeak * 7.5f + levelPeak * 0.75f);
    dreamyVisualEnvelope =
        0.74f * dreamyVisualEnvelope + 0.26f * visualTarget;

    dreamyVisualSamplesUntilUpdate -= numSamples;
    if (enabled && dreamyVisualSamplesUntilUpdate <= 0)
    {
        const int baseFrame = controllerFrameForXY (
            controllerX.load (std::memory_order_relaxed),
            controllerY.load (std::memory_order_relaxed));
        const int baseRow = baseFrame / 20;
        const int baseColumn = baseFrame % 20;
        const int activity = juce::jlimit (
            0, 6, juce::roundToInt (
                dreamyVisualEnvelope * 5.0f
                + juce::jlimit (0.0f, 1.0f, changePeak * 8.0f)));

        static constexpr int dx[8] { 0, 1, -1, 2, -2, 3, -3, 0 };
        static constexpr int dy[8] { 0, -1, 1, 1, -1, 2, -2, 0 };

        int frame = baseFrame;
        if (activity > 0)
        {
            dreamyVisualSequence = (dreamyVisualSequence + 1) & 7;
            const int scale = juce::jlimit (1, 3, (activity + 1) / 2);
            const int col = juce::jlimit (
                0, 19, baseColumn + dx[dreamyVisualSequence] * scale);
            const int row = juce::jlimit (
                0, 14, baseRow + dy[dreamyVisualSequence]
                         * juce::jlimit (1, 2, scale));
            frame = row * 20 + col;
        }

        visualFrame.store (frame, std::memory_order_relaxed);
        dreamyVisualSamplesUntilUpdate =
            juce::jmax (1, juce::roundToInt (currentSampleRate / 12.0));
    }

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

        const int minimumHistory =
            juce::roundToInt (currentSampleRate * 0.75);

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
                auto& active = dreamyVoiceActive[static_cast<size_t> (voice)];
                auto& loopStart = dreamyLoopStart[static_cast<size_t> (voice)];
                auto& loopLength = dreamyLoopLength[static_cast<size_t> (voice)];
                auto& outputPhase = dreamyOutputPhase[static_cast<size_t> (voice)];
                auto& localPosition = dreamyLocalPosition[static_cast<size_t> (voice)];
                auto& playbackSpeed = dreamyPlaybackSpeed[static_cast<size_t> (voice)];

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
                            dreamySamplesFilled - 2,
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
                        dreamyWritePosition - lookback);
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
                        dreamyBuffer.getReadPointer (channel);
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
            dreamyVoiceActive = { false, false };
        }

        for (int channel = 0; channel < channels; ++channel)
            dreamyBuffer.setSample (
                channel, dreamyWritePosition, dry[channel]);

        dreamyWritePosition =
            (dreamyWritePosition + 1) % capacity;
        dreamySamplesFilled =
            juce::jmin (capacity, dreamySamplesFilled + 1);
    }


    // EFFECTS-only post stage. Delay time and ambience are smoothed so XY
    // motion cannot jump a read head or reverb mix from one block to the next.
    const bool postEnabled =
        enabled && ambience > 0.001f
        && dreamyDelayBuffer.getNumSamples() > 0;

    if (postEnabled)
    {
        const int delayCapacity = dreamyDelayBuffer.getNumSamples();
        const float targetDelaySamples =
            (float) (currentSampleRate * (0.16 + 0.36 * y));
        const float delaySmoothing =
            1.0f - std::exp (
                -1.0f / juce::jmax (1.0f, (float) currentSampleRate * 0.030f));
        const float delayMix = 0.46f * ambience;
        const float feedback = 0.18f + 0.42f * ambience;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            dreamyDelaySamplesSmoothed +=
                delaySmoothing
                * (targetDelaySamples - dreamyDelaySamplesSmoothed);

            double readPosition =
                (double) dreamyDelayWritePosition
                - (double) dreamyDelaySamplesSmoothed;
            while (readPosition < 0.0)
                readPosition += (double) delayCapacity;
            while (readPosition >= (double) delayCapacity)
                readPosition -= (double) delayCapacity;

            const int read0 = (int) std::floor (readPosition);
            const int read1 = (read0 + 1) % delayCapacity;
            const float readFrac =
                (float) (readPosition - (double) read0);

            float current[2] { 0.0f, 0.0f };
            float delayed[2] { 0.0f, 0.0f };

            for (int channel = 0; channel < channels; ++channel)
            {
                current[channel] = buffer.getSample (channel, sample);
                const float a =
                    dreamyDelayBuffer.getSample (channel, read0);
                const float b =
                    dreamyDelayBuffer.getSample (channel, read1);
                const float rawDelay = a + (b - a) * readFrac;
                auto& lp = dreamyDelayLowpass[(size_t) channel];
                lp += 0.24f * (rawDelay - lp);
                delayed[channel] = lp;
            }

            for (int channel = 0; channel < channels; ++channel)
            {
                const int other = channels > 1 ? 1 - channel : channel;
                const float delayWrite =
                    softProtectSample (
                        current[channel] + delayed[other] * feedback);
                const float mixed =
                    softProtectSample (
                        current[channel] + delayed[channel] * delayMix);
                dreamyDelayBuffer.setSample (
                    channel, dreamyDelayWritePosition, delayWrite);
                buffer.setSample (
                    channel, sample, mixed);
            }

            dreamyDelayWritePosition =
                (dreamyDelayWritePosition + 1) % delayCapacity;
        }

        juce::Reverb::Parameters reverbParams;
        reverbParams.roomSize = 0.38f + 0.57f * ambience;
        reverbParams.damping = 0.62f;
        reverbParams.wetLevel = 0.26f * ambience;
        reverbParams.dryLevel = 0.50f;
        reverbParams.width = 1.0f;
        reverbParams.freezeMode = 0.0f;
        dreamyReverb.setParameters (reverbParams);

        if (channels >= 2)
            dreamyReverb.processStereo (
                buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
        else
            dreamyReverb.processMono (
                buffer.getWritePointer (0), numSamples);

        dreamyPostWasEnabled = true;
    }
    else if (dreamyPostWasEnabled)
    {
        // Never freeze an old tail and revive it on the next DREAMY entry.
        // At this point the post-stage wet level is already near zero.
        dreamyDelayBuffer.clear();
        dreamyDelayWritePosition = 0;
        dreamyDelayLowpass = { 0.0f, 0.0f };
        dreamyReverb.reset();
        dreamyPostWasEnabled = false;
    }

    if (enabled)
        softProtectBuffer (buffer);

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
    const int selectedEffectMode =
        juce::jlimit (0, 3, juce::roundToInt (
            apvts.getRawParameterValue (ParamID::effectMode)->load()));
    const bool enabled =
        selectedEffectMode != 3
        && apvts.getRawParameterValue (ParamID::chordMidiOut)->load() >= 0.5f;
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

int RealtimeChordFxAudioProcessor::getChordBotSlotRoot (int index) const noexcept
{
    index = juce::jlimit (0, 8, index);
    return chordBotCodes[(size_t) index].load (std::memory_order_relaxed) & 0x0f;
}

int RealtimeChordFxAudioProcessor::getChordBotSlotQuality (int index) const noexcept
{
    index = juce::jlimit (0, 8, index);
    return (chordBotCodes[(size_t) index].load (std::memory_order_relaxed) >> 4) & 0x0f;
}

juce::String RealtimeChordFxAudioProcessor::getChordBotSlotLabel (int index) const
{
    static constexpr const char* roots[] =
        { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    static constexpr const char* qualities[] =
        { "MAJ","MIN","7","MAJ7","MIN7","DIM","SUS2","SUS4" };

    const int root = juce::jlimit (0, 11, getChordBotSlotRoot (index));
    const int quality = juce::jlimit (0, 7, getChordBotSlotQuality (index));
    return juce::String (roots[root]) + " " + qualities[quality];
}

void RealtimeChordFxAudioProcessor::initialiseChordBotDefaults()
{
    for (auto& code : chordBotCodes)
        code.store (0, std::memory_order_relaxed);

    chordBotCodes[0].store (0, std::memory_order_relaxed); // C MAJ
    regenerateChordBotSuggestions();
}

void RealtimeChordFxAudioProcessor::regenerateChordBotSuggestions()
{
    const int seedRoot = getChordBotSlotRoot (0);
    const int seedQuality = getChordBotSlotQuality (0);
    const auto suggestions =
        chordfx::TheoryEngine::chordBotSuggestions (
            seedRoot, seedQuality);

    for (int i = 0; i < 8; ++i)
    {
        const int root =
            juce::jlimit (0, 11, suggestions[(size_t) i].rootPitchClass);
        const int quality =
            juce::jlimit (0, 7, suggestions[(size_t) i].quality);
        chordBotCodes[(size_t) (i + 1)].store (
            root | (quality << 4), std::memory_order_relaxed);
    }
}

void RealtimeChordFxAudioProcessor::saveChordBotLayout()
{
    juce::StringArray values;
    for (int i = 0; i < 9; ++i)
    {
        values.add (juce::String (getChordBotSlotRoot (i)));
        values.add (juce::String (getChordBotSlotQuality (i)));
    }

    apvts.state.setProperty (
        "chordBotLayout", values.joinIntoString (","), nullptr);
}

void RealtimeChordFxAudioProcessor::loadChordBotLayout()
{
    const auto value =
        apvts.state.getProperty ("chordBotLayout").toString();
    const auto values =
        juce::StringArray::fromTokens (value, ",", "");

    if (values.size() != 18)
    {
        initialiseChordBotDefaults();
        return;
    }

    for (int i = 0; i < 9; ++i)
    {
        const int root =
            juce::jlimit (0, 11, values[i * 2].getIntValue());
        const int quality =
            juce::jlimit (0, 7, values[i * 2 + 1].getIntValue());
        chordBotCodes[(size_t) i].store (
            root | (quality << 4), std::memory_order_relaxed);
    }
}

void RealtimeChordFxAudioProcessor::setChordBotSlot (
    int index, int rootPitchClass, int quality)
{
    index = juce::jlimit (0, 8, index);
    const int root = juce::jlimit (0, 11, rootPitchClass);
    const int q = juce::jlimit (0, 7, quality);

    chordBotCodes[(size_t) index].store (
        root | (q << 4), std::memory_order_relaxed);

    // The top-left tile is the theory seed. Only editing it rebuilds the
    // remaining eight theory-ranked candidates.
    if (index == 0)
        regenerateChordBotSuggestions();

    saveChordBotLayout();
}

void RealtimeChordFxAudioProcessor::triggerChordBotPad (
    int index, bool down) noexcept
{
    if (down)
        chordBotRequestedPad.store (
            juce::jlimit (0, 8, index), std::memory_order_release);
    else
        chordBotRequestedPad.store (-1, std::memory_order_release);
}

void RealtimeChordFxAudioProcessor::processChordBotMidi (
    juce::MidiBuffer& out)
{
    const int selectedEffectMode =
        juce::jlimit (0, 3, juce::roundToInt (
            apvts.getRawParameterValue (ParamID::effectMode)->load()));
    const int requested =
        selectedEffectMode == 3
            ? chordBotRequestedPad.load (std::memory_order_acquire)
            : -1;
    const int channel =
        juce::jlimit (1, 16, juce::roundToInt (
            apvts.getRawParameterValue (
                ParamID::chordMidiChannel)->load()));

    const int activePad =
        chordBotActivePad.load (std::memory_order_relaxed);
    const bool channelChanged =
        activeChordBotNoteCount > 0
        && channel != activeChordBotMidiChannel;

    if (requested == activePad && ! channelChanged)
        return;

    for (int i = 0; i < activeChordBotNoteCount; ++i)
        out.addEvent (
            juce::MidiMessage::noteOff (
                activeChordBotMidiChannel,
                activeChordBotNotes[(size_t) i]), 0);

    activeChordBotNoteCount = 0;
    chordBotActivePad.store (-1, std::memory_order_relaxed);

    if (requested < 0)
        return;

    const int rootPc = getChordBotSlotRoot (requested);
    const int quality = getChordBotSlotQuality (requested);
    static constexpr int intervals[8][4] = {
        { 0, 4, 7, -1 },
        { 0, 3, 7, -1 },
        { 0, 4, 7, 10 },
        { 0, 4, 7, 11 },
        { 0, 3, 7, 10 },
        { 0, 3, 6, -1 },
        { 0, 2, 7, -1 },
        { 0, 5, 7, -1 }
    };

    const int rootMidi = 48 + juce::jlimit (0, 11, rootPc);
    activeChordBotMidiChannel = channel;

    for (int i = 0; i < 4; ++i)
    {
        const int interval =
            intervals[juce::jlimit (0, 7, quality)][i];
        if (interval < 0)
            continue;

        const int note =
            juce::jlimit (0, 127, rootMidi + interval);
        activeChordBotNotes[(size_t) activeChordBotNoteCount++] = note;
        out.addEvent (
            juce::MidiMessage::noteOn (
                activeChordBotMidiChannel, note,
                (juce::uint8) 127), 0);
    }

    chordBotActivePad.store (
        requested, std::memory_order_relaxed);
}

chordfx::HazeProcessor::Params
RealtimeChordFxAudioProcessor::hazeParamsFromState() const noexcept
{
    chordfx::HazeProcessor::Params p;
    p.mix = apvts.getRawParameterValue (ParamID::hazeMix)->load();
    p.time = apvts.getRawParameterValue (ParamID::hazeTime)->load();
    p.haze = apvts.getRawParameterValue (ParamID::hazeAmount)->load();
    p.filter = apvts.getRawParameterValue (ParamID::hazeFilter)->load();
    p.repeat = apvts.getRawParameterValue (ParamID::hazeRepeat)->load();
    p.mod = apvts.getRawParameterValue (ParamID::hazeMod)->load();
    p.reverb = apvts.getRawParameterValue (ParamID::hazeReverb)->load();
    p.speed = juce::jlimit (0, 2, juce::roundToInt (
        apvts.getRawParameterValue (ParamID::hazeSpeed)->load()));
    p.loops = juce::jlimit (0, 2, juce::roundToInt (
        apvts.getRawParameterValue (ParamID::hazeLoops)->load()));
    p.warble = juce::jlimit (0, 2, juce::roundToInt (
        apvts.getRawParameterValue (ParamID::hazeWarble)->load()));
    p.transpose =
        apvts.getRawParameterValue (ParamID::hazeTranspose)->load() >= 0.5f;
    p.echo =
        apvts.getRawParameterValue (ParamID::hazeEcho)->load() >= 0.5f;
    p.og =
        apvts.getRawParameterValue (ParamID::hazeOg)->load() >= 0.5f;
    p.lock =
        apvts.getRawParameterValue (ParamID::hazeLock)->load() >= 0.5f;
    p.bypass =
        apvts.getRawParameterValue (ParamID::hazeBypass)->load() >= 0.5f;
    p.highGain =
        apvts.getRawParameterValue (ParamID::hazeGain)->load() >= 0.5f;
    p.stereoPath =
        apvts.getRawParameterValue (ParamID::hazePath)->load() >= 0.5f;
    return p;
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

bool RealtimeChordFxAudioProcessor::saveUserPreset()
{
    auto snapshot = apvts.copyState();
    snapshot.removeProperty ("userPresetXml", nullptr);
    if (auto xml = snapshot.createXml())
    {
        apvts.state.setProperty ("userPresetXml", xml->toString(), nullptr);
        return true;
    }
    return false;
}

bool RealtimeChordFxAudioProcessor::loadUserPreset()
{
    const auto stored = apvts.state.getProperty ("userPresetXml").toString();
    if (stored.isEmpty())
        return false;

    auto xml = juce::XmlDocument::parse (stored);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    auto restored = juce::ValueTree::fromXml (*xml);
    if (! restored.isValid())
        return false;

    restored.setProperty ("userPresetXml", stored, nullptr);
    apvts.replaceState (restored);
    loadChordBotLayout();
    notifyMidiControllerConfigChanged();
    chordAHoldRefreshRequested.store (true, std::memory_order_release);
    return true;
}

bool RealtimeChordFxAudioProcessor::hasUserPreset() const
{
    return apvts.state.hasProperty ("userPresetXml")
        && apvts.state.getProperty ("userPresetXml").toString().isNotEmpty();
}

juce::AudioProcessorValueTreeState::ParameterLayout RealtimeChordFxAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add (std::make_unique<juce::AudioParameterChoice> (
        ParamID::effectMode, "MODE", juce::StringArray { "CHORD", "DREAMY", "EUREKA", "CHORD BOT" }, 0));
    p.add (std::make_unique<juce::AudioParameterChoice> (
        ParamID::chordMode, "CHORD ENGINE", juce::StringArray { "A", "B" }, 0));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::complex, "COMPLEX", 0.0f, 1.0f, 0.25f));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::bar, "BAR", juce::StringArray { "1/4", "1/2", "1", "2" }, 2));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::width, "WIDTH", 0.0f, 1.0f, 0.35f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::length, "LENGTH", 0.0f, 1.0f, 0.70f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::hold, "HOLD", 0.0f, 1.0f, 0.0f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::effect, "EFFECT", 0.0f, 1.0f, 0.0f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::wet, "WET", 0.0f, 1.0f, 1.0f));
    p.add (std::make_unique<juce::AudioParameterFloat> (
        ParamID::boostDb, "BOOST", juce::NormalisableRange<float> (0.0f, 14.0f, 0.5f), 0.0f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::hazeMix, "HAZE MIX", 0.0f, 1.0f, 0.55f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::hazeTime, "HAZE TIME", 0.0f, 1.0f, 0.50f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::hazeAmount, "HAZE AMOUNT", 0.0f, 1.0f, 0.50f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::hazeFilter, "HAZE FILTER", 0.0f, 1.0f, 0.50f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::hazeRepeat, "HAZE REPEAT", 0.0f, 1.0f, 0.0f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::hazeMod, "HAZE MOD", 0.0f, 1.0f, 0.50f));
    p.add (std::make_unique<juce::AudioParameterFloat> (ParamID::hazeReverb, "EUREKA HALL", 0.0f, 1.0f, 0.0f));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::hazeSpeed, "HAZE SPEED", juce::StringArray { ".5x", "1x", "2x" }, 1));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::hazeLoops, "HAZE LOOPS", juce::StringArray { "1", "2", "2+" }, 1));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::hazeWarble, "HAZE WARBLE", juce::StringArray { "OFF", "LIGHT", "HEAVY" }, 0));
    p.add (std::make_unique<juce::AudioParameterBool> (ParamID::hazeTranspose, "HAZE TRANSPOSE", false));
    p.add (std::make_unique<juce::AudioParameterBool> (ParamID::hazeEcho, "HAZE ECHO", false));
    p.add (std::make_unique<juce::AudioParameterBool> (ParamID::hazeOg, "HAZE OG", false));
    p.add (std::make_unique<juce::AudioParameterBool> (ParamID::hazeLock, "HAZE LOCK", false));
    p.add (std::make_unique<juce::AudioParameterBool> (ParamID::hazeBypass, "HAZE BYPASS", false));
    p.add (std::make_unique<juce::AudioParameterBool> (ParamID::hazeGain, "HAZE HI GAIN", false));
    p.add (std::make_unique<juce::AudioParameterChoice> (ParamID::hazePath, "HAZE PATH", juce::StringArray { "MONO", "STEREO" }, 1));
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
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadChordBotLayout();
        }
    }
}

juce::AudioProcessorEditor* RealtimeChordFxAudioProcessor::createEditor()
{
    return new RealtimeChordFxAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RealtimeChordFxAudioProcessor();
}