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

    static constexpr int carnivalTrackCount = 10;
    static constexpr int carnivalStepCount = 8;
    static constexpr int carnivalParamCount = 7;

    enum class CarnivalInstrument
    {
        Kick = 0,
        Snare,
        Hihat,
        Chord,
        Tone,
        Tom,
        Bass,
        Count
    };

    enum class CarnivalParam
    {
        Volume = 0,
        Pan,
        Filter,
        Pitch,
        Decay,
        LfoRate,
        LfoDepth
    };

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

    void setCarnivalEnabled (bool enabled) noexcept;
    bool isCarnivalEnabled() const noexcept
    {
        return carnivalEnabled.load (std::memory_order_acquire);
    }

    void setCarnivalPlaying (bool playing) noexcept;
    bool isCarnivalPlaying() const noexcept
    {
        return carnivalPlaying.load (std::memory_order_acquire);
    }

    void setCarnivalClockMidi (bool midiClock) noexcept;
    bool isCarnivalClockMidi() const noexcept
    {
        return carnivalClockMidi.load (std::memory_order_acquire);
    }

    void setCarnivalBpm (float bpm) noexcept;
    float getCarnivalBpm() const noexcept
    {
        return carnivalBpm.load (std::memory_order_relaxed);
    }

    int getCarnivalCurrentStep() const noexcept
    {
        return carnivalCurrentStep.load (std::memory_order_relaxed);
    }

    bool getCarnivalStepEnabled (int track, int step) const noexcept;
    void setCarnivalStepEnabled (int track, int step, bool enabled) noexcept;
    void toggleCarnivalStep (int track, int step) noexcept;
    int getCarnivalInstrument (int track) const noexcept;
    void setCarnivalInstrument (int track, int instrument) noexcept;
    void cycleCarnivalInstrument (int track, int delta) noexcept;
    float getCarnivalBaseParam (int track, int param) const noexcept;
    void setCarnivalBaseParam (int track, int param, float value) noexcept;
    bool getCarnivalParamLockEnabled (int track, int step, int param) const noexcept;
    float getCarnivalParamLockValue (int track, int step, int param) const noexcept;
    void setCarnivalParamLock (int track, int step, int param,
                               bool enabled, float value) noexcept;
    bool carnivalStepHasLocks (int track, int step) const noexcept;
    void clearCarnivalStepLocks (int track, int step) noexcept;
    void clearCarnivalPattern() noexcept;
    void previewCarnivalTrack (int track) noexcept;

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
    void handleCarnivalMidiClock (const juce::MidiBuffer& midi);
    void processCarnival (juce::AudioBuffer<float>& buffer);
    void triggerCarnivalStep (int step) noexcept;
    void triggerCarnivalTrack (int track, int step) noexcept;
    float nextCarnivalNoise (int track) noexcept;
    static constexpr int carnivalStepIndex (int track, int step) noexcept
    {
        return track * carnivalStepCount + step;
    }
    static constexpr int carnivalBaseParamIndex (int track, int param) noexcept
    {
        return track * carnivalParamCount + param;
    }
    static constexpr int carnivalLockIndex (int track, int step, int param) noexcept
    {
        return (track * carnivalStepCount + step) * carnivalParamCount + param;
    }

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

    std::atomic<bool> carnivalEnabled { false };
    std::atomic<bool> carnivalPlaying { false };
    std::atomic<bool> carnivalClockMidi { false };
    std::atomic<float> carnivalBpm { 120.0f };
    std::atomic<int> carnivalCurrentStep { -1 };
    std::atomic<bool> carnivalResetRequested { false };
    std::atomic<int> carnivalPreviewTrackRequested { -1 };

    std::array<std::atomic<bool>,
               carnivalTrackCount * carnivalStepCount> carnivalSteps {};
    std::array<std::atomic<int>, carnivalTrackCount> carnivalInstruments {};
    std::array<std::atomic<float>,
               carnivalTrackCount * carnivalParamCount> carnivalBaseParams {};
    std::array<std::atomic<bool>,
               carnivalTrackCount * carnivalStepCount * carnivalParamCount> carnivalLockEnabled {};
    std::array<std::atomic<float>,
               carnivalTrackCount * carnivalStepCount * carnivalParamCount> carnivalLockValues {};

    struct CarnivalVoiceState
    {
        bool active = false;
        int instrument = 0;
        double phase1 = 0.0;
        double phase2 = 0.0;
        double phase3 = 0.0;
        double lfoPhase = 0.0;
        float ageSeconds = 0.0f;
        float frequency = 110.0f;
        float volume = 0.75f;
        float pan = 0.5f;
        float filter = 0.75f;
        float pitch = 0.5f;
        float decay = 0.45f;
        float lfoRate = 0.20f;
        float lfoDepth = 0.0f;
        float filterStateL = 0.0f;
        float filterStateR = 0.0f;
        uint32_t noiseState = 0x12345678u;
    };

    std::array<CarnivalVoiceState, carnivalTrackCount> carnivalVoices {};
    double carnivalSamplesUntilStep = 0.0;
    int carnivalMidiClockCounter = 0;
    bool carnivalMidiRunning = false;

    static constexpr int carnivalMidiTriggerCapacity = 32;
    std::array<int, carnivalMidiTriggerCapacity> carnivalMidiTriggerSamples {};
    std::array<int, carnivalMidiTriggerCapacity> carnivalMidiTriggerSteps {};
    int carnivalMidiTriggerCount = 0;

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
