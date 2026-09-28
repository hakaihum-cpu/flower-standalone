#include "PluginEditor.h"

#if JUCE_ANDROID
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

#include <cmath>

namespace
{
constexpr const char* patternNames[]
{
    "SINGLE", "UP", "DOWN", "UP/DOWN",
    "SKIP", "OCTAVE", "RANDOM", "CHAOS"
};

juce::Colour panelBackground() { return juce::Colour (0xff16130f); }
juce::Colour panelLine()       { return juce::Colour (0xff75684f); }
juce::Colour textMain()        { return juce::Colour (0xffddd0a5); }
juce::Colour textMuted()       { return juce::Colour (0xff8f846d); }
juce::Colour padBackground()   { return juce::Colour (0xff090908); }
juce::Colour padLine()         { return juce::Colour (0xff39352d); }
juce::Colour padHot()          { return juce::Colour (0xffd9d0b4); }
}

PerformancePadComponent::PerformancePadComponent()
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setWantsKeyboardFocus (false);
}

int PerformancePadComponent::getPatternIndex() const noexcept
{
    return juce::jlimit (0, 7, static_cast<int> (std::floor (xValue * 8.0f)));
}

juce::String PerformancePadComponent::getPatternName() const
{
    return patternNames[getPatternIndex()];
}

void PerformancePadComponent::setHeld (bool shouldHold)
{
    held = shouldHold;
    if (! held && ! active)
        notify();
    repaint();
}

void PerformancePadComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (padBackground());
    g.fillRoundedRectangle (area, 9.0f);

    g.setColour (panelLine());
    g.drawRoundedRectangle (area, 9.0f, 1.3f);

    auto grid = area.reduced (14.0f, 14.0f);

    for (int i = 1; i < 8; ++i)
    {
        const float x = grid.getX() + grid.getWidth() * static_cast<float> (i) / 8.0f;
        g.setColour (padLine().withAlpha (i == 4 ? 0.85f : 0.52f));
        g.drawVerticalLine (juce::roundToInt (x), grid.getY(), grid.getBottom());
    }

    for (int i = 1; i < 5; ++i)
    {
        const float y = grid.getY() + grid.getHeight() * static_cast<float> (i) / 5.0f;
        g.setColour (padLine().withAlpha (0.48f));
        g.drawHorizontalLine (juce::roundToInt (y), grid.getX(), grid.getRight());
    }

    g.setFont (juce::FontOptions (8.5f).withStyle ("Bold"));
    for (int i = 0; i < 8; ++i)
    {
        auto zone = juce::Rectangle<float> (
            grid.getX() + grid.getWidth() * static_cast<float> (i) / 8.0f,
            grid.getY(),
            grid.getWidth() / 8.0f,
            20.0f);

        g.setColour (i == getPatternIndex() ? textMain() : textMuted().withAlpha (0.72f));
        g.drawFittedText (patternNames[i], zone.toNearestInt(),
                          juce::Justification::centred, 1);
    }

    g.setColour (textMuted());
    g.setFont (juce::FontOptions (8.0f));
    g.drawText ("CLEAN", grid.toNearestInt().removeFromBottom (18),
                juce::Justification::bottomLeft);

    auto fxText = grid.toNearestInt();
    fxText.removeFromLeft (6);
    fxText.removeFromBottom (26);
    g.drawText ("GRAIN  •  DELAY  •  FILTER",
                fxText.removeFromTop (18),
                juce::Justification::topRight);

    const float px = grid.getX() + xValue * grid.getWidth();
    const float py = grid.getBottom() - yValue * grid.getHeight();

    g.setColour (padHot().withAlpha (0.25f));
    g.drawVerticalLine (juce::roundToInt (px), grid.getY(), grid.getBottom());
    g.drawHorizontalLine (juce::roundToInt (py), grid.getX(), grid.getRight());

    const float radius = 12.0f + speedValue * 8.0f;
    g.setColour (padHot().withAlpha ((active || held) ? 0.96f : 0.60f));
    g.drawEllipse (px - radius, py - radius, radius * 2.0f, radius * 2.0f, 2.0f);
    g.fillEllipse (px - 3.0f, py - 3.0f, 6.0f, 6.0f);

    if (held && ! active)
    {
        g.setColour (textMain());
        g.setFont (juce::FontOptions (9.0f).withStyle ("Bold"));
        g.drawText ("HOLD", area.toNearestInt().reduced (12).removeFromBottom (20),
                    juce::Justification::bottomRight);
    }
}

