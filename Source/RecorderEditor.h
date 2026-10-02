#pragma once
#include <JuceHeader.h>
#include "RecorderProcessor.h"

class RecorderAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit RecorderAudioProcessorEditor (RecorderAudioProcessor&);
    ~RecorderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Point<float> toDesign (juce::Point<float>) const;
    juce::Rectangle<float> tileBounds (int slot) const;
    void drawTile (juce::Graphics&, int slot);
    void drawButton (juce::Graphics&, juce::Rectangle<float>, const juce::String&, bool active = false);
    void setBpmFromX (float x);

    RecorderAudioProcessor& processor;
    bool draggingBpm = false;
    static constexpr float design = 720.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecorderAudioProcessorEditor)
};
