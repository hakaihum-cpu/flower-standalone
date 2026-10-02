#pragma once
#include <JuceHeader.h>
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

private:
    struct AudioInputOption { juce::String type; juce::String name; };
    enum class DragParam { none, complex, bar, width, length };

    void timerCallback() override;
    void paintMain (juce::Graphics&);
    void paintConfig (juce::Graphics&);
    void paintMidiControlConfig (juce::Graphics&);
    void paintBar (juce::Graphics&, juce::Rectangle<float>, const juce::String&, float, const juce::String&);
    void setParameterFromX (DragParam, float designX);
    juce::Point<float> toDesign (juce::Point<float>) const;
    juce::Rectangle<float> parameterBounds (int index) const;
    void refreshAudioInputs();
    void selectAudioInput (int index);
    void refreshMidiOutputs();
    void cycleMidiOutput (int direction);
    void updateMidiControllerFromPoint (juce::Point<float>);
    void setChoiceActual (const char* id, int value);
    juce::String currentInputName() const;
    juce::String noteText (int midi) const;

    RealtimeChordFxAudioProcessor& processor;
    FramePack frames;
    juce::Image currentFrame;
    int loadedFrame = -1;
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
    bool l1Latched = false;
    bool r1Latched = false;
    juce::uint32 lastL1EventMs = 0;
    juce::uint32 lastR1EventMs = 0;

    static constexpr float design = 720.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RealtimeChordFxAudioProcessorEditor)
};