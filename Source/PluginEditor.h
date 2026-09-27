#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "FlowerAnimationComponent.h"

class LabelledKnob final : public juce::Component
{
public:
    explicit LabelledKnob (juce::String name);
    juce::Slider& slider() noexcept { return control; }
    void resized() override;

private:
    juce::Slider control;
    juce::Label label;
};

class RetroToggleSwitch final : public juce::Button
{
public:
    explicit RetroToggleSwitch (const juce::String& name);
    void paintButton (juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
    void setNamedStateStyle (bool shouldShowName)
    {
        namedStateStyle = shouldShowName;
        repaint();
    }

private:
    bool namedStateStyle = false;
};

class FlowerPanel final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;
};

class SynthPanel final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;
};

class FlowerWaveformComponent final : public juce::Component
{
public:
    void setState (const std::array<float, FlowerStandaloneAudioProcessor::flowerWaveformBins>& newWaveform,
                   float validFraction,
                   float recordProgress,
                   bool isRecording,
                   float basePosition,
                   const std::array<float, FlowerStandaloneAudioProcessor::flowerGrainCount>& grainPositions,
                   int activeGrains,
                   float spread,
                   float grainSize);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    std::function<void(float)> onPositionChanged;

private:
    std::array<float, FlowerStandaloneAudioProcessor::flowerWaveformBins> waveform {};
    std::array<float, FlowerStandaloneAudioProcessor::flowerGrainCount> grains {};
    float valid = 0.0f;
    float progress = 0.0f;
    float position = 0.0f;
    float spreadAmount = 0.0f;
    float sizeAmount = 0.08f;
    int grainCount = 0;
    bool recording = false;
};

class FlowerStandaloneAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit FlowerStandaloneAudioProcessorEditor (FlowerStandaloneAudioProcessor&);
    ~FlowerStandaloneAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void showSynth (bool shouldShow);
    static void configureSlider (LabelledKnob& knob, double min, double max, double step = 0.0);
    static void setParameterNormalized (juce::RangedAudioParameter* parameter, float normalized);

    FlowerStandaloneAudioProcessor& processor;
    RetroLookAndFeel retroLookAndFeel;

    FlowerPanel flowerPanel;
    FlowerWaveformComponent flowerWaveform;
    FlowerAnimationComponent flowerAnimation;

    RetroToggleSwitch flowerOn { "FLOWER" };
    RetroToggleSwitch flowerReverse { "REVERSE" };
    juce::TextButton flowerClear { "CLEAR" };
    juce::TextButton synthButton { "SYNTH" };

    LabelledKnob flowerPosition { "POSITION" };
    LabelledKnob flowerSize { "SIZE" };
    LabelledKnob flowerDensity { "DENSITY" };
    LabelledKnob flowerSpread { "SPREAD" };
    LabelledKnob flowerHold { "HOLD" };
    LabelledKnob flowerPitch { "PITCH" };
    LabelledKnob flowerMix { "MIX" };
    LabelledKnob flowerFeedback { "FEEDBACK" };

    SynthPanel synthPanel;
    juce::TextButton closeSynthButton { "CLOSE" };
    LabelledKnob synthLevel { "LEVEL" };
    LabelledKnob synthAttack { "ATTACK" };
    LabelledKnob synthDecay { "DECAY" };
    LabelledKnob synthSustain { "SUSTAIN" };
    LabelledKnob synthRelease { "RELEASE" };
    LabelledKnob synthCutoff { "CUTOFF" };
    LabelledKnob synthResonance { "RESONANCE" };
    LabelledKnob synthLfoRate { "LFO RATE" };
    LabelledKnob synthLfoDepth { "LFO DEPTH" };
    juce::ComboBox synthLfoTarget;
    juce::Label synthLfoTargetLabel;
    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;

   #if JUCE_ANDROID
    int androidStartupTicks = 0;
    bool androidVisualLoadAttempted = false;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowerStandaloneAudioProcessorEditor)
};
