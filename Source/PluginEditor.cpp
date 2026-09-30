#include "PluginEditor.h"
#include "FlowerFrameData.h"
#include "CarnivalBg0.h"
#include "CarnivalBg1.h"
#include "CarnivalActiveData.h"

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

constexpr const char* rootNames[]
{
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

constexpr bool twilightRealtime3DPrototypeMode = true;

constexpr const char* scaleNames[]
{
    "MINOR PENT", "NATURAL MINOR", "MAJOR", "DORIAN", "RANDOM"
};

}

PerformancePadComponent::PerformancePadComponent()
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setWantsKeyboardFocus (false);

    if (twilightRealtime3DPrototypeMode)
        return;

    juce::MemoryOutputStream decodedFrames;
    if (! juce::Base64::convertFromBase64 (
            decodedFrames, FlowerFrameData::encodedPayload))
        return;

    const auto& frameBytes = decodedFrames.getMemoryBlock();
    if (frameBytes.getSize() != FlowerFrameData::decodedPayloadSize)
        return;

    const auto* bytes = static_cast<const unsigned char*> (frameBytes.getData());

    for (int i = 0; i < FlowerFrameData::frameCount; ++i)
    {
        const auto index = static_cast<size_t> (i);
        frameImages[index] = juce::ImageFileFormat::loadFrom (
            bytes + FlowerFrameData::offsets[index],
            FlowerFrameData::sizes[index]);
    }
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

void PerformancePadComponent::setExternalPosition (float x, float y)
{
    xValue = juce::jlimit (0.0f, 1.0f, x);
    yValue = juce::jlimit (0.0f, 1.0f, y);
    physicalPointerVisible = false;
    repaint();
}

void PerformancePadComponent::setEffectState (
    bool arpOn, bool delayOn, bool yEffectOn, bool dreamyMode)
{
    arpIndicatorOn = arpOn;
    delayIndicatorOn = delayOn;
    yEffectIndicatorOn = yEffectOn;
    dreamyIndicatorMode = dreamyMode;
    updateVisualTimer();
    repaint();
}

void PerformancePadComponent::setBpmDisplay (float bpm, bool visible)
{
    bpmDisplayValue = bpm;
    bpmDisplayVisible = visible;
    repaint();
}

void PerformancePadComponent::updateVisualTimer()
{
    const bool animated =
        arpIndicatorOn || delayIndicatorOn || yEffectIndicatorOn;

    if (animated)
    {
        if (! isTimerRunning())
            startTimerHz (30);
    }
    else
    {
        stopTimer();
    }
}

void PerformancePadComponent::timerCallback()
{
    visualPhase += 1.0f / 30.0f;
    if (visualPhase > 1024.0f)
        visualPhase = std::fmod (visualPhase, 1024.0f);

    repaint();
}

void PerformancePadComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

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

    const auto& frame = frameImages[static_cast<size_t> (visualTileIndex)];
    if (! frame.isValid())
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
        g.drawFittedText ("VISUAL FRAME ERROR",
                          getLocalBounds().reduced (24),
                          juce::Justification::centred, 1);
        return;
    }

    // 01.jpg..100.jpg are already the final user-cut frames.
    // Use every pixel of the selected frame and stretch the whole image
    // directly to the full 720 x 720 canvas. No crop, inset, cover or atlas.
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (frame,
                 0, 0, getWidth(), getHeight(),
                 0, 0, frame.getWidth(), frame.getHeight(),
                 false);

    // Effect state is expressed by the whole image. The original frame
    // remains the base layer; overlays stay restrained but clearly readable.

    if (delayIndicatorOn)
    {
        const float drift =
            std::sin (visualPhase * 2.1f) * 2.0f;
        const float drift2 =
            std::sin (visualPhase * 1.37f + 1.4f) * 3.0f;

        juce::Graphics::ScopedSaveState delayState (g);
        g.setOpacity (0.105f);
        g.drawImage (
            frame,
            juce::roundToInt (3.0f + drift), 0,
            getWidth(), getHeight(),
            0, 0, frame.getWidth(), frame.getHeight(),
            false);

        g.setOpacity (0.065f);
        g.drawImage (
            frame,
            juce::roundToInt (-5.0f + drift2), 1,
            getWidth(), getHeight(),
            0, 0, frame.getWidth(), frame.getHeight(),
            false);
    }

    if (yEffectIndicatorOn && ! dreamyIndicatorMode)
    {
        juce::Graphics::ScopedSaveState granularState (g);
        constexpr int stripCount = 9;
        const int sourceHeight = frame.getHeight();

        for (int strip = 0; strip < stripCount; ++strip)
        {
            const int y0 = getHeight() * strip / stripCount;
            const int y1 = getHeight() * (strip + 1) / stripCount;
            const int h = juce::jmax (1, y1 - y0);

            const int sourceY0 = sourceHeight * strip / stripCount;
            const int sourceY1 = sourceHeight * (strip + 1) / stripCount;
            const int sourceH = juce::jmax (1, sourceY1 - sourceY0);

            const float wave =
                std::sin (
                    visualPhase * (2.5f + 0.12f * strip)
                    + static_cast<float> (strip) * 1.71f);

            const int offset =
                juce::roundToInt (wave * (3.0f + (strip % 3)));

            if (std::abs (offset) < 2)
                continue;

            g.setOpacity (0.18f + 0.020f * static_cast<float> (strip % 2));
            g.drawImage (
                frame,
                offset, y0,
                getWidth(), h,
                0, sourceY0,
                frame.getWidth(), sourceH,
                false);
        }
    }

    if (yEffectIndicatorOn && dreamyIndicatorMode)
    {
        // The two visual ghost rates mirror DREAMY's +5 and +12 audio voices.
        const float phase5 = visualPhase * 1.3348398f;
        const float phase12 = visualPhase * 2.0f;

        const int dx5 = juce::roundToInt (std::sin (phase5 * 2.2f) * 4.0f);
        const int dy5 = juce::roundToInt (std::cos (phase5 * 1.7f) * 2.0f);
        const int dx12 = juce::roundToInt (std::cos (phase12 * 1.9f) * 6.0f);
        const int dy12 = juce::roundToInt (std::sin (phase12 * 1.3f) * 3.0f);

        juce::Graphics::ScopedSaveState dreamyState (g);

        g.setOpacity (0.120f);
        g.drawImage (
            frame,
            dx5, dy5,
            getWidth(), getHeight(),
            0, 0, frame.getWidth(), frame.getHeight(),
            false);

        g.setOpacity (0.080f);
        g.drawImage (
            frame,
            dx12, dy12,
            getWidth(), getHeight(),
            0, 0, frame.getWidth(), frame.getHeight(),
            false);

        const float wash =
            0.028f + 0.014f * (0.5f + 0.5f * std::sin (phase5));
        g.setOpacity (1.0f);
        g.setColour (juce::Colour (0xffeee5cf).withAlpha (wash));
        g.fillRect (getLocalBounds());
    }

    if (arpIndicatorOn)
    {
        // Keep ARP readable without adding an icon: a subtle global light pulse.
        const float pulse =
            0.012f
            + 0.020f
                * (0.5f + 0.5f * std::sin (visualPhase * 6.0f));

        juce::Graphics::ScopedSaveState arpState (g);
        g.setColour (juce::Colours::white.withAlpha (pulse));
        g.fillRect (getLocalBounds());
    }

    // Compact effect-state legend. Only active functions are shown.
    {
        juce::StringArray activeEffects;

        if (delayIndicatorOn)
            activeEffects.add ("DELAY");

        if (yEffectIndicatorOn)
            activeEffects.add (dreamyIndicatorMode ? "DRM" : "GRN");

        if (arpIndicatorOn)
            activeEffects.add ("ARP");

        if (! activeEffects.isEmpty())
        {
            const float unit =
                juce::jmax (0.60f,
                    juce::jmin (getWidth(), getHeight()) / 720.0f);
            const auto text = activeEffects.joinIntoString ("  ");

            juce::Font effectFont (
                juce::FontOptions (13.0f * unit).withStyle ("Bold"));

            g.setFont (effectFont);
            g.setColour (juce::Colours::black.withAlpha (0.72f));
            g.drawText (
                text,
                juce::roundToInt (13.0f * unit),
                juce::roundToInt (12.0f * unit),
                juce::roundToInt (280.0f * unit),
                juce::roundToInt (22.0f * unit),
                juce::Justification::centredLeft);

            g.setColour (juce::Colours::white.withAlpha (0.96f));
            g.drawText (
                text,
                juce::roundToInt (12.0f * unit),
                juce::roundToInt (11.0f * unit),
                juce::roundToInt (280.0f * unit),
                juce::roundToInt (22.0f * unit),
                juce::Justification::centredLeft);
        }
    }

    // BPM is intentionally transient: it is visible only while L/R is held.
    if (bpmDisplayVisible)
    {
        const float unit =
            juce::jmax (0.60f,
                juce::jmin (getWidth(), getHeight()) / 720.0f);

        const juce::String bpmText =
            juce::String (juce::roundToInt (bpmDisplayValue));

        juce::Font bpmFont (
            juce::FontOptions (72.0f * unit).withStyle ("Bold"));

        const auto area = juce::Rectangle<int> (
            0,
            juce::roundToInt (68.0f * unit),
            getWidth(),
            juce::roundToInt (92.0f * unit));

        g.setFont (bpmFont);
        g.setColour (juce::Colours::black.withAlpha (0.70f));
        g.drawText (
            bpmText,
            area.translated (
                juce::roundToInt (2.0f * unit),
                juce::roundToInt (3.0f * unit)),
            juce::Justification::centred);

        g.setColour (juce::Colours::white);
        g.drawText (
            bpmText,
            area,
            juce::Justification::centred);

        g.setFont (
            juce::FontOptions (16.0f * unit).withStyle ("Bold"));
        g.setColour (juce::Colours::white.withAlpha (0.90f));
        g.drawText (
            "BPM",
            0,
            juce::roundToInt (145.0f * unit),
            getWidth(),
            juce::roundToInt (26.0f * unit),
            juce::Justification::centred);
    }

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


