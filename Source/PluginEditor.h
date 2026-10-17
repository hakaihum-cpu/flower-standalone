#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include "FramePack.h"
#include "PluginProcessor.h"
#include "VideoInteraction.h"

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
    void paintEffectsMenu (juce::Graphics&);
    void paintEffectsShortcut (juce::Graphics&);
    void chooseEffectFromMenu (int index);
    void drawMidiVideoTiles (juce::Graphics&);
    void applyMultiXY (float x, float y);
    void startMultiRecording();
    void stopMultiRecordingAndPlay();
    void clearMultiRecording();
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
    unsigned int lastVisualMidiSequence = 0;
    int midiTileDivisions = 1;
    int midiTileHoldTicks = 0; // hold successful MIDI tiling for ~2.5 s
    int midiTileTriggerNote = -1;
    int eurekaFrameIndex = 0;
    int vongonFrameIndex = 0;
    float eurekaFramePhase = 0.0f;
    float vongonFramePhase = 0.0f;
    float vongonVideoSpeed = 0.0f;
    float vongonVisualFx = 0.0f;
    float vongonPreviousX = 0.5f;
    float vongonPreviousY = 0.5f;
    int vongonRewindFrames = 0;
    int vongonFxCooldown = 0;
    int vongonFxStyle = 0;
    int vongonFxEvent = 0;
    int vongonClipCooldown = 0;
    std::uint32_t vongonParameterSignature = 0;
    int vongonParameterSignatureMode = -1;
    juce::Image vongonGhostFrame;
    juce::Image vongonRedFringe;
    juce::Image vongonCyanFringe;
    int vongonFringeRevision = -1;
    bool eurekaSwitchClipPending = false;
    int chordVisualCooldown = 0;
    int eurekaVisualCooldown = 0;
    int vongonVisualCooldown = 0;
    float chordVisualPreviousActivity = 0.0f;
    float eurekaVisualPreviousActivity = 0.0f;
    float vongonVisualPreviousActivity = 0.0f;
    uint32_t visualRandomState = 0x45464658u;
    bool configVisible = false;
    bool effectsMenuVisible = false;
    bool multiModeEnabled = false;
    // MULTI recorder lives on the 30Hz UI thread, independent of the
    // original MIDI motion-recorder and audio engine.
    int multiTransport = 0; // 0=idle 1=REC 2=PLAY
    static constexpr int multiRecordCapacity = 1800;
    std::array<juce::Point<float>, multiRecordCapacity> multiRecordedXY {};
    int multiRecordCount = 0;
    int multiPlaybackIndex = 0;
    float multiX = 0.25f;
    float multiY = 0.75f;
    bool multiPointerDown = false;
    bool multiPointerMoved = false;
    juce::Point<float> multiPointerStart;
    double multiPointerDownMs = 0.0;
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
    juce::Array<juce::MidiDeviceInfo> midiOutputs;
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