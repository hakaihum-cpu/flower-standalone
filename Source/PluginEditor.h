#pragma once

#include <JuceHeader.h>
#include <functional>

#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"

class PerformancePadComponent final : public juce::Component
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
    float getXValue() const noexcept { return xValue; }
    float getYValue() const noexcept { return yValue; }
    int getPatternIndex() const noexcept;
    juce::String getPatternName() const;

    PadCallback onPadChanged;

private:
    void updateFromEvent (const juce::MouseEvent& e, bool isActive);
    void notify();

    float xValue = 0.28f;
    float yValue = 0.28f;
    float speedValue = 0.0f;
    float horizontalDirection = 0.0f;
    bool active = false;
    bool held = false;

    juce::Point<float> lastPoint;
    double lastEventMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformancePadComponent)
};

class FlowerStandaloneAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit FlowerStandaloneAudioProcessorEditor (FlowerStandaloneAudioProcessor&);
    ~FlowerStandaloneAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void updatePadReadout();
    static void styleLabel (juce::Label& label, float size, bool bold = false);

    FlowerStandaloneAudioProcessor& processor;
    RetroLookAndFeel retroLookAndFeel;

    PerformancePadComponent performancePad;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label padReadout;

    juce::Label rootLabel;
    juce::ComboBox rootBox;

    juce::Label scaleLabel;
    juce::ComboBox scaleBox;

    juce::Label tempoLabel;
    juce::Slider tempoSlider;

    juce::TextButton holdButton { "HOLD" };
    juce::TextButton panicButton { "STOP" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowerStandaloneAudioProcessorEditor)
};