juce::Rectangle<int> ConfigScreenComponent::getRowBounds (int row) const
{
    constexpr int rowStep = 65;
    constexpr int firstY = 155;
    return { 72, firstY + row * rowStep, 576, 54 };
}

juce::Rectangle<int> ConfigScreenComponent::getCloseBounds() const
{
    return { 516, 636, 132, 52 };
}

void ConfigScreenComponent::setValues (int newRootKey,
                                       int newScale,
                                       bool newEffectsEnabled,
                                       bool newYEffectDreamy,
                                       int newMidiChannel)
{
    rootKey = juce::jlimit (0, 11, newRootKey);
    scaleIndex = juce::jlimit (0, 4, newScale);
    effectsEnabled = newEffectsEnabled;
    yEffectDreamy = newYEffectDreamy;
    midiChannel = juce::jlimit (1, 16, newMidiChannel);
    repaint();
}

void ConfigScreenComponent::moveSelection (int delta)
{
    selectedRow = (selectedRow + delta) % 6;
    if (selectedRow < 0)
        selectedRow += 6;
    repaint();
}

void ConfigScreenComponent::adjustSelected (int delta)
{
    if (selectedRow == 0)
    {
        rootKey = (rootKey + delta) % 12;
        if (rootKey < 0)
            rootKey += 12;

        if (onRootChanged)
            onRootChanged (rootKey);
    }
    else if (selectedRow == 1)
    {
        scaleIndex = (scaleIndex + delta) % 5;
        if (scaleIndex < 0)
            scaleIndex += 5;

        if (onScaleChanged)
            onScaleChanged (scaleIndex);
    }
    else if (selectedRow == 2)
    {
        effectsEnabled = ! effectsEnabled;
        if (onEffectsChanged)
            onEffectsChanged (effectsEnabled);
    }
    else if (selectedRow == 3)
    {
        yEffectDreamy = ! yEffectDreamy;
        if (onYEffectModeChanged)
            onYEffectModeChanged (yEffectDreamy);
    }
    else if (selectedRow == 4)
    {
        midiChannel += delta;
        while (midiChannel < 1)
            midiChannel += 16;
        while (midiChannel > 16)
            midiChannel -= 16;

        if (onMidiChannelChanged)
            onMidiChannelChanged (midiChannel);
    }
    else if (onCarnivalRequested)
    {
        onCarnivalRequested();
    }

    repaint();
}

void ConfigScreenComponent::activateSelected()
{
    if (selectedRow == 5)
    {
        if (onCarnivalRequested)
            onCarnivalRequested();
        return;
    }

    adjustSelected (1);
}

void ConfigScreenComponent::notifyCurrentRow()
{
    if (selectedRow == 0)
    {
        if (onRootChanged)
            onRootChanged (rootKey);
    }
    else if (selectedRow == 1)
    {
        if (onScaleChanged)
            onScaleChanged (scaleIndex);
    }
    else if (selectedRow == 2)
    {
        if (onEffectsChanged)
            onEffectsChanged (effectsEnabled);
    }
    else if (selectedRow == 3)
    {
        if (onYEffectModeChanged)
            onYEffectModeChanged (yEffectDreamy);
    }
    else if (selectedRow == 4)
    {
        if (onMidiChannelChanged)
            onMidiChannelChanged (midiChannel);
    }
}

void ConfigScreenComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0a));

    constexpr float designSize = 720.0f;
    const float scaleX = static_cast<float> (getWidth()) / designSize;
    const float scaleY = static_cast<float> (getHeight()) / designSize;

    juce::Graphics::ScopedSaveState savedState (g);
    g.addTransform (juce::AffineTransform::scale (scaleX, scaleY));

    g.setColour (juce::Colour (0xffe1d7ba));
    g.setFont (juce::FontOptions (34.0f).withStyle ("Bold"));
    g.drawText ("CONFIG", 72, 64, 576, 54,
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff8f8776));
    g.setFont (juce::FontOptions (16.0f));
    g.drawText ("SELECT: CONFIG    UP/DOWN: ITEM    LEFT/RIGHT: CHANGE    B: OK",
                72, 124, 576, 28,
                juce::Justification::centredLeft);

    for (int row = 0; row < 6; ++row)
    {
        const auto bounds = getRowBounds (row);
        const bool selected = row == selectedRow;

        g.setColour (selected
            ? juce::Colour (0xffd8ccb0)
            : juce::Colour (0xff292722));
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);

        g.setColour (selected
            ? juce::Colour (0xff11110f)
            : juce::Colour (0xffd7ceb8));
        g.setFont (juce::FontOptions (19.0f).withStyle ("Bold"));

        juce::String label;
        juce::String value;

        if (row == 0)
        {
            label = "ROOT KEY";
            value = rootNames[rootKey];
        }
        else if (row == 1)
        {
            label = "SCALE";
            value = scaleNames[scaleIndex];
        }
        else if (row == 2)
        {
            label = "DEFAULT EFFECT";
            value = effectsEnabled ? "ON" : "OFF";
        }
        else if (row == 3)
        {
            label = "Y EFFECT";
            value = yEffectDreamy ? "DREAMY" : "GRANULAR";
        }
        else if (row == 4)
        {
            label = "MIDI CHANNEL";
            value = juce::String (midiChannel);
        }
        else
        {
            label = "CARNIVAL";
            value = "ENTER";
        }

        g.drawText (label,
                    bounds.withTrimmedRight (260),
                    juce::Justification::centredLeft);
        g.drawText (value,
                    bounds.withTrimmedLeft (260),
                    juce::Justification::centredRight);
    }

    const auto closeBounds = getCloseBounds();
    g.setColour (juce::Colour (0xffd8ccb0));
    g.fillRoundedRectangle (closeBounds.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff11110f));
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    g.drawText ("CLOSE", closeBounds, juce::Justification::centred);

    g.setColour (juce::Colour (0xff77705f));
    g.setFont (juce::FontOptions (14.0f));
    g.drawFittedText (
        "Y EFFECT: GRANULAR / DREAMY.  CARNIVAL: drum machine mode.",
        72, 552, 420, 52,
        juce::Justification::topLeft, 2);
}

void ConfigScreenComponent::mouseDown (const juce::MouseEvent& e)
{
    constexpr float designSize = 720.0f;
    const float scaleX = static_cast<float> (getWidth()) / designSize;
    const float scaleY = static_cast<float> (getHeight()) / designSize;

    if (scaleX <= 0.0f || scaleY <= 0.0f)
        return;

    const juce::Point<int> designPoint {
        juce::roundToInt (e.position.x / scaleX),
        juce::roundToInt (e.position.y / scaleY)
    };

    if (getCloseBounds().contains (designPoint))
    {
        if (onCloseRequested)
            onCloseRequested();
        return;
    }

    for (int row = 0; row < 6; ++row)
    {
        const auto bounds = getRowBounds (row);
        if (! bounds.contains (designPoint))
            continue;

        selectedRow = row;

        if (row == 2 || row == 3 || row == 5)
        {
            activateSelected();
        }
        else
        {
            const int delta = designPoint.x < bounds.getCentreX() ? -1 : 1;
            adjustSelected (delta);
        }

        return;
    }
}

CarnivalScreenComponent::CarnivalScreenComponent (
    FlowerStandaloneAudioProcessor& p)
    : processor (p)
{
    setOpaque (true);
    setWantsKeyboardFocus (false);

    const auto loadImageFromBase64 = [] (const juce::String& encoded)
    {
        juce::MemoryOutputStream decoded;
        if (! juce::Base64::convertFromBase64 (decoded, encoded))
            return juce::Image {};

        const auto& bytes = decoded.getMemoryBlock();
        return juce::ImageFileFormat::loadFrom (
            bytes.getData(), bytes.getSize());
    };

    sequenceBackground = loadImageFromBase64 (
        juce::String (CarnivalBg0::data)
        + juce::String (CarnivalBg1::data));

    activeStepImage = loadImageFromBase64 (
        juce::String (CarnivalActiveData::data));

    startTimerHz (30);
}

CarnivalScreenComponent::~CarnivalScreenComponent()
{
    stopTimer();
}

juce::Point<float> CarnivalScreenComponent::toDesignPoint (
    juce::Point<float> point) const
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return {};

    return {
        point.x * designSize / static_cast<float> (getWidth()),
        point.y * designSize / static_cast<float> (getHeight())
    };
}

void CarnivalScreenComponent::setPage (Page newPage)
{
    page = newPage;

    if (page == Page::Parameter && ! parameterLockMode)
        selectedTrack = cursorRow;

    repaint();
}

void CarnivalScreenComponent::showSequencePage()
{
    parameterLockMode = false;
    setPage (Page::Sequence);
}

void CarnivalScreenComponent::selectPageFromHeader (float x)
{
    if (x < 240.0f)
        return;

    if (x < 400.0f)
    {
        parameterLockMode = false;
        setPage (Page::Sequence);
    }
    else if (x < 560.0f)
    {
        parameterLockMode = false;
        selectedTrack = cursorRow;
        setPage (Page::Parameter);
    }
    else
    {
        setPage (Page::Config);
    }
}

juce::String CarnivalScreenComponent::getMachineName (int track) const
{
    switch (processor.getCarnivalInstrument (track))
    {
        case 0: return "KICK";
        case 1: return "SNARE";
        case 2: return "HIHAT";
        case 3: return "CHORD";
        case 4: return "TONE";
        case 5: return "TOM";
        case 6: return "BASS";
        default: return "KICK";
    }
}

juce::String CarnivalScreenComponent::getParameterName (int param) const
{
    static constexpr const char* names[]
    {
        "VOLUME", "PAN", "FILTER", "PITCH",
        "DECAY", "LFO RATE", "LFO DEPTH"
    };

    return names[juce::jlimit (
        0, FlowerStandaloneAudioProcessor::carnivalParamCount - 1, param)];
}

