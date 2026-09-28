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

void PerformancePadComponent::nudgeFromPhysicalKey (float deltaX, float deltaY)
{
    xValue = juce::jlimit (0.0f, 1.0f, xValue + deltaX);
    yValue = juce::jlimit (0.0f, 1.0f, yValue + deltaY);

    const float magnitude = std::sqrt (deltaX * deltaX + deltaY * deltaY);
    speedValue = juce::jlimit (0.0f, 1.0f, magnitude * 18.0f);

    if (std::abs (deltaX) > 0.0001f)
        horizontalDirection = deltaX < 0.0f ? -1.0f : 1.0f;
    else
        horizontalDirection = 0.0f;

    active = true;
    physicalPointerVisible = true;
    notify();
    repaint();
}

void PerformancePadComponent::endPhysicalKeyControl()
{
    active = false;
    physicalPointerVisible = false;
    speedValue = 0.0f;
    horizontalDirection = 0.0f;
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

    // Strict 10 x 10 mapping.  No cell is filtered or skipped:
    // visualTileIndex 0..99 maps row-major to every source-sheet cell.
    const int column = juce::jlimit (
        0, tileColumns - 1,
        static_cast<int> (std::floor (
            xValue * static_cast<float> (tileColumns))));
    const int row = juce::jlimit (
        0, tileRows - 1,
        static_cast<int> (std::floor (
            (1.0f - yValue) * static_cast<float> (tileRows))));
    const int visualTileIndex = row * tileColumns + column;

    jassert (visualTileIndex >= 0 && visualTileIndex < tileCount);

    const int visualRow = visualTileIndex / tileColumns;
    const int visualColumn = visualTileIndex % tileColumns;

    const int imageWidth = tileSheetImage.getWidth();
    const int imageHeight = tileSheetImage.getHeight();

    const int tileLeft = (visualColumn * imageWidth) / tileColumns;
    const int tileRight = ((visualColumn + 1) * imageWidth) / tileColumns;
    const int tileTop = (visualRow * imageHeight) / tileRows;
    const int tileBottom = ((visualRow + 1) * imageHeight) / tileRows;

    // Remove only the contact-sheet separator itself.  Do not crop the
    // photograph: the girl's complete face/head must remain visible.
    constexpr int separatorInset = 2;
    const int sourceX = tileLeft + separatorInset;
    const int sourceY = tileTop + separatorInset;
    const int sourceWidth = juce::jmax (
        1, tileRight - tileLeft - separatorInset * 2);
    const int sourceHeight = juce::jmax (
        1, tileBottom - tileTop - separatorInset * 2);

    // Preserve the entire landscape tile and its original aspect ratio.
    // It is centred in the 720 x 720 canvas.  The otherwise-empty top/bottom
    // areas are filled by extending the source edge pixels rather than black
    // letterbox bars, so no part of the face is lost and no black band appears.
    const float scale = juce::jmin (
        static_cast<float> (getWidth()) / static_cast<float> (sourceWidth),
        static_cast<float> (getHeight()) / static_cast<float> (sourceHeight));

    const int destWidth = juce::jmax (
        1, juce::roundToInt (static_cast<float> (sourceWidth) * scale));
    const int destHeight = juce::jmax (
        1, juce::roundToInt (static_cast<float> (sourceHeight) * scale));
    const int destX = (getWidth() - destWidth) / 2;
    const int destY = (getHeight() - destHeight) / 2;

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    if (destY > 0)
    {
        constexpr int edgeSampleHeight = 2;

        // Extend top edge into the upper square margin.
        g.drawImage (tileSheetImage,
                     destX, 0, destWidth, destY,
                     sourceX, sourceY, sourceWidth, edgeSampleHeight,
                     false);

        // Extend bottom edge into the lower square margin.
        g.drawImage (tileSheetImage,
                     destX, destY + destHeight,
                     destWidth, getHeight() - (destY + destHeight),
                     sourceX,
                     sourceY + juce::jmax (0, sourceHeight - edgeSampleHeight),
                     sourceWidth, edgeSampleHeight,
                     false);
    }

    if (destX > 0)
    {
        constexpr int edgeSampleWidth = 2;

        // This is normally unnecessary for the supplied landscape cells, but
        // keeps the same no-black-border rule if the asset aspect changes.
        g.drawImage (tileSheetImage,
                     0, destY, destX, destHeight,
                     sourceX, sourceY, edgeSampleWidth, sourceHeight,
                     false);
        g.drawImage (tileSheetImage,
                     destX + destWidth, destY,
                     getWidth() - (destX + destWidth), destHeight,
                     sourceX + juce::jmax (0, sourceWidth - edgeSampleWidth),
                     sourceY, edgeSampleWidth, sourceHeight,
                     false);
    }

    // Draw the complete source cell last so the central picture is never
    // distorted or cropped.
    g.drawImage (tileSheetImage,
                 destX, destY, destWidth, destHeight,
                 sourceX, sourceY, sourceWidth, sourceHeight,
                 false);

    if (physicalPointerVisible)
    {
        const float px = xValue * static_cast<float> (getWidth());
        const float py = (1.0f - yValue) * static_cast<float> (getHeight());

        constexpr float outerRadius = 13.0f;
        constexpr float innerRadius = 10.0f;

        g.setColour (juce::Colours::black.withAlpha (0.90f));
        g.drawEllipse (px - outerRadius, py - outerRadius,
                       outerRadius * 2.0f, outerRadius * 2.0f, 5.0f);

        g.setColour (juce::Colours::white.withAlpha (0.98f));
        g.drawEllipse (px - innerRadius, py - innerRadius,
                       innerRadius * 2.0f, innerRadius * 2.0f, 2.5f);
        g.drawLine (px - 17.0f, py, px + 17.0f, py, 2.0f);
        g.drawLine (px, py - 17.0f, px, py + 17.0f, 2.0f);
    }
}

