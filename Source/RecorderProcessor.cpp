#include "RecorderProcessor.h"
#include "RecorderEditor.h"
#include <cmath>

RecorderAudioProcessor::RecorderAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::mono(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto& v : validSamples) v.store (0);
    for (auto& slot : peaks)
        for (auto& p : slot) p.store (0.0f);
    for (auto& p : uiPlaying) p.store (false);
    for (auto& p : uiPlaybackProgress) p.store (0.0f);
}

bool RecorderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RecorderAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 1000.0 ? sampleRate : 48000.0;
    segmentSamples = juce::jmax (1, (int) std::llround (currentSampleRate * kSegmentSeconds));

    for (int i = 0; i < kSlots; ++i)
    {
        slotBuffers[(size_t) i].setSize (1, segmentSamples, false, true, false);
        slotBuffers[(size_t) i].clear();
        validSamples[(size_t) i].store (0);
        for (auto& p : peaks[(size_t) i]) p.store (0.0f);
    }

    writeSlot = -1;
    writePosition = 0;
    recordingWasEnabled = false;
    playPositions.fill (0);
    requestedPlayMask.store (0u);
    uiRecordingSlot.store (-1);
    for (int i = 0; i < kSlots; ++i)
    {
        uiPlaying[(size_t) i].store (false);
        uiPlaybackProgress[(size_t) i].store (0.0f);
    }
    internalBeatSamplesRemaining = 0.0;
    midiClockTicks = 0;
}

void RecorderAudioProcessor::setInternalBpm (int bpm) noexcept
{
    internalBpm.store (juce::jlimit (30, 300, bpm));
}

void RecorderAudioProcessor::requestPlaySlot (int slot) noexcept
{
    if (juce::isPositiveAndBelow (slot, kSlots))
        requestedPlayMask.fetch_or (1u << (uint32_t) slot);
}

bool RecorderAudioProcessor::isSlotPlaying (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, kSlots)
        ? uiPlaying[(size_t) slot].load()
        : false;
}

float RecorderAudioProcessor::getSlotPlaybackProgress (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, kSlots)
        ? uiPlaybackProgress[(size_t) slot].load()
        : 0.0f;
}

int RecorderAudioProcessor::getValidSamples (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, kSlots) ? validSamples[(size_t) slot].load() : 0;
}

float RecorderAudioProcessor::getPeak (int slot, int bin) const noexcept
{
    if (! juce::isPositiveAndBelow (slot, kSlots) || ! juce::isPositiveAndBelow (bin, kPeakBins))
        return 0.0f;
    return peaks[(size_t) slot][(size_t) bin].load();
}

void RecorderAudioProcessor::beginRecordingSegment()
{
    writeSlot = (writeSlot + 1) % kSlots;
    playPositions[(size_t) writeSlot] = 0;
    uiPlaying[(size_t) writeSlot].store (false);
    uiPlaybackProgress[(size_t) writeSlot].store (0.0f);

    writePosition = 0;
    slotBuffers[(size_t) writeSlot].clear();
    validSamples[(size_t) writeSlot].store (0);
    for (auto& p : peaks[(size_t) writeSlot]) p.store (0.0f);
    uiRecordingSlot.store (writeSlot);
}

void RecorderAudioProcessor::finishRecordingSegment()
{
    if (writeSlot >= 0)
        validSamples[(size_t) writeSlot].store (juce::jlimit (0, segmentSamples, writePosition));
    uiRecordingSlot.store (-1);
}

void RecorderAudioProcessor::beginPlayback (int slot)
{
    if (! juce::isPositiveAndBelow (slot, kSlots)) return;
    if (slot == uiRecordingSlot.load()) return;
    if (validSamples[(size_t) slot].load() <= 0) return;

    playPositions[(size_t) slot] = 0;
    uiPlaying[(size_t) slot].store (true);
    uiPlaybackProgress[(size_t) slot].store (0.0f);
}

int RecorderAudioProcessor::chooseRandomValidSlot()
{
    int candidates[kSlots] {};
    int count = 0;
    const int rec = uiRecordingSlot.load();
    for (int i = 0; i < kSlots; ++i)
        if (i != rec && validSamples[(size_t) i].load() > 0)
            candidates[count++] = i;

    if (count == 0) return -1;
    randomState = randomState * 1664525u + 1013904223u;
    return candidates[randomState % (uint32_t) count];
}

void RecorderAudioProcessor::maybeStartRandomPlayback()
{
    if (! randomEnabled.load()) return;

    for (int i = 0; i < kSlots; ++i)
        if (uiPlaying[(size_t) i].load())
            return;

    const int slot = chooseRandomValidSlot();
    if (slot >= 0) beginPlayback (slot);
}

