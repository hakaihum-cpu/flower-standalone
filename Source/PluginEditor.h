#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>

#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"

class PerformancePadComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    using PadCallback = std::function<void(float x,
                                           float y,
                                           float speed,
                                           float horizontalDirection,
                                           bool active)>;

    PerformancePadComponent();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void setHeld (bool shouldHold);
    void nudgeFromPhysicalKey (float deltaX, float deltaY);
    void endPhysicalKeyControl();
    void setExternalPosition (float x, float y);
    void setEffectState (bool arpOn, bool delayOn, bool yEffectOn, bool dreamyMode);
    void setBpmDisplay (float bpm, bool visible);
    float getXValue() const noexcept { return xValue; }
    float getYValue() const noexcept { return yValue; }
    int getPatternIndex() const noexcept;
    juce::String getPatternName() const;

    PadCallback onPadChanged;
    std::function<void()> onTouchStarted;
    std::function<void()> onTapStopRequested;

private:
    static constexpr int tileColumns = 10;
    static constexpr int tileRows = 10;
    static constexpr int tileCount = tileColumns * tileRows;
    static_assert (tileCount == 100, "FLOWER visual bank must contain all 100 cells");

    void updateFromEvent (const juce::MouseEvent& e, bool isActive);
    void notify();
    void updateVisualTimer();
    void timerCallback() override;

    std::array<juce::Image, 100> frameImages;
    bool arpIndicatorOn = true;
    bool delayIndicatorOn = true;
    bool yEffectIndicatorOn = true;
    bool dreamyIndicatorMode = false;
    float visualPhase = 0.0f;
    float bpmDisplayValue = 112.0f;
    bool bpmDisplayVisible = false;

    float xValue = 0.28f;
    float yValue = 0.28f;
    float speedValue = 0.0f;
    float horizontalDirection = 0.0f;
    bool active = false;
    bool held = false;
    bool physicalPointerVisible = false;

    juce::Point<float> lastPoint;
    juce::Point<float> touchDownPoint;
    bool touchDragged = false;
    double lastEventMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformancePadComponent)
};

class ConfigScreenComponent final : public juce::Component
{
public:
    ConfigScreenComponent() = default;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

    void setValues (int rootKey,
                    int scale,
                    bool effectsEnabled,
                    bool yEffectDreamy,
                    int midiChannel);
    void moveSelection (int delta);
    void adjustSelected (int delta);
    void activateSelected();

    std::function<void(int)> onRootChanged;
    std::function<void(int)> onScaleChanged;
    std::function<void(bool)> onEffectsChanged;
    std::function<void(bool)> onYEffectModeChanged;
    std::function<void(int)> onMidiChannelChanged;
    std::function<void()> onCarnivalRequested;
    std::function<void()> onCloseRequested;

private:
    int rootKey = 0;
    int scaleIndex = 0;
    bool effectsEnabled = true;
    bool yEffectDreamy = false;
    int midiChannel = 1;
    int selectedRow = 0;

    void notifyCurrentRow();
    juce::Rectangle<int> getRowBounds (int row) const;
    juce::Rectangle<int> getCloseBounds() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigScreenComponent)
};


class CarnivalScreenComponent final : public juce::Component,
                                     private juce::Timer
{
public:
    explicit CarnivalScreenComponent (FlowerStandaloneAudioProcessor&);
    ~CarnivalScreenComponent() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    bool handleKeyPress (const juce::KeyPress& key);
    void showSequencePage();

    std::function<void()> onExitRequested;

private:
    enum class Page
    {
        Sequence = 0,
        Parameter,
        Config
    };

    static constexpr float designSize = 720.0f;
    static constexpr int gridColumns = 10;
    static constexpr int gridRows = 10;
    static constexpr float cellSize = 72.0f;

    void timerCallback() override;
    juce::Point<float> toDesignPoint (juce::Point<float> point) const;
    void setPage (Page newPage);
    void selectPageFromHeader (float x);
    void paintSequence (juce::Graphics& g);
    void paintParameter (juce::Graphics& g);
    void paintConfig (juce::Graphics& g);
    void paintHeader (juce::Graphics& g, const juce::String& title);
    void adjustCurrentParameter (float normalised);
    void nudgeCurrentParameter (float delta);
    void activateConfigRow (int direction);
    juce::String getMachineName (int track) const;
    juce::String getParameterName (int param) const;
    juce::String getParameterValueText (int param, float value) const;
    void openParameterForStep (int track, int step);
    void openParameterForTrack (int track);

    FlowerStandaloneAudioProcessor& processor;
    juce::Image sequenceBackground;
    juce::Image activeStepImage;

    Page page = Page::Sequence;
    int cursorRow = 0;
    int cursorColumn = 0;
    int selectedTrack = 0;
    int selectedStep = 0;
    int selectedParam = 0;
    int configRow = 0;
    bool parameterLockMode = false;

    juce::Point<float> pointerDownDesign;
    double pointerDownMs = 0.0;
    bool pointerDragged = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CarnivalScreenComponent)
};

class FlowerStandaloneAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit FlowerStandaloneAudioProcessorEditor (FlowerStandaloneAudioProcessor&);
    ~FlowerStandaloneAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    void applyRootDelta (int delta);
    void applyBpmDelta (float delta);
    void cycleScale (int delta);
    void toggleHold();
    void toggleArp();
    void toggleDelay();
    void toggleGranular();
    void toggleConfig();
    void stopAll();
    void beginDpadControl (int keyCode);
    void endDpadControl();
    void beginBpmAdjust (int direction);
    void endBpmAdjust();
    void beginLooperButton();
    void endLooperButton();
    void refreshControlTimer();
    void timerCallback() override;

    FlowerStandaloneAudioProcessor& processor;
    RetroLookAndFeel retroLookAndFeel;
    PerformancePadComponent performancePad;
    ConfigScreenComponent configScreen;
    CarnivalScreenComponent carnivalScreen;

    int rootClass = 0;
    int scaleIndex = 0;
    float bpm = 112.0f;
    bool hold = false;
    bool arpEnabled = true;
    bool delayEnabled = true;
    bool granularEnabled = true;
    bool yEffectDreamy = false;
    int midiChannel = 1;
    bool configVisible = false;
    bool carnivalVisible = false;

    bool dpadActive = false;
    int dpadKeyCode = 0;
    float dpadDeltaX = 0.0f;
    float dpadDeltaY = 0.0f;

    bool bpmAdjustActive = false;
    int bpmAdjustDirection = 0;
    double bpmHoldStartMs = 0.0;
    double bpmLastRepeatMs = 0.0;

    bool looperButtonActive = false;
    bool looperLongHandled = false;
    double looperHoldStartMs = 0.0;

    int toggleButtonLatchCode = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowerStandaloneAudioProcessorEditor)
};