juce::String CarnivalScreenComponent::getParameterValueText (
    int param, float value) const
{
    value = juce::jlimit (0.0f, 1.0f, value);

    switch (param)
    {
        case 0:
            return juce::String (juce::roundToInt (value * 100.0f)) + "%";

        case 1:
        {
            const int pan = juce::roundToInt ((value - 0.5f) * 200.0f);
            if (std::abs (pan) <= 1)
                return "C";
            return pan < 0
                ? "L" + juce::String (-pan)
                : "R" + juce::String (pan);
        }

        case 2:
        {
            const float hz = 180.0f + value * value * 15500.0f;
            return hz >= 1000.0f
                ? juce::String (hz / 1000.0f, 1) + "k"
                : juce::String (juce::roundToInt (hz)) + "Hz";
        }

        case 3:
            return juce::String (
                juce::roundToInt ((value - 0.5f) * 48.0f)) + "st";

        case 4:
            return juce::String (
                juce::roundToInt ((0.04f + value * 1.25f) * 1000.0f)) + "ms";

        case 5:
            return juce::String (0.10f + value * 15.9f, 1) + "Hz";

        default:
            return juce::String (juce::roundToInt (value * 100.0f)) + "%";
    }
}

void CarnivalScreenComponent::paintHeader (
    juce::Graphics& g, const juce::String& title)
{
    g.setColour (juce::Colours::black.withAlpha (0.78f));
    g.fillRect (0.0f, 0.0f, 720.0f, 40.0f);

    g.setColour (juce::Colour (0xffe7dcc0));
    g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
    g.drawText ("CARNIVAL  " + title, 12, 0, 225, 40,
                juce::Justification::centredLeft);

    const char* labels[] { "SEQ", "PARAM", "CONFIG" };
    const float x[] { 240.0f, 400.0f, 560.0f };

    for (int i = 0; i < 3; ++i)
    {
        const bool selected =
            static_cast<int> (page) == i;

        g.setColour (selected
            ? juce::Colour (0xffe0d5b9)
            : juce::Colour (0xff292722));
        g.fillRect (x[i], 4.0f, 154.0f, 32.0f);

        g.setColour (selected
            ? juce::Colour (0xff11110f)
            : juce::Colour (0xffd7ceb8));
        g.drawText (labels[i],
                    juce::roundToInt (x[i]), 4, 154, 32,
                    juce::Justification::centred);
    }
}

void CarnivalScreenComponent::paintSequence (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff090908));

    if (sequenceBackground.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (sequenceBackground,
                     0, 0, 720, 720,
                     0, 0,
                     sequenceBackground.getWidth(),
                     sequenceBackground.getHeight(),
                     false);
    }

    for (int track = 0;
         track < FlowerStandaloneAudioProcessor::carnivalTrackCount;
         ++track)
    {
        for (int step = 0;
             step < FlowerStandaloneAudioProcessor::carnivalStepCount;
             ++step)
        {
            const float x = step * cellSize;
            const float y = track * cellSize;

            if (processor.getCarnivalStepEnabled (track, step))
            {
                if (activeStepImage.isValid())
                {
                    g.drawImage (activeStepImage,
                                 juce::roundToInt (x),
                                 juce::roundToInt (y),
                                 juce::roundToInt (cellSize),
                                 juce::roundToInt (cellSize),
                                 0, 0,
                                 activeStepImage.getWidth(),
                                 activeStepImage.getHeight(),
                                 false);
                }
                else
                {
                    g.setColour (juce::Colour (0xffd9c58f));
                    g.fillRect (x, y, cellSize, cellSize);
                }
            }

            if (processor.carnivalStepHasLocks (track, step))
            {
                g.setColour (juce::Colour (0xffffe7a5).withAlpha (0.92f));
                g.fillEllipse (
                    x + cellSize - 10.0f, y + 5.0f,
                    5.0f, 5.0f);
            }
        }

        const float y = track * cellSize;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (8.0f * cellSize, y, 2.0f * cellSize, cellSize);

        g.setColour (juce::Colour (0xffe5dcc6));
        g.setFont (juce::FontOptions (11.5f).withStyle ("Bold"));
        g.drawText ("<", 576, juce::roundToInt (y), 24, 72,
                    juce::Justification::centred);
        g.drawText (getMachineName (track),
                    597, juce::roundToInt (y), 102, 72,
                    juce::Justification::centred);
        g.drawText (">", 696, juce::roundToInt (y), 24, 72,
                    juce::Justification::centred);
    }

    const int playhead = processor.getCarnivalCurrentStep();
    if (playhead >= 0 && playhead < 8)
    {
        g.setColour (juce::Colours::white.withAlpha (0.085f));
        g.fillRect (playhead * cellSize, 0.0f, cellSize, 720.0f);
        g.setColour (juce::Colour (0xffffe8b4).withAlpha (0.82f));
        g.drawRect (playhead * cellSize, 0.0f, cellSize, 720.0f, 2.0f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.68f));
    for (int column = 1; column < gridColumns; ++column)
        g.fillRect (column * cellSize - 1.0f, 0.0f, 2.0f, 720.0f);
    for (int row = 1; row < gridRows; ++row)
        g.fillRect (0.0f, row * cellSize - 1.0f, 720.0f, 2.0f);

    g.setColour (juce::Colour (0xffffe7a5).withAlpha (0.90f));
    g.drawRect (cursorColumn * cellSize + 2.0f,
                cursorRow * cellSize + 2.0f,
                cellSize - 4.0f, cellSize - 4.0f,
                2.0f);

    paintHeader (g, processor.isCarnivalPlaying() ? "PLAY" : "STOP");
}