void PerformancePadComponent::mouseDown (const juce::MouseEvent& e)
{
    physicalPointerVisible = false;
    touchDownPoint = e.position;
    lastPoint = e.position;
    lastEventMs = juce::Time::getMillisecondCounterHiRes();
    touchDragged = false;

    if (onTouchStarted)
        onTouchStarted();
}

void PerformancePadComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! touchDragged
        && e.position.getDistanceFrom (touchDownPoint) >= 4.0f)
        touchDragged = true;

    if (touchDragged)
        updateFromEvent (e, true);
}

void PerformancePadComponent::mouseUp (const juce::MouseEvent& e)
{
    if (touchDragged)
        updateFromEvent (e, false);
    else if (onTapStopRequested)
        onTapStopRequested();

    touchDragged = false;
}

void PerformancePadComponent::updateFromEvent (const juce::MouseEvent& e, bool isActive)
{
    physicalPointerVisible = false;
    auto grid = getLocalBounds().toFloat();
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

    performancePad.onTouchStarted =
        [this]
        {
            // Touch takes over from a latched D-pad performance.
            hold = false;
            processor.setPerformanceHold (false);
            performancePad.setHeld (false);
        };

    performancePad.onTapStopRequested =
        [this]
        {
            stopAll();
        };

    processor.setPerformanceRoot (rootClass);
    processor.setPerformanceScale (scaleIndex);
    processor.setPerformanceBpm (bpm);
    processor.setPerformanceHold (hold);
    processor.setPerformanceArpEnabled (arpEnabled);
    processor.setPerformanceDelayEnabled (delayEnabled);
    processor.setPerformanceGranularEnabled (granularEnabled);

    grabKeyboardFocus();
}

