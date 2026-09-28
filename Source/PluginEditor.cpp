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

FlowerStandaloneAudioProcessorEditor::FlowerStandaloneAudioProcessorEditor (
    FlowerStandaloneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&retroLookAndFeel);
    setOpaque (true);
    setWantsKeyboardFocus (true);

    constexpr int canvasSize = 720;
    setSize (canvasSize, canvasSize);
    setResizable (false, false);
    setResizeLimits (canvasSize, canvasSize, canvasSize, canvasSize);

   #if JUCE_ANDROID
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
   #endif

    addAndMakeVisible (performancePad);

    performancePad.onPadChanged =
        [this] (float x, float y, float speed, float horizontalDirection, bool active)
        {
            processor.setPerformancePad (x, y, speed, horizontalDirection, active);
        };

    processor.setPerformanceRoot (rootClass);
    processor.setPerformanceScale (scaleIndex);
    processor.setPerformanceBpm (bpm);
    processor.setPerformanceHold (hold);

    grabKeyboardFocus();
}

FlowerStandaloneAudioProcessorEditor::~FlowerStandaloneAudioProcessorEditor()
{
    processor.stopPerformance();
    setLookAndFeel (nullptr);
}

void FlowerStandaloneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff080807));
}

void FlowerStandaloneAudioProcessorEditor::resized()
{
    performancePad.setBounds (getLocalBounds());
}

void FlowerStandaloneAudioProcessorEditor::applyRootDelta (int delta)
{
    rootClass = (rootClass + delta) % 12;
    if (rootClass < 0)
        rootClass += 12;

    processor.setPerformanceRoot (rootClass);
}

void FlowerStandaloneAudioProcessorEditor::applyBpmDelta (float delta)
{
    bpm = juce::jlimit (50.0f, 190.0f, bpm + delta);
    processor.setPerformanceBpm (bpm);
}

void FlowerStandaloneAudioProcessorEditor::cycleScale (int delta)
{
    scaleIndex = (scaleIndex + delta) % 4;
    if (scaleIndex < 0)
        scaleIndex += 4;

    processor.setPerformanceScale (scaleIndex);
}

void FlowerStandaloneAudioProcessorEditor::toggleHold()
{
    hold = ! hold;
    performancePad.setHeld (hold);
    processor.setPerformanceHold (hold);
}

void FlowerStandaloneAudioProcessorEditor::stopAll()
{
    hold = false;
    performancePad.setHeld (false);
    processor.setPerformanceHold (false);
    processor.stopPerformance();
}

bool FlowerStandaloneAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();

    if (code == juce::KeyPress::leftKey)
    {
        applyRootDelta (-1);
        return true;
    }

    if (code == juce::KeyPress::rightKey)
    {
        applyRootDelta (1);
        return true;
    }

    if (code == juce::KeyPress::upKey)
    {
        applyBpmDelta (2.0f);
        return true;
    }

    if (code == juce::KeyPress::downKey)
    {
        applyBpmDelta (-2.0f);
        return true;
    }

    const auto ch = key.getTextCharacter();

    if (ch == 'h' || ch == 'H')
    {
        toggleHold();
        return true;
    }

    if (ch == 's' || ch == 'S')
    {
        cycleScale (1);
        return true;
    }

    if (ch >= '1' && ch <= '4')
    {
        scaleIndex = static_cast<int> (ch - '1');
        processor.setPerformanceScale (scaleIndex);
        return true;
    }

    if (ch == ' ' || code == juce::KeyPress::escapeKey)
    {
        stopAll();
        return true;
    }

    return false;
}