void CarnivalScreenComponent::paintParameter (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a09));

    if (sequenceBackground.isValid())
    {
        juce::Graphics::ScopedSaveState backgroundState (g);
        g.setOpacity (0.10f);
        g.drawImage (sequenceBackground,
                     0, 0, 720, 720,
                     0, 0,
                     sequenceBackground.getWidth(),
                     sequenceBackground.getHeight(),
                     false);
    }

    g.setColour (juce::Colour (0xffe4dac0));
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));

    juce::String context =
        "TR " + juce::String (selectedTrack + 1)
        + "  " + getMachineName (selectedTrack);

    if (parameterLockMode)
        context += "  STEP " + juce::String (selectedStep + 1) + "  P-LOCK";
    else
        context += "  TRACK";

    g.drawText (context, 48, 54, 624, 42,
                juce::Justification::centredLeft);

    for (int param = 0;
         param < FlowerStandaloneAudioProcessor::carnivalParamCount;
         ++param)
    {
        const float y = 110.0f + param * 66.0f;
        const bool locked =
            parameterLockMode
            && processor.getCarnivalParamLockEnabled (
                selectedTrack, selectedStep, param);

        const float value =
            locked
                ? processor.getCarnivalParamLockValue (
                    selectedTrack, selectedStep, param)
                : processor.getCarnivalBaseParam (
                    selectedTrack, param);

        g.setColour (param == selectedParam
            ? juce::Colour (0xffd9ceb3).withAlpha (0.20f)
            : juce::Colour (0xff1d1c19).withAlpha (0.88f));
        g.fillRoundedRectangle (48.0f, y, 624.0f, 52.0f, 6.0f);

        g.setColour (juce::Colour (0xffd9cfb8));
        g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
        g.drawText (getParameterName (param),
                    62, juce::roundToInt (y), 145, 52,
                    juce::Justification::centredLeft);

        const juce::Rectangle<float> bar (218.0f, y + 19.0f, 330.0f, 14.0f);
        g.setColour (juce::Colour (0xff3a3730));
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (locked
            ? juce::Colour (0xffffdf91)
            : juce::Colour (0xffc7bfa9));
        g.fillRoundedRectangle (
            bar.withWidth (bar.getWidth() * value), 4.0f);

        g.setColour (juce::Colour (0xffe8dfcb));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (getParameterValueText (param, value),
                    558, juce::roundToInt (y), 100, 52,
                    juce::Justification::centredRight);

        if (locked)
        {
            g.setColour (juce::Colour (0xffffdf91));
            g.fillEllipse (204.0f, y + 22.0f, 7.0f, 7.0f);
        }
    }

    g.setColour (juce::Colour (0xff292722));
    g.fillRoundedRectangle (48.0f, 590.0f, 288.0f, 52.0f, 7.0f);
    g.fillRoundedRectangle (384.0f, 590.0f, 288.0f, 52.0f, 7.0f);

    g.setColour (juce::Colour (0xffded3b7));
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.drawText (parameterLockMode ? "CLEAR LOCKS" : "TRACK PARAMETERS",
                48, 590, 288, 52, juce::Justification::centred);
    g.drawText ("BACK TO SEQUENCE",
                384, 590, 288, 52, juce::Justification::centred);

    paintHeader (g, "PARAMETER");
}

void CarnivalScreenComponent::paintConfig (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a09));

    static constexpr const char* labels[]
    {
        "CLOCK SOURCE", "BPM", "TRANSPORT",
        "MIDI CHANNEL", "CLEAR PATTERN", "EXIT CARNIVAL"
    };

    for (int row = 0; row < 6; ++row)
    {
        const float y = 120.0f + row * 76.0f;
        const bool selected = row == configRow;

        g.setColour (selected
            ? juce::Colour (0xffd9ceb3)
            : juce::Colour (0xff24231f));
        g.fillRoundedRectangle (64.0f, y, 592.0f, 58.0f, 7.0f);

        g.setColour (selected
            ? juce::Colour (0xff11110f)
            : juce::Colour (0xffded5bf));
        g.setFont (juce::FontOptions (17.0f).withStyle ("Bold"));
        g.drawText (labels[row],
                    82, juce::roundToInt (y), 260, 58,
                    juce::Justification::centredLeft);

        juce::String value;

        if (row == 0)
            value = processor.isCarnivalClockMidi() ? "MIDI" : "INTERNAL";
        else if (row == 1)
            value = processor.isCarnivalClockMidi()
                ? "EXT"
                : juce::String (juce::roundToInt (processor.getCarnivalBpm()));
        else if (row == 2)
            value = processor.isCarnivalClockMidi()
                ? (processor.isCarnivalPlaying() ? "MIDI RUN" : "WAIT MIDI")
                : (processor.isCarnivalPlaying() ? "PLAYING" : "STOPPED");
        else if (row == 3)
            value = juce::String (processor.getConfiguredMidiChannel());
        else if (row == 4)
            value = "CLEAR";
        else
            value = "EXIT";

        g.drawText (value,
                    360, juce::roundToInt (y), 278, 58,
                    juce::Justification::centredRight);
    }

    g.setColour (juce::Colour (0xff77705f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText (
        "MIDI clock: Start / Continue / Stop, 24 PPQN, 12 clocks per step.",
        64, 592, 592, 42,
        juce::Justification::centredLeft, 2);

    paintHeader (g, "CONFIG");
}

void CarnivalScreenComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    juce::Graphics::ScopedSaveState savedState (g);
    g.addTransform (juce::AffineTransform::scale (
        static_cast<float> (getWidth()) / designSize,
        static_cast<float> (getHeight()) / designSize));

    if (page == Page::Sequence)
        paintSequence (g);
    else if (page == Page::Parameter)
        paintParameter (g);
    else
        paintConfig (g);
}

void CarnivalScreenComponent::timerCallback()
{
    repaint();
}

void CarnivalScreenComponent::openParameterForStep (
    int track, int step)
{
    selectedTrack = juce::jlimit (
        0, FlowerStandaloneAudioProcessor::carnivalTrackCount - 1, track);
    selectedStep = juce::jlimit (
        0, FlowerStandaloneAudioProcessor::carnivalStepCount - 1, step);
    selectedParam = 0;
    parameterLockMode = true;
    setPage (Page::Parameter);
}

void CarnivalScreenComponent::openParameterForTrack (int track)
{
    selectedTrack = juce::jlimit (
        0, FlowerStandaloneAudioProcessor::carnivalTrackCount - 1, track);
    selectedParam = 0;
    parameterLockMode = false;
    setPage (Page::Parameter);
}

void CarnivalScreenComponent::adjustCurrentParameter (float normalised)
{
    normalised = juce::jlimit (0.0f, 1.0f, normalised);

    if (parameterLockMode)
    {
        processor.setCarnivalParamLock (
            selectedTrack, selectedStep, selectedParam,
            true, normalised);
    }
    else
    {
        processor.setCarnivalBaseParam (
            selectedTrack, selectedParam, normalised);
    }

    processor.previewCarnivalTrack (selectedTrack);
    repaint();
}

void CarnivalScreenComponent::nudgeCurrentParameter (float delta)
{
    float value = parameterLockMode
        && processor.getCarnivalParamLockEnabled (
            selectedTrack, selectedStep, selectedParam)
        ? processor.getCarnivalParamLockValue (
            selectedTrack, selectedStep, selectedParam)
        : processor.getCarnivalBaseParam (
            selectedTrack, selectedParam);

    adjustCurrentParameter (value + delta);
}

void CarnivalScreenComponent::activateConfigRow (int direction)
{
    direction = direction < 0 ? -1 : 1;

    if (configRow == 0)
    {
        processor.setCarnivalClockMidi (
            ! processor.isCarnivalClockMidi());
    }
    else if (configRow == 1)
    {
        if (! processor.isCarnivalClockMidi())
            processor.setCarnivalBpm (
                processor.getCarnivalBpm()
                + 2.0f * static_cast<float> (direction));
    }
    else if (configRow == 2)
    {
        if (! processor.isCarnivalClockMidi())
            processor.setCarnivalPlaying (
                ! processor.isCarnivalPlaying());
    }
    else if (configRow == 3)
    {
        int channel =
            processor.getConfiguredMidiChannel() + direction;
        if (channel < 1)
            channel = 16;
        if (channel > 16)
            channel = 1;
        processor.setConfiguredMidiChannel (channel);
    }
    else if (configRow == 4)
    {
        processor.clearCarnivalPattern();
    }
    else if (onExitRequested)
    {
        onExitRequested();
    }

    repaint();
}

