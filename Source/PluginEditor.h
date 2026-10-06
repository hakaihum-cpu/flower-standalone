#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include "FramePack.h"
#include "PluginProcessor.h"

class RealtimeChordFxAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit RealtimeChordFxAudioProcessorEditor (RealtimeChordFxAudioProcessor&);
    ~RealtimeChordFxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    struct AudioInputOption { juce::String type; juce::String name; };
    enum class DragParam {
        none, complex, bar, width, length, hold, effect,
        hazeMix, hazeTime, hazeAmount, hazeFilter, hazeRepeat, hazeMod,
        hazeReverb
    };
    void timerCallback() override;
    void paintMain (juce::Graphics&);
    void paintHaze (juce::Graphics&);
    void paintChordBot (juce::Graphics&);
    void paintConfig (juce::Graphics&);
    void paintMidiControlConfig (juce::Graphics&);
    void paintBar (juce::Graphics&, juce::Rectangle<float>, const juce::String&, float, const juce::String&);
    void setParameterFromX (DragParam, float designX);
    juce::Point<float> toDesign (juce::Point<float>) const;
    juce::Rectangle<float> parameterBounds (int index) const;
    juce::Rectangle<float> holdBounds() const;
    juce::Rectangle<float> hazeParameterBounds (int index) const;
    juce::Rectangle<float> hazeToggleBounds (int index) const;
    juce::Rectangle<float> chordBotPadBounds (int index) const;
    int chordBotPadAtPoint (juce::Point<float>) const;
    void refreshAudioInputs();
    void selectAudioInput (int index);
    void refreshMidiOutputs();
    void cycleMidiOutput (int direction);
    void updateMidiControllerFromPoint (juce::Point<float>);
    void updateEurekaMotionPoint (juce::Point<float>);
    int nextVisualFrame (int count, int avoid);
    void setChoiceActual (const char* id, int value);
    juce::String currentInputName() const;
    juce::String noteText (int midi) const;

    RealtimeChordFxAudioProcessor& processor;
    FramePack frames;
    FramePack eurekaFrames;
    juce::Image currentFrame;
    juce::Image currentEurekaFrame;
    int loadedFrame = -1;
    int loadedEurekaFrame = -1;
    int eurekaFrameIndex = 0;
    int chordVisualCooldown = 0;
    int eurekaVisualCooldown = 0;
    float chordVisualPreviousActivity = 0.0f;
    float eurekaVisualPreviousActivity = 0.0f;
    uint32_t visualRandomState = 0x45464658u;
    bool configVisible = false;
    bool midiControlConfigVisible = false;
    bool xyDragging = false;
    DragParam dragging = DragParam::none;
    std::vector<AudioInputOption> audioInputs;
    juce::StringArray physicalInputNames;
    int selectedAudioInput = -1;
    juce::Array<juce::MidiDeviceInfo> midiOutputs;
    int selectedMidiOutput = -1;
    int midiPresetSlot = 1;
    juce::String midiPresetMessage;
    juce::String lastAudioRouteError;
    bool l1Latched = false;
    bool r1Latched = false;
    bool chordBotEditMode = false;
    int chordBotEditSlot = -1;
    int chordBotEditRoot = 0;
    int chordBotEditQuality = 0;
    int chordBotPressedPad = -1;

    bool eurekaPanelVisible = false;
    bool eurekaMotionArmed = false;
    bool eurekaMotionRecording = false;
    bool eurekaMotionPlaying = false;
    bool eurekaMotionTouchDown = false;
    int eurekaMotionRecordIndex = 0;
    int eurekaMotionPlaybackIndex = 0;
    float eurekaMotionTouchX = 0.5f;
    float eurekaMotionTouchY = 0.5f;
    static constexpr int eurekaMotionSteps = 90; // 3 s at 30 Hz
    std::array<float, eurekaMotionSteps> eurekaMotionMix {};
    std::array<float, eurekaMotionSteps> eurekaMotionHaze {};

    static constexpr float design = 720.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RealtimeChordFxAudioProcessorEditor)
};