void RecorderAudioProcessor::handleClock (const juce::MidiBuffer& midi, int numSamples)
{
    if (midiClockMode.load())
    {
        for (const auto metadata : midi)
        {
            const auto m = metadata.getMessage();
            if (m.isMidiStart() || m.isMidiContinue())
            {
                midiClockRunning = true;
                midiClockTicks = 0;
            }
            else if (m.isMidiStop())
            {
                midiClockRunning = false;
                midiClockTicks = 0;
            }
            else if (m.isMidiClock() && midiClockRunning)
            {
                if (++midiClockTicks >= 24)
                {
                    midiClockTicks = 0;
                    maybeStartRandomPlayback();
                }
            }
        }
        return;
    }

    const double beatSamples = currentSampleRate * 60.0 / (double) juce::jmax (30, internalBpm.load());
    if (internalBeatSamplesRemaining <= 0.0)
        internalBeatSamplesRemaining = beatSamples;

    internalBeatSamplesRemaining -= numSamples;
    while (internalBeatSamplesRemaining <= 0.0)
    {
        maybeStartRandomPlayback();
        internalBeatSamplesRemaining += beatSamples;
    }
}

void RecorderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const float* input = getTotalNumInputChannels() > 0 ? buffer.getReadPointer (0) : nullptr;

    float level = 0.0f;
    if (input != nullptr)
        for (int i = 0; i < numSamples; ++i) level = juce::jmax (level, std::abs (input[i]));
    inputLevel.store (0.85f * inputLevel.load() + 0.15f * level);

    const bool shouldRecord = recordingEnabled.load();
    if (shouldRecord && ! recordingWasEnabled)
        beginRecordingSegment();
    else if (! shouldRecord && recordingWasEnabled)
        finishRecordingSegment();
    recordingWasEnabled = shouldRecord;

    if (shouldRecord && input != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (writePosition >= segmentSamples)
            {
                finishRecordingSegment();

                if (writeSlot == kSlots - 1)
                {
                    recordingEnabled.store (false);
                    recordingWasEnabled = false;
                    break;
                }

                beginRecordingSegment();
            }

            const float s = input[i];
            slotBuffers[(size_t) writeSlot].setSample (0, writePosition, s);
            const int bin = juce::jlimit (0, kPeakBins - 1,
                                         (int) ((int64_t) writePosition * kPeakBins / segmentSamples));
            auto& peak = peaks[(size_t) writeSlot][(size_t) bin];
            float old = peak.load();
            const float a = std::abs (s);
            while (a > old && ! peak.compare_exchange_weak (old, a)) {}
            ++writePosition;
            validSamples[(size_t) writeSlot].store (writePosition);

            if (writePosition >= segmentSamples && writeSlot == kSlots - 1)
            {
                finishRecordingSegment();
                recordingEnabled.store (false);
                recordingWasEnabled = false;
                break;
            }
        }
    }

    const uint32_t requests = requestedPlayMask.exchange (0u);
    for (int slot = 0; slot < kSlots; ++slot)
        if ((requests & (1u << (uint32_t) slot)) != 0u)
            beginPlayback (slot);

    handleClock (midi, numSamples);

    for (int ch = 0; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    for (int slot = 0; slot < kSlots; ++slot)
    {
        if (! uiPlaying[(size_t) slot].load())
            continue;

        const int length = validSamples[(size_t) slot].load();
        const float* src = slotBuffers[(size_t) slot].getReadPointer (0);
        int& position = playPositions[(size_t) slot];
        int outPos = 0;

        while (outPos < numSamples && position < length)
        {
            const float s = src[position++];
            for (int ch = 0; ch < getTotalNumOutputChannels(); ++ch)
                buffer.addSample (ch, outPos, s);
            ++outPos;
        }

        uiPlaybackProgress[(size_t) slot].store (
            length > 0 ? (float) position / (float) length : 0.0f);

        if (position >= length)
        {
            position = 0;
            uiPlaying[(size_t) slot].store (false);
            uiPlaybackProgress[(size_t) slot].store (0.0f);
        }
    }
}

void RecorderAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::MemoryOutputStream out (dest, true);
    out.writeInt (0x52454331);
    out.writeInt (internalBpm.load());
    out.writeBool (randomEnabled.load());
    out.writeBool (midiClockMode.load());
}

void RecorderAudioProcessor::setStateInformation (const void* data, int size)
{
    juce::MemoryInputStream in (data, (size_t) juce::jmax (0, size), false);
    if (in.readInt() != 0x52454331) return;
    setInternalBpm (in.readInt());
    randomEnabled.store (in.readBool());
    midiClockMode.store (in.readBool());
}

juce::AudioProcessorEditor* RecorderAudioProcessor::createEditor()
{
    return new RecorderAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RecorderAudioProcessor();
}