FlowerStandaloneAudioProcessorEditor::~FlowerStandaloneAudioProcessorEditor()
{
    stopTimer();
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
    bpm = juce::jlimit (50.0f, 200.0f, bpm + delta);
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

void FlowerStandaloneAudioProcessorEditor::toggleArp()
{
    arpEnabled = ! arpEnabled;
    processor.setPerformanceArpEnabled (arpEnabled);
}

void FlowerStandaloneAudioProcessorEditor::toggleDelay()
{
    delayEnabled = ! delayEnabled;
    processor.setPerformanceDelayEnabled (delayEnabled);
}

void FlowerStandaloneAudioProcessorEditor::toggleGranular()
{
    granularEnabled = ! granularEnabled;
    processor.setPerformanceGranularEnabled (granularEnabled);
}

void FlowerStandaloneAudioProcessorEditor::stopAll()
{
    hold = false;
    performancePad.setHeld (false);
    processor.setPerformanceHold (false);
    processor.stopPerformance();
}

void FlowerStandaloneAudioProcessorEditor::beginDpadControl (int keyCode)
{
    float dx = 0.0f;
    float dy = 0.0f;

    if (keyCode == juce::KeyPress::leftKey)        dx = -0.025f;
    else if (keyCode == juce::KeyPress::rightKey) dx =  0.025f;
    else if (keyCode == juce::KeyPress::upKey)    dy =  0.025f;
    else if (keyCode == juce::KeyPress::downKey)  dy = -0.025f;
    else
        return;

    const bool directionChanged = ! dpadActive || dpadKeyCode != keyCode;

    // D-pad performance is latched.  Releasing the D-pad only hides the
    // pointer; a screen tap is the explicit stop gesture.
    hold = true;
    processor.setPerformanceHold (true);
    performancePad.setHeld (true);

    dpadActive = true;
    dpadKeyCode = keyCode;
    dpadDeltaX = dx;
    dpadDeltaY = dy;

    if (directionChanged)
        performancePad.nudgeFromPhysicalKey (dpadDeltaX, dpadDeltaY);

    refreshControlTimer();
}

void FlowerStandaloneAudioProcessorEditor::endDpadControl()
{
    if (! dpadActive)
        return;

    dpadActive = false;
    dpadKeyCode = 0;
    dpadDeltaX = 0.0f;
    dpadDeltaY = 0.0f;

    // active=false is reported to the processor, but performanceHold remains
    // true, so the last D-pad position keeps sounding.
    performancePad.endPhysicalKeyControl();
    refreshControlTimer();
}

void FlowerStandaloneAudioProcessorEditor::beginBpmAdjust (int direction)
{
    direction = direction < 0 ? -1 : 1;

    if (bpmAdjustActive && bpmAdjustDirection == direction)
        return;

    bpmAdjustActive = true;
    bpmAdjustDirection = direction;
    bpmHoldStartMs = juce::Time::getMillisecondCounterHiRes();
    bpmLastRepeatMs = bpmHoldStartMs;

    applyBpmDelta (2.0f * static_cast<float> (direction));
    refreshControlTimer();
}

void FlowerStandaloneAudioProcessorEditor::endBpmAdjust()
{
    bpmAdjustActive = false;
    bpmAdjustDirection = 0;
    refreshControlTimer();
}

void FlowerStandaloneAudioProcessorEditor::beginLooperButton()
{
    if (looperButtonActive)
        return;

    looperButtonActive = true;
    looperLongHandled = false;
    looperHoldStartMs = juce::Time::getMillisecondCounterHiRes();
    refreshControlTimer();
}

void FlowerStandaloneAudioProcessorEditor::endLooperButton()
{
    if (! looperButtonActive)
        return;

    if (! looperLongHandled)
        processor.cycleFlowerTransport();

    looperButtonActive = false;
    looperLongHandled = false;
    refreshControlTimer();
}

void FlowerStandaloneAudioProcessorEditor::refreshControlTimer()
{
    if (dpadActive || bpmAdjustActive || looperButtonActive)
    {
        if (! isTimerRunning())
            startTimer (40);
    }
    else
    {
        stopTimer();
    }
}

void FlowerStandaloneAudioProcessorEditor::timerCallback()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    if (dpadActive)
        performancePad.nudgeFromPhysicalKey (dpadDeltaX, dpadDeltaY);

    if (bpmAdjustActive
        && nowMs - bpmHoldStartMs >= 350.0
        && nowMs - bpmLastRepeatMs >= 80.0)
    {
        applyBpmDelta (
            2.0f * static_cast<float> (bpmAdjustDirection));
        bpmLastRepeatMs = nowMs;
    }

    if (looperButtonActive
        && ! looperLongHandled
        && nowMs - looperHoldStartMs >= 800.0)
    {
        processor.clearFlowerLoop();
        looperLongHandled = true;
    }

    refreshControlTimer();
}

bool FlowerStandaloneAudioProcessorEditor::keyStateChanged (bool isKeyDown)
{
    if (isKeyDown)
        return dpadActive || bpmAdjustActive || looperButtonActive;

    bool used = false;

    if (dpadActive)
    {
        endDpadControl();
        used = true;
    }

    if (bpmAdjustActive)
    {
        endBpmAdjust();
        used = true;
    }

    if (looperButtonActive)
    {
        endLooperButton();
        used = true;
    }

    if (toggleButtonLatchCode != 0)
    {
        toggleButtonLatchCode = 0;
        used = true;
    }

    return used;
}

bool FlowerStandaloneAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const auto ch = key.getTextCharacter();

    if (code == juce::KeyPress::leftKey
        || code == juce::KeyPress::rightKey
        || code == juce::KeyPress::upKey
        || code == juce::KeyPress::downKey)
    {
        beginDpadControl (code);
        return true;
    }

    // Android gamepad bridge:
    // A=F13, B=F14, X=F15, Y=F16, L1=F17, R1=F18.
    if (code == juce::KeyPress::F13Key || ch == 'a' || ch == 'A')
    {
        if (toggleButtonLatchCode != 1)
        {
            toggleDelay();
            toggleButtonLatchCode = 1;
        }
        return true;
    }

    if (code == juce::KeyPress::F14Key || ch == 'b' || ch == 'B')
    {
        if (toggleButtonLatchCode != 2)
        {
            toggleArp();
            toggleButtonLatchCode = 2;
        }
        return true;
    }

    if (code == juce::KeyPress::F15Key || ch == 'x' || ch == 'X')
    {
        beginLooperButton();
        return true;
    }

    if (code == juce::KeyPress::F16Key || ch == 'y' || ch == 'Y')
    {
        if (toggleButtonLatchCode != 4)
        {
            toggleGranular();
            toggleButtonLatchCode = 4;
        }
        return true;
    }

    if (code == juce::KeyPress::F17Key || ch == 'l' || ch == 'L')
    {
        beginBpmAdjust (-1);
        return true;
    }

    if (code == juce::KeyPress::F18Key || ch == 'r' || ch == 'R')
    {
        beginBpmAdjust (1);
        return true;
    }

    // Desktop/debug fallbacks retained outside the six gamepad assignments.
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
