#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "SynthVoice.h"

class FlowerStandaloneAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int flowerWaveformBins = 256;
    static constexpr int flowerGrainCount = 4;

    FlowerStandaloneAudioProcessor();
    ~FlowerStandaloneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FLOWER"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    void setPerformancePad (float x, float y, float speed, float horizontalDirection, bool active) noexcept;
    void setPerformanceHold (bool shouldHold) noexcept;
    void setPerformanceRoot (int noteClass) noexcept;
    void setPerformanceScale (int scaleIndex) noexcept;
    void setPerformanceBpm (float bpm) noexcept;
    void setPerformanceArpEnabled (bool enabled) noexcept;
    void setPerformanceDelayEnabled (bool enabled) noexcept;
    void setPerformanceGranularEnabled (bool enabled) noexcept;
    void setPerformanceDreamyMode (bool enabled) noexcept;
    void cycleFlowerTransport() noexcept;
    void stopPerformance() noexcept;

    int getConfiguredRoot() const noexcept;
    int getConfiguredScale() const noexcept;
    bool getDefaultEffectsEnabled() const noexcept;
    bool getConfiguredYEffectDreamy() const noexcept;
    int getConfiguredMidiChannel() const noexcept;
    float getPerformanceX() const noexcept { return performanceX.load (std::memory_order_relaxed); }
    float getPerformanceY() const noexcept { return performanceY.load (std::memory_order_relaxed); }
    float getPerformanceBpm() const noexcept { return performanceBpm.load (std::memory_order_relaxed); }
    bool getPerformanceArpEnabled() const noexcept { return performanceArpEnabled.load (std::memory_order_relaxed); }
    bool getPerformanceDelayEnabled() const noexcept { return performanceDelayEnabled.load (std::memory_order_relaxed); }
    bool getPerformanceYEffectEnabled() const noexcept { return performanceGranularEnabled.load (std::memory_order_relaxed); }
    bool getPerformanceHold() const noexcept { return performanceHold.load (std::memory_order_relaxed); }
    void setConfiguredRoot (int noteClass);
    void setConfiguredScale (int scaleIndex);
    void setDefaultEffectsEnabled (bool enabled);
    void setConfiguredYEffectDreamy (bool enabled);
    void setConfiguredMidiChannel (int channel);

    void getFlowerWaveform (std::array<float, flowerWaveformBins>& destination) const noexcept;
    bool hasFlowerLoop() const noexcept { return flowerLoopLengthSamples.load (std::memory_order_relaxed) > 0; }
    bool isFlowerRecording() const noexcept { return flowerRecordingActive.load (std::memory_order_relaxed); }
    float getFlowerRecordProgress() const noexcept { return flowerRecordProgress.load (std::memory_order_relaxed); }
    float getFlowerLoopValidFraction() const noexcept { return flowerLoopValidFraction.load (std::memory_order_relaxed); }
    float getFlowerBasePosition() const noexcept { return flowerBasePosition.load (std::memory_order_relaxed); }
    float getFlowerGrainPosition (int index) const noexcept;
    int getFlowerActiveGrains() const noexcept { return flowerActiveGrains.load (std::memory_order_relaxed); }
    void clearFlowerLoop() noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateSynthParams();
    void processFlower (juce::AudioBuffer<float>& buffer);
    void resetFlowerState() noexcept;
    float nextFlowerRandomBipolar() noexcept;

    void handlePerformanceMidiCC (juce::MidiBuffer& midi);
    void generatePerformanceMidi (juce::MidiBuffer& midi, int numSamples);
    void processPerformanceDelay (juce::AudioBuffer<float>& buffer);
    void processPerformanceDreamy (juce::AudioBuffer<float>& buffer);
    bool isPerformanceGateOpen() const noexcept;
    int nextPerformanceNote (int patternIndex);
    int performanceScaleLength() const noexcept;
    int performanceScaleSemitone (int degree) const noexcept;
    uint32_t nextPerformanceRandom() noexcept;

    juce::AudioProcessorValueTreeState apvts;
    juce::Synthesiser synthesiser;
    juce::MidiKeyboardState keyboardState;
    double currentSampleRate = 44100.0;

    juce::AudioBuffer<float> flowerLoopBuffer;
    int flowerRecordWritePosition = 0;
    int flowerLoopReadLength = 0;
    bool flowerLastRecordParam = false;
    bool flowerLastOverdubParam = false;
    bool flowerOverdubActive = false;
    bool flowerAnchorsInitialised = false;
    std::array<float, flowerGrainCount> flowerPhase { 0.0f, 0.25f, 0.5f, 0.75f };
    std::array<float, flowerGrainCount> flowerAnchor { 0.0f, 0.25f, 0.5f, 0.75f };
    std::array<int, flowerGrainCount> flowerRepeatCounter { 0, 0, 0, 0 };
    uint32_t flowerRandomState = 0x514D4959u;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> flowerPositionSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> flowerSizeSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> flowerDensitySmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> flowerSpreadSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> flowerHoldSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> flowerPitchSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> flowerMixSmoothed;
    std::array<std::atomic<float>, flowerWaveformBins> flowerWaveform {};
    std::array<std::atomic<float>, flowerGrainCount> flowerGrainPositions {};
    std::atomic<int> flowerLoopLengthSamples { 0 };
    std::atomic<bool> flowerRecordingActive { false };
    std::atomic<float> flowerRecordProgress { 0.0f };
    std::atomic<float> flowerLoopValidFraction { 0.0f };
    std::atomic<float> flowerBasePosition { 0.0f };
    std::atomic<int> flowerActiveGrains { 0 };
    std::atomic<bool> flowerClearRequested { false };
    std::atomic<int> flowerTransportRequest { 0 }; // 1=cycle, 2=clear
    std::atomic<int> flowerTransportState { 0 };   // 0=empty, 1=record, 2=stop, 3=overdub

    std::atomic<float> performanceX { 0.28f };
    std::atomic<float> performanceY { 0.28f };
    std::atomic<float> performanceSpeed { 0.0f };
    std::atomic<float> performanceDirection { 0.0f };
    std::atomic<bool> performanceActive { false };
    std::atomic<bool> performanceHold { false };
    std::atomic<bool> performanceLatched { false };
    std::atomic<int> performanceRootClass { 0 };
    std::atomic<int> performanceScaleIndex { 0 };
    std::atomic<float> performanceBpm { 112.0f };
    std::atomic<bool> performanceArpEnabled { true };
    std::atomic<bool> performanceDelayEnabled { true };
    std::atomic<bool> performanceGranularEnabled { true };
    std::atomic<bool> performanceDreamyMode { false };
    std::atomic<bool> performanceStopRequested { false };
    std::array<bool, 128> performanceCcGate {};

    static constexpr int performanceDreamyVoiceCount = 2;
    juce::AudioBuffer<float> performanceDreamyBuffer;
    int performanceDreamyWritePosition = 0;
    int performanceDreamySamplesFilled = 0;
    std::array<int, performanceDreamyVoiceCount> performanceDreamyLoopStart { 0, 0 };
    std::array<int, performanceDreamyVoiceCount> performanceDreamyLoopLength { 0, 0 };
    std::array<int, performanceDreamyVoiceCount> performanceDreamyOutputPhase { 0, 0 };
    std::array<float, performanceDreamyVoiceCount> performanceDreamyLocalPosition { 0.0f, 0.0f };
    std::array<float, performanceDreamyVoiceCount> performanceDreamyPlaybackSpeed { 1.3348398f, 2.0f };
    std::array<bool, performanceDreamyVoiceCount> performanceDreamyVoiceActive { false, false };
    uint32_t performanceDreamyRandomState = 0x44524541u;

    juce::AudioBuffer<float> performanceDelayBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> performanceDelaySamplesSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> performanceDelayFeedbackSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> performanceDelayWetSmoothed;
    juce::dsp::Limiter<float> performanceOutputLimiter;
    int performanceDelayWritePosition = 0;
    double performanceSamplesUntilStep = 0.0;
    int performanceStep = 0;
    int performanceCurrentNote = -1;
    uint32_t performanceRandomState = 0x46574C52u;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowerStandaloneAudioProcessor)
};
