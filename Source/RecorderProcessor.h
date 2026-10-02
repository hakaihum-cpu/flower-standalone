#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>

class RecorderAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kSlots = 9;
    static constexpr int kPeakBins = 128;
    static constexpr double kSegmentSeconds = 5.0;

    RecorderAudioProcessor();
    ~RecorderAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RECORDER"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void setRecording (bool enabled) noexcept { recordingEnabled.store (enabled); }
    void toggleRecording() noexcept { setRecording (! isRecording()); }
    bool isRecording() const noexcept { return recordingEnabled.load(); }

    void setRandomMode (bool enabled) noexcept { randomEnabled.store (enabled); }
    void toggleRandomMode() noexcept { setRandomMode (! isRandomMode()); }
    bool isRandomMode() const noexcept { return randomEnabled.load(); }

    void setMidiClockMode (bool midi) noexcept { midiClockMode.store (midi); }
    void toggleClockMode() noexcept { setMidiClockMode (! isMidiClockMode()); }
    bool isMidiClockMode() const noexcept { return midiClockMode.load(); }

    void setInternalBpm (int bpm) noexcept;
    int getInternalBpm() const noexcept { return internalBpm.load(); }

    void requestPlaySlot (int slot) noexcept;
    int getRecordingSlot() const noexcept { return uiRecordingSlot.load(); }
    int getPlaybackSlot() const noexcept { return uiPlaybackSlot.load(); }
    float getPlaybackProgress() const noexcept { return uiPlaybackProgress.load(); }
    int getValidSamples (int slot) const noexcept;
    float getPeak (int slot, int bin) const noexcept;
    float getInputLevel() const noexcept { return inputLevel.load(); }

private:
    void beginRecordingSegment();
    void finishRecordingSegment();
    void beginPlayback (int slot);
    void maybeStartRandomPlayback();
    void handleClock (const juce::MidiBuffer& midi, int numSamples);
    int chooseRandomValidSlot();

    std::array<juce::AudioBuffer<float>, kSlots> slotBuffers;
    std::array<std::atomic<int>, kSlots> validSamples;
    std::array<std::array<std::atomic<float>, kPeakBins>, kSlots> peaks;

    double currentSampleRate = 48000.0;
    int segmentSamples = 240000;
    int writeSlot = -1;
    int writePosition = 0;
    bool recordingWasEnabled = false;

    int playSlot = -1;
    int playPosition = 0;
    std::atomic<int> requestedPlay { -1 };

    std::atomic<bool> recordingEnabled { false };
    std::atomic<bool> randomEnabled { false };
    std::atomic<bool> midiClockMode { false };
    std::atomic<int> internalBpm { 120 };
    std::atomic<float> inputLevel { 0.0f };

    std::atomic<int> uiRecordingSlot { -1 };
    std::atomic<int> uiPlaybackSlot { -1 };
    std::atomic<float> uiPlaybackProgress { 0.0f };

    double internalBeatSamplesRemaining = 0.0;
    int midiClockTicks = 0;
    bool midiClockRunning = true;
    uint32_t randomState = 0x52454331u;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecorderAudioProcessor)
};