void CarnivalScreenComponent::mouseDown (const juce::MouseEvent& e)
{
    pointerDownDesign = toDesignPoint (e.position);
    pointerDownMs = juce::Time::getMillisecondCounterHiRes();
    pointerDragged = false;
}

void CarnivalScreenComponent::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = toDesignPoint (e.position);

    if (p.getDistanceFrom (pointerDownDesign) > 5.0f)
        pointerDragged = true;

    if (page != Page::Parameter || p.y < 100.0f)
        return;

    const int row =
        static_cast<int> ((p.y - 110.0f) / 66.0f);

    if (row < 0
        || row >= FlowerStandaloneAudioProcessor::carnivalParamCount)
        return;

    const float rowY = 110.0f + row * 66.0f;
    if (p.y < rowY || p.y > rowY + 52.0f)
        return;

    selectedParam = row;
    adjustCurrentParameter (
        (p.x - 218.0f) / 330.0f);
}

void CarnivalScreenComponent::mouseUp (const juce::MouseEvent& e)
{
    const auto p = toDesignPoint (e.position);

    if (p.y < 40.0f && p.x >= 240.0f)
    {
        selectPageFromHeader (p.x);
        return;
    }

    const double heldMs =
        juce::Time::getMillisecondCounterHiRes() - pointerDownMs;
    const bool longPress =
        heldMs >= 450.0
        && p.getDistanceFrom (pointerDownDesign) <= 10.0f;

    if (page == Page::Sequence)
    {
        const int column = juce::jlimit (
            0, gridColumns - 1,
            static_cast<int> (p.x / cellSize));
        const int row = juce::jlimit (
            0, gridRows - 1,
            static_cast<int> (p.y / cellSize));

        cursorColumn = column;
        cursorRow = row;
        selectedTrack = row;

        if (column < 8)
        {
            selectedStep = column;

            if (longPress)
                openParameterForStep (row, column);
            else
                processor.toggleCarnivalStep (row, column);
        }
        else
        {
            if (longPress)
                openParameterForTrack (row);
            else
                processor.cycleCarnivalInstrument (
                    row, column == 8 ? -1 : 1);
        }

        repaint();
        return;
    }

    if (page == Page::Parameter)
    {
        if (p.y >= 590.0f && p.y <= 642.0f)
        {
            if (p.x < 360.0f)
            {
                if (parameterLockMode)
                    processor.clearCarnivalStepLocks (
                        selectedTrack, selectedStep);
            }
            else
            {
                showSequencePage();
            }

            repaint();
            return;
        }

        const int row =
            static_cast<int> ((p.y - 110.0f) / 66.0f);

        if (row >= 0
            && row < FlowerStandaloneAudioProcessor::carnivalParamCount)
        {
            const float rowY = 110.0f + row * 66.0f;
            if (p.y >= rowY && p.y <= rowY + 52.0f)
            {
                selectedParam = row;
                adjustCurrentParameter (
                    (p.x - 218.0f) / 330.0f);
            }
        }

        return;
    }

    const int row =
        static_cast<int> ((p.y - 120.0f) / 76.0f);

    if (row >= 0 && row < 6)
    {
        const float rowY = 120.0f + row * 76.0f;
        if (p.y >= rowY && p.y <= rowY + 58.0f)
        {
            configRow = row;
            const int direction = p.x < 360.0f ? -1 : 1;
            activateConfigRow (direction);
        }
    }
}

bool CarnivalScreenComponent::handleKeyPress (
    const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const auto ch = key.getTextCharacter();

    if (code == juce::KeyPress::F19Key || ch == 'c' || ch == 'C')
    {
        const int next =
            (static_cast<int> (page) + 1) % 3;
        if (next == static_cast<int> (Page::Sequence))
            showSequencePage();
        else
            setPage (static_cast<Page> (next));
        return true;
    }

    if (code == juce::KeyPress::F13Key || ch == 'a' || ch == 'A')
    {
        if (! processor.isCarnivalClockMidi())
            processor.setCarnivalPlaying (
                ! processor.isCarnivalPlaying());
        return true;
    }

    if (page == Page::Sequence)
    {
        if (code == juce::KeyPress::leftKey)
            cursorColumn = juce::jmax (0, cursorColumn - 1);
        else if (code == juce::KeyPress::rightKey)
            cursorColumn = juce::jmin (9, cursorColumn + 1);
        else if (code == juce::KeyPress::upKey)
            cursorRow = juce::jmax (0, cursorRow - 1);
        else if (code == juce::KeyPress::downKey)
            cursorRow = juce::jmin (9, cursorRow + 1);
        else if (code == juce::KeyPress::F14Key || ch == 'b' || ch == 'B')
        {
            if (cursorColumn < 8)
            {
                selectedStep = cursorColumn;
                processor.toggleCarnivalStep (
                    cursorRow, cursorColumn);
            }
            else
            {
                processor.cycleCarnivalInstrument (
                    cursorRow, cursorColumn == 8 ? -1 : 1);
            }
        }
        else if (code == juce::KeyPress::F16Key || ch == 'y' || ch == 'Y')
        {
            if (cursorColumn < 8)
                openParameterForStep (
                    cursorRow, cursorColumn);
            else
                openParameterForTrack (cursorRow);
        }
        else
        {
            return false;
        }

        selectedTrack = cursorRow;
        repaint();
        return true;
    }

    if (page == Page::Parameter)
    {
        if (code == juce::KeyPress::upKey)
            selectedParam = juce::jmax (0, selectedParam - 1);
        else if (code == juce::KeyPress::downKey)
            selectedParam = juce::jmin (
                FlowerStandaloneAudioProcessor::carnivalParamCount - 1,
                selectedParam + 1);
        else if (code == juce::KeyPress::leftKey)
            nudgeCurrentParameter (-0.025f);
        else if (code == juce::KeyPress::rightKey)
            nudgeCurrentParameter (0.025f);
        else if ((code == juce::KeyPress::F14Key || ch == 'b' || ch == 'B')
                 && parameterLockMode)
        {
            const bool enabled =
                processor.getCarnivalParamLockEnabled (
                    selectedTrack, selectedStep, selectedParam);
            const float value =
                enabled
                    ? processor.getCarnivalParamLockValue (
                        selectedTrack, selectedStep, selectedParam)
                    : processor.getCarnivalBaseParam (
                        selectedTrack, selectedParam);

            processor.setCarnivalParamLock (
                selectedTrack, selectedStep, selectedParam,
                ! enabled, value);
        }
        else if (code == juce::KeyPress::F15Key || ch == 'x' || ch == 'X')
        {
            showSequencePage();
        }
        else
        {
            return false;
        }

        repaint();
        return true;
    }

    if (code == juce::KeyPress::upKey)
        configRow = juce::jmax (0, configRow - 1);
    else if (code == juce::KeyPress::downKey)
        configRow = juce::jmin (5, configRow + 1);
    else if (code == juce::KeyPress::leftKey)
        activateConfigRow (-1);
    else if (code == juce::KeyPress::rightKey
             || code == juce::KeyPress::F14Key
             || ch == 'b' || ch == 'B')
        activateConfigRow (1);
    else if (code == juce::KeyPress::F15Key || ch == 'x' || ch == 'X')
        showSequencePage();
    else
        return false;

    repaint();
    return true;
}