void PerformancePadComponent::mouseDown (const juce::MouseEvent& e)
{
    lastPoint = e.position;
    lastEventMs = juce::Time::getMillisecondCounterHiRes();
    updateFromEvent (e, true);
}

void PerformancePadComponent::mouseDrag (const juce::MouseEvent& e)
{
    updateFromEvent (e, true);
}

void PerformancePadComponent::mouseUp (const juce::MouseEvent& e)
{
    updateFromEvent (e, false);
}

void PerformancePadComponent::updateFromEvent (const juce::MouseEvent& e, bool isActive)
{
    auto grid = getLocalBounds().toFloat().reduced (15.0f);
    if (grid.getWidth() <= 1.0f || grid.getHeight() <= 1.0f)
        return;

    const float newX = juce::jlimit (
        0.0f, 1.0f,
        (e.position.x - grid.getX()) / grid.getWidth());
    const float newY = juce::jlimit (
        0.0f, 1.0f,
        (grid.getBottom() - e.position.y) / grid.getHeight());

    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const double elapsedSeconds = juce::jmax (0.001, (nowMs - lastEventMs) / 1000.0);
    const auto delta = e.position - lastPoint;
    const float normDistance = std::sqrt (
        std::pow (delta.x / juce::jmax (1.0f, grid.getWidth()), 2.0f)
      + std::pow (delta.y / juce::jmax (1.0f, grid.getHeight()), 2.0f));

    speedValue = juce::jlimit (
        0.0f, 1.0f,
        static_cast<float> (normDistance / elapsedSeconds) * 0.55f);

    if (std::abs (delta.x) > 0.5f)
        horizontalDirection = juce::jlimit (-1.0f, 1.0f,
            delta.x / juce::jmax (12.0f, grid.getWidth() * 0.16f));

    xValue = newX;
    yValue = newY;
    active = isActive;

    lastPoint = e.position;
    lastEventMs = nowMs;

    notify();
    repaint();
}

void PerformancePadComponent::notify()
{
    if (onPadChanged)
        onPadChanged (xValue, yValue, speedValue, horizontalDirection, active || held);
}

void FlowerStandaloneAudioProcessorEditor::styleLabel (juce::Label& label,
                                                        float size,
                                                        bool bold)
{
    label.setColour (juce::Label::textColourId, textMain());
    label.setFont (bold
        ? juce::FontOptions (size).withStyle ("Bold")
        : juce::FontOptions (size));
}

