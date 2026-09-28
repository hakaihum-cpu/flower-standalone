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

}

PerformancePadComponent::PerformancePadComponent()
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setWantsKeyboardFocus (false);

    tileSheetImage = juce::ImageFileFormat::loadFrom (
        BinaryData::flower_xy_sheet_01_jpg,
        static_cast<size_t> (BinaryData::flower_xy_sheet_01_jpgSize));
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
    g.fillAll (juce::Colours::black);

    if (! tileSheetImage.isValid())
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
        g.drawFittedText ("VISUAL ASSET ERROR",
                          getLocalBounds().reduced (24),
                          juce::Justification::centred, 1);
        return;
    }

    // One contact-sheet tile is one visual state.  X selects the column and
    // musical Y (bottom=0, top=1) selects the row in the same direction as the
    // visible image: top of the pad -> top sheet row.
    const int column = juce::jlimit (
        0, tileColumns - 1,
        static_cast<int> (std::floor (xValue * static_cast<float> (tileColumns))));
    const int row = juce::jlimit (
        0, tileRows - 1,
        static_cast<int> (std::floor (
            (1.0f - yValue) * static_cast<float> (tileRows))));

    const int imageWidth = tileSheetImage.getWidth();
    const int imageHeight = tileSheetImage.getHeight();

    const int tileLeft = (column * imageWidth) / tileColumns;
    const int tileRight = ((column + 1) * imageWidth) / tileColumns;
    const int tileTop = (row * imageHeight) / tileRows;
    const int tileBottom = ((row + 1) * imageHeight) / tileRows;

    // The supplied sheet has dark separator lines.  Inset each cell before
    // cropping so those separators do not appear in the fullscreen image.
    constexpr int separatorInset = 2;
    const int innerLeft = tileLeft + separatorInset;
    const int innerTop = tileTop + separatorInset;
    const int innerWidth = juce::jmax (
        1, tileRight - tileLeft - separatorInset * 2);
    const int innerHeight = juce::jmax (
        1, tileBottom - tileTop - separatorInset * 2);

    // The app canvas is square.  Centre-crop each source tile to square rather
    // than stretching the face image.
    const int sourceSide = juce::jmax (1, juce::jmin (innerWidth, innerHeight));
    const int sourceX = innerLeft + (innerWidth - sourceSide) / 2;
    const int sourceY = innerTop + (innerHeight - sourceSide) / 2;

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (tileSheetImage,
                 0, 0, getWidth(), getHeight(),
                 sourceX, sourceY, sourceSide, sourceSide,
                 false);
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
    // HOLD is managed separately by the processor.  The pad callback reports
    // actual touch state only; otherwise releasing a held touch would leave
    // performanceActive latched even after HOLD was later turned off.
    if (onPadChanged)
        onPadChanged (xValue, yValue, speedValue, horizontalDirection, active);
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
