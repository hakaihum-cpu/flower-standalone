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
    struct AudioOutputOption { juce::String type; juce::String name; };
    enum class DragParam {
        none, complex, bar, width, length, hold, effect,
        hazeMix, hazeTime, hazeAmount, hazeFilter, hazeRepeat, hazeMod,
        hazeReverb
    };
    void timerCallback() override;
    void paintMain (juce::Graphics&);
    void paintGlobalControls (juce::Graphics&);
    bool dotModeEnabled() const;
    juce::Colour uiColour (float alpha) const;
    void paintMediaFrame (juce::Graphics&, const juce::Image&,
                          juce::Rectangle<float>, int sourceKind, int sourceRevision);
    void rebuildDotLuma (const juce::Image&, int sourceKind, int sourceRevision);
    void rebuildDotDisplay (int width, int height, int sourceKind, int sourceRevision);
    void paintDotUiFrame (juce::Graphics&);
    void paintHaze (juce::Graphics&);
    void paintClouds (juce::Graphics&);
    void paintVideoEffect (juce::Graphics&, const juce::Image&, float x, float y);
    void paintVongonMode (juce::Graphics&, int effectMode);
    void paintVongonParameterPanel (juce::Graphics&, int effectMode);
    void setVongonParameterFromX (int mode, int index, float x);
    void paintChordBot (juce::Graphics&);
    void paintConfig (juce::Graphics&);
    void paintAudioConfig (juce::Graphics&);
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
    void refreshAudioOutputs();
    void selectAudioOutput (int index);
    void refreshAudioBufferStatus();
    bool applyAudioBufferMode (int mode);
    bool resetAudioDevice();
    juce::String makeAudioBufferStatus() const;
    juce::String currentOutputName() const;
    void refreshMidiInputs();
    void refreshMidiOutputs();
    void cycleMidiOutput (int direction);
    void updateMidiControllerFromPoint (juce::Point<float>);
    void updateCloudsXYPoint (juce::Point<float>, bool touchDown);
    void updateVongonXYPoint (juce::Point<float>, bool touchDown);
    void updateEurekaXYPoint (juce::Point<float>);
    int nextVisualFrame (int count, int avoid);
    void setChoiceActual (const char* id, int value);
    void updateInputGainFromX (float x);
    juce::String currentInputName() const;
    juce::String noteText (int midi) const;

    RealtimeChordFxAudioProcessor& processor;
    FramePack frames;
    FramePack eurekaFrames;
    FramePack vongonFrames;
    juce::Image currentFrame;
    juce::Image currentEurekaFrame;
    juce::Image currentVongonFrame;
    int loadedFrame = -1;
    int loadedEurekaFrame = -1;
    int loadedVongonFrame = -1;
    int eurekaFrameIndex = 0;
    int vongonFrameIndex = 0;
    float eurekaFramePhase = 0.0f;
    float vongonFramePhase = 0.0f;
    int videoEffectTick = 0;
    bool eurekaSwitchClipPending = false;
    bool vongonSwitchClipPending = false;
    int chordVisualCooldown = 0;
    int eurekaVisualCooldown = 0;
    int vongonVisualCooldown = 0;
    float chordVisualPreviousActivity = 0.0f;
    float eurekaVisualPreviousActivity = 0.0f;
    float vongonVisualPreviousActivity = 0.0f;
    uint32_t visualRandomState = 0x45464658u;
    bool configVisible = false;
    bool inputGainDragging = false;
    bool audioConfigVisible = false;
    bool midiControlConfigVisible = false;
    bool xyDragging = false;
    DragParam dragging = DragParam::none;
    std::vector<AudioInputOption> audioInputs;
    std::vector<AudioOutputOption> audioOutputs;
    juce::StringArray physicalInputNames;
    int selectedAudioInput = -1;
    int selectedAudioOutput = -1;
    int audioBufferMode = 0;
    int audioDefaultBufferFrames = -1;
    int audioFramesPerBurst = 0;
    int audioActualBufferFrames = 0;
    double audioActualSampleRate = 0.0;
    juce::String audioDeviceMessage;
    juce::String audioBufferError;
    juce::Array<juce::MidiDeviceInfo> midiInputs;
    juce::Array<juce::MidiDeviceInfo> midiOutputs;
    int midiInputRefreshTick = 0;
    int selectedMidiOutput = -1;
    int midiPresetSlot = 1;
    juce::String midiPresetMessage;
    int globalPresetSlot = 1;
    juce::String globalPresetMessage;
    juce::String lastAudioRouteError;
    bool l1Latched = false;
    bool r1Latched = false;
    bool chordBotEditMode = false;
    int chordBotEditSlot = -1;
    int chordBotEditRoot = 0;
    int chordBotEditQuality = 0;
    int chordBotPressedPad = -1;

    bool eurekaPanelVisible = false;
    bool eurekaXYTouchDown = false;
    bool cloudsXYTouchDown = false;
    bool vongonXYTouchDown = false;
    bool vongonPanelVisible = false;
    int vongonPanelPage = 0;
    int vongonDraggingParameter = -1;
    float eurekaXYX = 0.5f;
    float eurekaXYY = 0.5f;

    static constexpr int dotSourceW = 512;
    static constexpr int dotSourceH = 384;
    std::array<float, dotSourceW * dotSourceH> dotLuma {};
    juce::Image dotDisplayCache;
    int dotFrameRevision = 1;
    int dotEurekaRevision = 1;
    int dotVongonRevision = 1;
    int dotLumaSourceKind = -1;
    int dotLumaRevision = -1;
    int dotRenderedSourceKind = -1;
    int dotRenderedRevision = -1;
    int dotRenderedTick = -1;
    int dotUiTick = 0;
    int dotTimerDivider = 0;
    float dotActivity = 0.0f;

    static constexpr float design = 720.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RealtimeChordFxAudioProcessorEditor)
};