#include "RecorderEditor.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

RecorderAudioProcessorEditor::RecorderAudioProcessorEditor (RecorderAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
#if JUCE_ANDROID
    setResizable (true, false);
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        holder->stopPlaying();
        holder->deviceManager.closeAudioDevice();
        const auto error = holder->deviceManager.initialise (1, 2, nullptr, true);
        if (error.isNotEmpty())
            juce::Logger::writeToLog ("RECORDER Android audio initialise failed: " + error);
        holder->getMuteInputValue().setValue (false);
        holder->startPlaying();
    }
#else
    setSize (720, 720);
    setResizable (false, false);
#endif
    startTimerHz (20);
}

RecorderAudioProcessorEditor::~RecorderAudioProcessorEditor()
{
    stopTimer();
}

juce::Point<float> RecorderAudioProcessorEditor::toDesign (juce::Point<float> p) const
{
    return { p.x * design / (float) juce::jmax (1, getWidth()),
             p.y * design / (float) juce::jmax (1, getHeight()) };
}

juce::Rectangle<float> RecorderAudioProcessorEditor::tileBounds (int slot) const
{
    const int row = slot / 3;
    const int col = slot % 3;
    constexpr float gap = 12.0f;
    constexpr float x0 = 66.0f;
    constexpr float y0 = 68.0f;
    constexpr float tile = 188.0f;
    return { x0 + col * (tile + gap), y0 + row * (tile + gap), tile, tile };
}

void RecorderAudioProcessorEditor::timerCallback()
{
    repaint();
}

void RecorderAudioProcessorEditor::drawButton (juce::Graphics& g, juce::Rectangle<float> r,
                                                const juce::String& text, bool active)
{
    g.setColour (active ? juce::Colours::white : juce::Colours::white.withAlpha (0.28f));
    g.drawRect (r, active ? 2.0f : 1.0f);
    g.setColour (juce::Colours::white.withAlpha (active ? 0.98f : 0.78f));
    g.setFont (juce::FontOptions (13.0f).withStyle (active ? "Bold" : "Regular"));
    g.drawText (text, r, juce::Justification::centred);
}

void RecorderAudioProcessorEditor::drawTile (juce::Graphics& g, int slot)
{
    auto r = tileBounds (slot);
    const bool recording = processor.getRecordingSlot() == slot;
    const bool playing = processor.getPlaybackSlot() == slot;
    const int valid = processor.getValidSamples (slot);

    g.setColour (juce::Colours::white.withAlpha (0.055f));
    g.fillRect (r);
    g.setColour (recording ? juce::Colour (0xffe7463d)
                           : juce::Colours::white.withAlpha (playing ? 0.92f : 0.30f));
    g.drawRect (r, recording || playing ? 2.5f : 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String (slot + 1), r.reduced (9.0f).removeFromTop (20.0f), juce::Justification::topLeft);

    if (valid <= 0)
        return;

    auto wave = r.reduced (10.0f, 28.0f);
    const float cy = wave.getCentreY();
    const float half = wave.getHeight() * 0.46f;
    juce::Path path;
    for (int i = 0; i < RecorderAudioProcessor::kPeakBins; ++i)
    {
        const float x = wave.getX() + wave.getWidth() * (float) i / (float) (RecorderAudioProcessor::kPeakBins - 1);
        const float a = juce::jlimit (0.0f, 1.0f, processor.getPeak (slot, i) * 1.7f);
        if (i == 0) path.startNewSubPath (x, cy - a * half);
        else path.lineTo (x, cy - a * half);
    }
    for (int i = RecorderAudioProcessor::kPeakBins - 1; i >= 0; --i)
    {
        const float x = wave.getX() + wave.getWidth() * (float) i / (float) (RecorderAudioProcessor::kPeakBins - 1);
        const float a = juce::jlimit (0.0f, 1.0f, processor.getPeak (slot, i) * 1.7f);
        path.lineTo (x, cy + a * half);
    }
    path.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (0.80f));
    g.fillPath (path);

    if (playing)
    {
        const float px = wave.getX() + wave.getWidth() * juce::jlimit (0.0f, 1.0f, processor.getPlaybackProgress());
        g.setColour (juce::Colours::white);
        g.drawLine (px, wave.getY(), px, wave.getBottom(), 2.0f);
    }
}

void RecorderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / design, (float) getHeight() / design));

    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
    g.drawText ("RECORDER", 30, 20, 230, 34, juce::Justification::centredLeft);

    const float level = juce::jlimit (0.0f, 1.0f, processor.getInputLevel() * 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.fillRect (455.0f, 36.0f, 235.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.82f));
    g.fillRect (455.0f, 35.0f, 235.0f * level, 4.0f);

    for (int i = 0; i < RecorderAudioProcessor::kSlots; ++i)
        drawTile (g, i);

    const auto rec = juce::Rectangle<float> (30, 676, 145, 30);
    const auto rnd = juce::Rectangle<float> (187, 676, 145, 30);
    const auto clk = juce::Rectangle<float> (344, 676, 145, 30);
    const auto bpm = juce::Rectangle<float> (501, 676, 189, 30);

    drawButton (g, rec, processor.isRecording() ? juce::String::fromUTF8 (u8"● REC") : "REC", processor.isRecording());
    drawButton (g, rnd, "RANDOM", processor.isRandomMode());
    drawButton (g, clk, processor.isMidiClockMode() ? "MIDI" : "INTERNAL", processor.isMidiClockMode());
    drawButton (g, bpm, processor.isMidiClockMode() ? "BPM  MIDI" : "BPM  " + juce::String (processor.getInternalBpm()), false);
}

void RecorderAudioProcessorEditor::setBpmFromX (float x)
{
    const auto r = juce::Rectangle<float> (501, 676, 189, 30);
    if (! r.contains (x, r.getCentreY()) || processor.isMidiClockMode()) return;
    const float norm = juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth());
    processor.setInternalBpm (30 + juce::roundToInt (norm * 270.0f));
}

void RecorderAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = toDesign (e.position);
    draggingBpm = false;

    for (int i = 0; i < RecorderAudioProcessor::kSlots; ++i)
        if (tileBounds (i).contains (p))
        {
            processor.requestPlaySlot (i);
            return;
        }

    if (juce::Rectangle<float> (30, 676, 145, 30).contains (p))
        processor.toggleRecording();
    else if (juce::Rectangle<float> (187, 676, 145, 30).contains (p))
        processor.toggleRandomMode();
    else if (juce::Rectangle<float> (344, 676, 145, 30).contains (p))
        processor.toggleClockMode();
    else if (juce::Rectangle<float> (501, 676, 189, 30).contains (p))
    {
        draggingBpm = true;
        setBpmFromX (p.x);
    }
    repaint();
}

void RecorderAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingBpm) return;
    setBpmFromX (toDesign (e.position).x);
    repaint();
}