FlowerStandaloneAudioProcessorEditor::FlowerStandaloneAudioProcessorEditor (
    FlowerStandaloneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&retroLookAndFeel);
    setOpaque (true);

   #if JUCE_ANDROID
    constexpr int androidCanvasSize = 720;
    setSize (androidCanvasSize, androidCanvasSize);
    setResizable (false, false);
    setResizeLimits (androidCanvasSize, androidCanvasSize,
                     androidCanvasSize, androidCanvasSize);

    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        holder->stopPlaying();
        holder->deviceManager.closeAudioDevice();

        const auto audioError =
            holder->deviceManager.initialise (0, 2, nullptr, true);

        if (audioError.isNotEmpty())
            juce::Logger::writeToLog (
                "FLOWER XY Android audio initialise failed: " + audioError);

        holder->startPlaying();
    }
   #else
    constexpr int desktopCanvasSize = 720;
    setSize (desktopCanvasSize, desktopCanvasSize);
    setResizable (false, false);
    setResizeLimits (desktopCanvasSize, desktopCanvasSize,
                     desktopCanvasSize, desktopCanvasSize);
   #endif

    titleLabel.setText ("FLOWER", juce::dontSendNotification);
    styleLabel (titleLabel, 22.0f, true);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("XY PERFORMANCE SYNTH / SEQUENCER / FX",
                           juce::dontSendNotification);
    subtitleLabel.setColour (juce::Label::textColourId, textMuted());
    subtitleLabel.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    padReadout.setJustificationType (juce::Justification::centredRight);
    padReadout.setColour (juce::Label::textColourId, textMain());
    padReadout.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    addAndMakeVisible (padReadout);

    addAndMakeVisible (performancePad);

    rootLabel.setText ("ROOT", juce::dontSendNotification);
    scaleLabel.setText ("SCALE", juce::dontSendNotification);
    tempoLabel.setText ("BPM", juce::dontSendNotification);
    for (auto* label : { &rootLabel, &scaleLabel, &tempoLabel })
    {
        styleLabel (*label, 9.0f, true);
        label->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*label);
    }

    rootBox.addItemList (
        juce::StringArray { "C", "C#", "D", "D#", "E", "F",
                            "F#", "G", "G#", "A", "A#", "B" }, 1);
    rootBox.setSelectedId (1, juce::dontSendNotification);
    rootBox.onChange = [this]
    {
        processor.setPerformanceRoot (juce::jmax (0, rootBox.getSelectedId() - 1));
    };
    addAndMakeVisible (rootBox);

    scaleBox.addItemList (
        juce::StringArray { "MIN PENT", "MINOR", "MAJOR", "DORIAN" }, 1);
    scaleBox.setSelectedId (1, juce::dontSendNotification);
    scaleBox.onChange = [this]
    {
        processor.setPerformanceScale (juce::jmax (0, scaleBox.getSelectedId() - 1));
    };
    addAndMakeVisible (scaleBox);

    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 28);
    tempoSlider.setRange (50.0, 190.0, 1.0);
    tempoSlider.setValue (112.0, juce::dontSendNotification);
    tempoSlider.onValueChange = [this]
    {
        processor.setPerformanceBpm (static_cast<float> (tempoSlider.getValue()));
    };
    addAndMakeVisible (tempoSlider);

    holdButton.setClickingTogglesState (true);
    holdButton.onClick = [this]
    {
        const bool held = holdButton.getToggleState();
        performancePad.setHeld (held);
        processor.setPerformanceHold (held);
        updatePadReadout();
    };
    addAndMakeVisible (holdButton);

    panicButton.onClick = [this]
    {
        holdButton.setToggleState (false, juce::dontSendNotification);
        performancePad.setHeld (false);
        processor.setPerformanceHold (false);
        processor.stopPerformance();
        updatePadReadout();
    };
    addAndMakeVisible (panicButton);

    performancePad.onPadChanged =
        [this] (float x, float y, float speed, float horizontalDirection, bool active)
        {
            processor.setPerformancePad (x, y, speed, horizontalDirection, active);
            updatePadReadout();
        };

    processor.setPerformanceRoot (0);
    processor.setPerformanceScale (0);
    processor.setPerformanceBpm (112.0f);
    processor.setPerformanceHold (false);
    updatePadReadout();
}

FlowerStandaloneAudioProcessorEditor::~FlowerStandaloneAudioProcessorEditor()
{
    processor.stopPerformance();
    setLookAndFeel (nullptr);
}

void FlowerStandaloneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff080807));

    auto panel = getLocalBounds().toFloat().reduced (18.0f);
    g.setColour (panelBackground());
    g.fillRoundedRectangle (panel, 13.0f);
    g.setColour (panelLine());
    g.drawRoundedRectangle (panel, 13.0f, 1.2f);
}

void FlowerStandaloneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (30, 24);

    auto header = area.removeFromTop (54);
    titleLabel.setBounds (header.removeFromLeft (112));
    subtitleLabel.setBounds (header.removeFromLeft (260));
    padReadout.setBounds (header);

    area.removeFromTop (4);

    auto footer = area.removeFromBottom (88);
    area.removeFromBottom (8);
    performancePad.setBounds (area);

    const int buttonWidth = 72;
    panicButton.setBounds (footer.removeFromRight (buttonWidth).reduced (4, 19));
    holdButton.setBounds (footer.removeFromRight (buttonWidth).reduced (4, 19));

    auto bpm = footer.removeFromRight (190);
    tempoLabel.setBounds (bpm.removeFromTop (18));
    tempoSlider.setBounds (bpm.reduced (2, 5));

    auto scale = footer.removeFromRight (130);
    scaleLabel.setBounds (scale.removeFromTop (18));
    scaleBox.setBounds (scale.reduced (4, 9));

    auto root = footer.removeFromRight (92);
    rootLabel.setBounds (root.removeFromTop (18));
    rootBox.setBounds (root.reduced (4, 9));
}

void FlowerStandaloneAudioProcessorEditor::updatePadReadout()
{
    const auto x = performancePad.getXValue();
    const auto y = performancePad.getYValue();

    padReadout.setText (
        performancePad.getPatternName()
        + "   X " + juce::String (x, 2)
        + "   FX " + juce::String (y, 2),
        juce::dontSendNotification);
}
