#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
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
    bool producesMidi() const override { return true; }
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
    void setMidiControllerXY (int x, int y, bool touchDown) noexcept;
    void releaseMidiControllerTouch() noexcept;
    void notifyMidiControllerConfigChanged() noexcept { controllerDirty.store (true, std::memory_order_release); }
    int getMidiControllerX() const noexcept { return controllerX.load (std::memory_order_relaxed); }
    int getMidiControllerY() const noexcept { return controllerY.load (std::memory_order_relaxed); }
    bool saveMidiControllerPreset (int slot);
    bool loadMidiControllerPreset (int slot);
    bool hasMidiControllerPreset (int slot) const;
    void toggleMotionRecord() noexcept { motionCommand.store (1, std::memory_order_release); }
    void clearMotion() noexcept { motionCommand.store (2, std::memory_order_release); }
    int getMotionState() const noexcept { return motionState.load (std::memory_order_relaxed); }

private:
    void acceptPitch (const chordfx::PitchEstimate& estimate);
    void applyChord (const chordfx::ChordPlan& plan);
    void refreshPitchRatios();
    double barIntervalSamples() const;
    void handleMidiClock (const juce::MidiBuffer& midi);
    void advanceProgression();
    void processMidiController (juce::MidiBuffer&, int numSamples);
    int quantiseControllerNote (int value) const;
    int controllerFrameForXY (int x, int y) const noexcept;
    void setParameterActual (const char* id, float actual);
    void handleMotionCommand();
    void processMotionTick();
    void processInternalMotionClock (int numSamples);
    void applyMotionPoint (int x, int y);
    void processChordMidi (juce::MidiBuffer&);
    void stopActiveChordMidi (juce::MidiBuffer&);

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
    std::atomic<int> controllerX { 0 };
    std::atomic<int> controllerY { 0 };
    std::atomic<bool> controllerTouch { false };
    std::atomic<bool> controllerDirty { false };
    int activeControllerNoteX = -1;
    int activeControllerNoteY = -1;
    int activeControllerChannelX = 1;
    int activeControllerChannelY = 1;
    double controllerClockSamplesUntilNext = 0.0;
    bool controllerClockRunning = false;

    static constexpr int motionTicksPerBar = 96;
    static constexpr int maxMotionBars = 16;
    static constexpr int maxMotionTicks = motionTicksPerBar * maxMotionBars;
    std::array<juce::uint8, maxMotionTicks> motionX {};
    std::array<juce::uint8, maxMotionTicks> motionY {};
    std::atomic<int> motionCommand { 0 }; // 1=toggle record, 2=clear
    std::atomic<int> motionState { 0 };   // 0=empty/stopped, 1=recording, 2=playing
    int motionLengthTicks = 0;
    int motionPositionTicks = 0;
    int motionTargetTicks = motionTicksPerBar;
    double motionSamplesUntilNextTick = 0.0;

    static constexpr int maxChordMidiNotes = 16;
    std::array<int, maxChordMidiNotes> activeChordMidiNotes {};
    int activeChordMidiNoteCount = 0;
    int activeChordMidiChannel = 1;
    bool chordMidiRefreshRequested = false;
    bool chordMidiStopRequested = false;
    bool chordMidiGateOpen = false;

    mutable juce::SpinLock labelLock;
    juce::String chordLabel { "--" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RealtimeChordFxAudioProcessor)
};