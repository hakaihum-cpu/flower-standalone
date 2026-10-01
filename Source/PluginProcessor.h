#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <string>
#include "GranularPitchBank.h"
#include "TheoryEngine.h"
#include "YinPitchDetector.h"

class RealtimeChordFxAudioProcessor final : public juce::AudioProcessor
{
public:
    RealtimeChordFxAudioProcessor();
    ~RealtimeChordFxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Realtime Chord FX"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void toggleRunState() noexcept;
    void stopAndClear() noexcept;
    bool isRunning() const noexcept { return running.load (std::memory_order_relaxed); }
    float getInputLevel() const noexcept { return inputLevel.load (std::memory_order_relaxed); }
    int getDetectedMidi() const noexcept { return detectedMidi.load (std::memory_order_relaxed); }
    int getVisualFrame() const noexcept { return visualFrame.load (std::memory_order_relaxed); }
    juce::String getChordLabel() const;

private:
    void acceptPitch (const chordfx::PitchEstimate& estimate);
    void applyChord (const chordfx::ChordPlan& plan);
    void refreshPitchRatios();
    double barIntervalSamples() const;
    void handleMidiClock (const juce::MidiBuffer& midi);
    void advanceProgression();

    juce::AudioProcessorValueTreeState apvts;
    chordfx::YinPitchDetector pitchDetector;
    chordfx::TheoryEngine theory;
    chordfx::GranularPitchBank pitchBank;

    double currentSampleRate = 48000.0;
    std::atomic<bool> running { false };
    std::atomic<bool> clearRequested { false };
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<int> detectedMidi { -1 };
    std::atomic<int> visualFrame { 0 };

    int pendingMidi = -1;
    int stableCount = 0;
    float lastInputMidiFloat = 60.0f;
    chordfx::ChordPlan currentPlan;
    bool haveChord = false;
    double samplesUntilChange = 0.0;
    double gateSamplesRemaining = 0.0;
    int midiClockTicks = 0;
    bool midiClockRunning = false;

    mutable juce::SpinLock labelLock;
    juce::String chordLabel { "--" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RealtimeChordFxAudioProcessor)
};