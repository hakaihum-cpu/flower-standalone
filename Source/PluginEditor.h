#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>

#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "FramePack.h"

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
    void timerCallback() override;
    void chooseNextMixedVisual();

    std::array<juce::Image, 100> frameImages;
    FramePack videoFrames;
    juce::Image currentVideoFrame;
    int currentMixedVisual = -1;
    int visualCooldown = 0;
    uint32_t visualRandomState = 0x464C5752u;

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

    void setValues (int rootKey, int scale, bool effectsEnabled,
                    int audioBufferMode, const juce::String& audioStatus);
    void moveSelection (int delta);
    void adjustSelected (int delta);
    void activateSelected();

    std::function<void(int)> onRootChanged;
    std::function<void(int)> onScaleChanged;
    std::function<void(bool)> onEffectsChanged;
    std::function<void(int)> onAudioBufferChanged;
    std::function<void()> onCloseRequested;

private:
    int rootKey = 0;
    int scaleIndex = 0;
    bool effectsEnabled = true;
    int audioBufferMode = 0;
    juce::String audioStatus;
    int selectedRow = 0;

    void notifyCurrentRow();
    juce::Rectangle<int> getRowBounds (int row) const;
    juce::Rectangle<int> getCloseBounds() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigScreenComponent)
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
    bool applyAudioBufferMode (int mode);
    void refreshAudioBufferStatus();
    juce::String makeAudioBufferStatus() const;
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

    int rootClass = 0;
    int scaleIndex = 0;
    float bpm = 112.0f;
    bool hold = false;
    bool arpEnabled = true;
    bool delayEnabled = true;
    bool granularEnabled = true;
    bool configVisible = false;
    int audioBufferMode = 0;
    int audioDefaultBufferFrames = -1;
    int audioFramesPerBurst = 0;
    int audioActualBufferFrames = 0;
    double audioActualSampleRate = 0.0;
    juce::String audioBufferError;

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