FlowerStandaloneAudioProcessorEditor::FlowerStandaloneAudioProcessorEditor (
    FlowerStandaloneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      carnivalScreen (p)
{
    setLookAndFeel (&retroLookAndFeel);
    setOpaque (true);
    setWantsKeyboardFocus (true);

   #if JUCE_ANDROID
    // Android fullscreen uses density-independent logical coordinates.
    // A fixed 720x720 logical editor can therefore be larger than a physical
    // 720x720 panel and gets clipped. Let the standalone host size the editor
    // to the actual fullscreen content bounds instead.
    setResizable (true, false);
   #else
    constexpr int canvasSize = 720;
    setSize (canvasSize, canvasSize);
    setResizable (false, false);
    setResizeLimits (canvasSize, canvasSize, canvasSize, canvasSize);
   #endif

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
    addAndMakeVisible (configScreen);
    addAndMakeVisible (carnivalScreen);
    addAndMakeVisible (twilightRealtime3D);
    configScreen.setVisible (false);
    carnivalScreen.setVisible (false);

    // Branch-local graphics feasibility test only.
    // The production FLOWER UI/audio paths stay compiled but hidden.
    performancePad.setVisible (false);
    twilightRealtime3D.setVisible (true);
    twilightRealtime3D.toFront (false);

    rootClass = processor.getConfiguredRoot();
    scaleIndex = processor.getConfiguredScale();
    delayEnabled = processor.getDefaultEffectsEnabled();
    granularEnabled = delayEnabled;
    yEffectDreamy = processor.getConfiguredYEffectDreamy();
    midiChannel = processor.getConfiguredMidiChannel();

    performancePad.setEffectState (
        arpEnabled, delayEnabled, granularEnabled, yEffectDreamy);
    configScreen.setValues (
        rootClass, scaleIndex, delayEnabled, yEffectDreamy, midiChannel);

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

    configScreen.onRootChanged =
        [this] (int value)
        {
            rootClass = value;
            processor.setConfiguredRoot (rootClass);
        };

    configScreen.onScaleChanged =
        [this] (int value)
        {
            scaleIndex = value;
            processor.setConfiguredScale (scaleIndex);
        };

    configScreen.onEffectsChanged =
        [this] (bool enabled)
        {
            delayEnabled = enabled;
            granularEnabled = enabled;
            processor.setDefaultEffectsEnabled (enabled);
            performancePad.setEffectState (
                arpEnabled, delayEnabled, granularEnabled, yEffectDreamy);
        };

    configScreen.onYEffectModeChanged =
        [this] (bool dreamy)
        {
            yEffectDreamy = dreamy;
            processor.setConfiguredYEffectDreamy (yEffectDreamy);
            performancePad.setEffectState (
                arpEnabled, delayEnabled, granularEnabled, yEffectDreamy);
        };

    configScreen.onMidiChannelChanged =
        [this] (int channel)
        {
            midiChannel = juce::jlimit (1, 16, channel);
            processor.setConfiguredMidiChannel (midiChannel);
        };

    configScreen.onCarnivalRequested =
        [this]
        {
            stopAll();

            if (dpadActive)
                endDpadControl();
            if (bpmAdjustActive)
                endBpmAdjust();
            if (looperButtonActive)
                endLooperButton();

            configVisible = false;
            carnivalVisible = true;
            processor.setCarnivalEnabled (true);
            carnivalScreen.showSequencePage();

            performancePad.setVisible (false);
            configScreen.setVisible (false);
            carnivalScreen.setBounds (getLocalBounds());
            carnivalScreen.setVisible (true);
            carnivalScreen.toFront (false);
            grabKeyboardFocus();
        };

    carnivalScreen.onExitRequested =
        [this]
        {
            processor.setCarnivalEnabled (false);
            carnivalVisible = false;
            carnivalScreen.setVisible (false);

            configVisible = true;
            configScreen.setValues (
                rootClass,
                scaleIndex,
                processor.getDefaultEffectsEnabled(),
                yEffectDreamy,
                processor.getConfiguredMidiChannel());
            configScreen.setBounds (getLocalBounds());
            performancePad.setVisible (false);
            configScreen.setVisible (true);
            configScreen.toFront (false);
            grabKeyboardFocus();
        };

    configScreen.onCloseRequested =
        [this]
        {
            if (configVisible)
                toggleConfig();
        };

    processor.setConfiguredRoot (rootClass);
    processor.setConfiguredScale (scaleIndex);
    processor.setPerformanceBpm (bpm);
    processor.setPerformanceHold (hold);
    processor.setPerformanceArpEnabled (arpEnabled);
    processor.setPerformanceDelayEnabled (delayEnabled);
    processor.setPerformanceGranularEnabled (granularEnabled);
    processor.setPerformanceDreamyMode (yEffectDreamy);
    processor.setConfiguredMidiChannel (midiChannel);

    if (! twilightRealtime3DPrototypeMode)
        startTimer (40);

    grabKeyboardFocus();
}

FlowerStandaloneAudioProcessorEditor::~FlowerStandaloneAudioProcessorEditor()
{
    stopTimer();
    processor.setCarnivalEnabled (false);
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
    configScreen.setBounds (getLocalBounds());
    carnivalScreen.setBounds (getLocalBounds());
    twilightRealtime3D.setBounds (getLocalBounds());
}

void FlowerStandaloneAudioProcessorEditor::applyRootDelta (int delta)
{
    rootClass = (rootClass + delta) % 12;
    if (rootClass < 0)
        rootClass += 12;

    processor.setConfiguredRoot (rootClass);
    configScreen.setValues (
        rootClass, scaleIndex,
        processor.getDefaultEffectsEnabled(),
        yEffectDreamy,
        midiChannel);
}

void FlowerStandaloneAudioProcessorEditor::applyBpmDelta (float delta)
{
    bpm = juce::jlimit (50.0f, 200.0f, bpm + delta);
    processor.setPerformanceBpm (bpm);

    if (bpmAdjustActive)
        performancePad.setBpmDisplay (bpm, true);
}

void FlowerStandaloneAudioProcessorEditor::cycleScale (int delta)
{
    scaleIndex = (scaleIndex + delta) % 5;
    if (scaleIndex < 0)
        scaleIndex += 5;

    processor.setConfiguredScale (scaleIndex);
    configScreen.setValues (
        rootClass, scaleIndex,
        processor.getDefaultEffectsEnabled(),
        yEffectDreamy,
        midiChannel);
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
    performancePad.setEffectState (
        arpEnabled, delayEnabled, granularEnabled, yEffectDreamy);
}

void FlowerStandaloneAudioProcessorEditor::toggleDelay()
{
    delayEnabled = ! delayEnabled;
    processor.setPerformanceDelayEnabled (delayEnabled);
    performancePad.setEffectState (
        arpEnabled, delayEnabled, granularEnabled, yEffectDreamy);
}

void FlowerStandaloneAudioProcessorEditor::toggleGranular()
{
    granularEnabled = ! granularEnabled;
    processor.setPerformanceGranularEnabled (granularEnabled);
    performancePad.setEffectState (
        arpEnabled, delayEnabled, granularEnabled, yEffectDreamy);
}

void FlowerStandaloneAudioProcessorEditor::toggleConfig()
{
    configVisible = ! configVisible;

    if (configVisible)
    {
        stopAll();

        if (dpadActive)
            endDpadControl();
        if (bpmAdjustActive)
            endBpmAdjust();

        configScreen.setValues (
            rootClass,
            scaleIndex,
            processor.getDefaultEffectsEnabled(),
            yEffectDreamy,
        midiChannel);

        // SELECT-opened CONFIG must use the exact same fullscreen bounds as
        // the performance surface. The CONFIG UI itself is authored in a
        // 720x720 design space and scaled inside ConfigScreenComponent::paint().
        configScreen.setBounds (getLocalBounds());
        configScreen.repaint();

        performancePad.setVisible (false);
        configScreen.setVisible (true);
        configScreen.toFront (false);
    }
    else
    {
        configScreen.setVisible (false);
        performancePad.setVisible (true);
        performancePad.toFront (false);
        grabKeyboardFocus();
    }
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
    performancePad.setBpmDisplay (bpm, true);
    refreshControlTimer();
}

void FlowerStandaloneAudioProcessorEditor::endBpmAdjust()
{
    bpmAdjustActive = false;
    bpmAdjustDirection = 0;
    performancePad.setBpmDisplay (bpm, false);
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
    // Keep a lightweight 25 Hz poll alive so incoming MIDI CC changes are
    // reflected in XY and effect-state visuals.
    if (! isTimerRunning())
        startTimer (40);
}

void FlowerStandaloneAudioProcessorEditor::timerCallback()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    const float externalX = processor.getPerformanceX();
    const float externalY = processor.getPerformanceY();
    bpm = processor.getPerformanceBpm();

    const bool externalArp = processor.getPerformanceArpEnabled();
    const bool externalDelay = processor.getPerformanceDelayEnabled();
    const bool externalYEffect = processor.getPerformanceYEffectEnabled();
    const bool externalHold = processor.getPerformanceHold();

    if (externalArp != arpEnabled
        || externalDelay != delayEnabled
        || externalYEffect != granularEnabled)
    {
        arpEnabled = externalArp;
        delayEnabled = externalDelay;
        granularEnabled = externalYEffect;
        performancePad.setEffectState (
            arpEnabled, delayEnabled, granularEnabled, yEffectDreamy);
    }

    if (externalHold != hold)
    {
        hold = externalHold;
        performancePad.setHeld (hold);
    }

    if (! dpadActive
        && (std::abs (performancePad.getXValue() - externalX) > 0.0005f
            || std::abs (performancePad.getYValue() - externalY) > 0.0005f))
    {
        performancePad.setExternalPosition (externalX, externalY);
    }

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
    if (twilightRealtime3DPrototypeMode)
        return false;

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
    if (twilightRealtime3DPrototypeMode)
        return true;

    const int code = key.getKeyCode();
    const auto ch = key.getTextCharacter();

    if (carnivalVisible)
    {
        int latchCode = 0;
        if (code == juce::KeyPress::F13Key) latchCode = 1;
        else if (code == juce::KeyPress::F14Key) latchCode = 2;
        else if (code == juce::KeyPress::F15Key) latchCode = 3;
        else if (code == juce::KeyPress::F16Key) latchCode = 4;
        else if (code == juce::KeyPress::F19Key) latchCode = 19;

        if (latchCode != 0 && toggleButtonLatchCode == latchCode)
            return true;

        const bool used = carnivalScreen.handleKeyPress (key);
        if (used && latchCode != 0)
            toggleButtonLatchCode = latchCode;
        return used;
    }

    // Android gamepad bridge:
    // SELECT=F19, A=F13, B=F14, X=F15, Y=F16, L1=F17, R1=F18.
    if (code == juce::KeyPress::F19Key || ch == 'c' || ch == 'C')
    {
        if (toggleButtonLatchCode != 19)
        {
            toggleConfig();
            toggleButtonLatchCode = 19;
        }
        return true;
    }

    if (configVisible)
    {
        if (code == juce::KeyPress::upKey)
        {
            configScreen.moveSelection (-1);
            return true;
        }

        if (code == juce::KeyPress::downKey)
        {
            configScreen.moveSelection (1);
            return true;
        }

        if (code == juce::KeyPress::leftKey)
        {
            configScreen.adjustSelected (-1);
            return true;
        }

        if (code == juce::KeyPress::rightKey)
        {
            configScreen.adjustSelected (1);
            return true;
        }

        if (code == juce::KeyPress::F14Key || ch == 'b' || ch == 'B')
        {
            configScreen.activateSelected();
            return true;
        }

        return true;
    }

    if (code == juce::KeyPress::leftKey
        || code == juce::KeyPress::rightKey
        || code == juce::KeyPress::upKey
        || code == juce::KeyPress::downKey)
    {
        beginDpadControl (code);
        return true;
    }

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

    // Desktop/debug fallbacks retained outside the gamepad assignments.
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

    if (ch >= '1' && ch <= '5')
    {
        scaleIndex = static_cast<int> (ch - '1');
        processor.setConfiguredScale (scaleIndex);
        configScreen.setValues (
            rootClass, scaleIndex,
            processor.getDefaultEffectsEnabled(),
            yEffectDreamy,
        midiChannel);
        return true;
    }

    if (ch == ' ' || code == juce::KeyPress::escapeKey)
    {
        stopAll();
        return true;
    }

    return false;
}
