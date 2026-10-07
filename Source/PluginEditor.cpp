#include "PluginEditor.h"
#include "FlowerFrameData.h"

#if JUCE_ANDROID
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
 #include <jni.h>
 #include <juce_core/native/juce_JNIHelpers_android.h>
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

constexpr const char* scaleNames[]
{
    "MINOR PENT", "NATURAL MINOR", "MAJOR", "DORIAN", "RANDOM"
};

constexpr const char* audioBufferNames[]
{
    "AUTO", "0.5 BURST", "0.75 BURST", "1 BURST",
    "1.5 BURSTS", "2 BURSTS", "3 BURSTS", "4 BURSTS",
    "5 BURSTS", "6 BURSTS", "7 BURSTS", "8 BURSTS"
};

constexpr float audioBufferBursts[]
{
    0.0f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f,
    3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f
};

#if JUCE_ANDROID
int getAndroidOutputFramesPerBuffer()
{
    auto* env = juce::getEnv();
    if (env == nullptr)
        return 0;

    const auto context = juce::getAppContext();
    if (context == nullptr)
        return 0;

    jclass contextClass = env->GetObjectClass (context.get());
    if (contextClass == nullptr)
        return 0;

    jmethodID getSystemService = env->GetMethodID (
        contextClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (getSystemService == nullptr)
    {
        env->DeleteLocalRef (contextClass);
        return 0;
    }

    jstring audioService = env->NewStringUTF ("audio");
    jobject audioManager = env->CallObjectMethod (
        context.get(), getSystemService, audioService);
    env->DeleteLocalRef (audioService);
    env->DeleteLocalRef (contextClass);
    if (audioManager == nullptr)
        return 0;

    jclass audioManagerClass = env->FindClass ("android/media/AudioManager");
    jmethodID getProperty = audioManagerClass != nullptr
        ? env->GetMethodID (
            audioManagerClass, "getProperty",
            "(Ljava/lang/String;)Ljava/lang/String;")
        : nullptr;

    int frames = 0;
    if (getProperty != nullptr)
    {
        jstring key = env->NewStringUTF (
            "android.media.property.OUTPUT_FRAMES_PER_BUFFER");
        jstring value = static_cast<jstring> (
            env->CallObjectMethod (audioManager, getProperty, key));
        env->DeleteLocalRef (key);

        if (value != nullptr)
        {
            const char* chars = env->GetStringUTFChars (value, nullptr);
            if (chars != nullptr)
            {
                frames = juce::String::fromUTF8 (chars).getIntValue();
                env->ReleaseStringUTFChars (value, chars);
            }
            env->DeleteLocalRef (value);
        }
    }

    if (audioManagerClass != nullptr)
        env->DeleteLocalRef (audioManagerClass);
    env->DeleteLocalRef (audioManager);
    return juce::jmax (0, frames);
}
#else
int getAndroidOutputFramesPerBuffer() { return 0; }
#endif

}

PerformancePadComponent::PerformancePadComponent()
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setWantsKeyboardFocus (false);

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

    videoFrames.load (
        BinaryData::flower_video_frames_pack,
        BinaryData::flower_video_frames_packSize);

    if (videoFrames.getFrameCount() > 0)
        startTimerHz (30);
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

void PerformancePadComponent::chooseNextMixedVisual()
{
    const int videoCount = videoFrames.getFrameCount();
    const int totalCount = tileCount + videoCount;
    if (totalCount <= 0)
        return;

    visualRandomState ^= visualRandomState << 13;
    visualRandomState ^= visualRandomState >> 17;
    visualRandomState ^= visualRandomState << 5;

    int next = static_cast<int> (
        visualRandomState % static_cast<uint32_t> (totalCount));

    if (totalCount > 1 && next == currentMixedVisual)
        next = (next + 1) % totalCount;

    currentMixedVisual = next;
    if (currentMixedVisual >= tileCount)
        currentVideoFrame =
            videoFrames.getFrame (currentMixedVisual - tileCount);
    else
        currentVideoFrame = {};
}

void PerformancePadComponent::timerCallback()
{
    if (! active && ! held && ! physicalPointerVisible)
        return;

    if (visualCooldown > 0)
    {
        --visualCooldown;
        return;
    }

    chooseNextMixedVisual();
    visualCooldown = 4; // EFFECTS cadence: about 133 ms at 30 Hz.
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

    const auto& fallbackFrame =
        frameImages[static_cast<size_t> (visualTileIndex)];
    const juce::Image* frame = &fallbackFrame;

    if (currentMixedVisual >= 0 && currentMixedVisual < tileCount)
        frame = &frameImages[static_cast<size_t> (currentMixedVisual)];
    else if (currentMixedVisual >= tileCount && currentVideoFrame.isValid())
        frame = &currentVideoFrame;

    if (! frame->isValid())
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
    g.drawImage (*frame,
                 0, 0, getWidth(), getHeight(),
                 0, 0, frame->getWidth(), frame->getHeight(),
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

    chooseNextMixedVisual();
    visualCooldown = 4;
    repaint();

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
    constexpr int rowHeight = 82;
    constexpr int firstY = 172;
    return { 72, firstY + row * rowHeight, 576, 66 };
}

juce::Rectangle<int> ConfigScreenComponent::getCloseBounds() const
{
    return { 516, 636, 132, 52 };
}


void ConfigScreenComponent::setValues (int newRootKey,
                                       int newScale,
                                       bool newEffectsEnabled,
                                       int newAudioBufferMode,
                                       const juce::String& newAudioStatus)
{
    rootKey = juce::jlimit (0, 11, newRootKey);
    scaleIndex = juce::jlimit (0, 4, newScale);
    effectsEnabled = newEffectsEnabled;
    audioBufferMode = juce::jlimit (0, 11, newAudioBufferMode);
    audioStatus = newAudioStatus;
    repaint();
}

void ConfigScreenComponent::moveSelection (int delta)
{
    selectedRow = (selectedRow + delta) % 4;
    if (selectedRow < 0)
        selectedRow += 4;
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
    else
    {
        audioBufferMode = (audioBufferMode + delta) % 12;
        if (audioBufferMode < 0)
            audioBufferMode += 12;

        if (onAudioBufferChanged)
            onAudioBufferChanged (audioBufferMode);
    }

    repaint();
}

void ConfigScreenComponent::activateSelected()
{
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
    else
    {
        if (onAudioBufferChanged)
            onAudioBufferChanged (audioBufferMode);
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
                72, 124, 576, 34,
                juce::Justification::centredLeft);

    for (int row = 0; row < 4; ++row)
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
        g.setFont (juce::FontOptions (21.0f).withStyle ("Bold"));

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
        else
        {
            label = "AUDIO BUFFER";
            value = audioBufferNames[audioBufferMode];
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
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText (
        audioStatus.isNotEmpty()
            ? audioStatus
            : "AUDIO: waiting for Android output buffer information",
        72, 512, 576, 108,
        juce::Justification::topLeft, 4);
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

    for (int row = 0; row < 4; ++row)
    {
        const auto bounds = getRowBounds (row);
        if (! bounds.contains (designPoint))
            continue;

        selectedRow = row;

        if (row == 2)
        {
            adjustSelected (1);
        }
        else
        {
            const int delta = designPoint.x < bounds.getCentreX()
                ? -1
                : 1;
            adjustSelected (delta);
        }

        return;
    }
}

FlowerStandaloneAudioProcessorEditor::FlowerStandaloneAudioProcessorEditor (
    FlowerStandaloneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p)
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
    configScreen.setVisible (false);

    rootClass = processor.getConfiguredRoot();
    scaleIndex = processor.getConfiguredScale();
    delayEnabled = processor.getDefaultEffectsEnabled();
    granularEnabled = delayEnabled;
    audioBufferMode = processor.getAudioBufferMode();
    refreshAudioBufferStatus();
    if (audioBufferMode != 0)
        applyAudioBufferMode (audioBufferMode);
    configScreen.setValues (
        rootClass, scaleIndex, delayEnabled,
        audioBufferMode, makeAudioBufferStatus());

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
        };

    configScreen.onAudioBufferChanged =
        [this] (int mode)
        {
            const int previous = audioBufferMode;
            if (applyAudioBufferMode (mode))
            {
                audioBufferMode = mode;
                processor.setAudioBufferMode (audioBufferMode);
            }
            else
            {
                applyAudioBufferMode (previous);
            }

            configScreen.setValues (
                rootClass, scaleIndex, delayEnabled,
                audioBufferMode, makeAudioBufferStatus());
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
    configScreen.setBounds (getLocalBounds());
}

void FlowerStandaloneAudioProcessorEditor::applyRootDelta (int delta)
{
    rootClass = (rootClass + delta) % 12;
    if (rootClass < 0)
        rootClass += 12;

    processor.setConfiguredRoot (rootClass);
    configScreen.setValues (rootClass, scaleIndex,
                            processor.getDefaultEffectsEnabled(),
                            audioBufferMode, makeAudioBufferStatus());
}

void FlowerStandaloneAudioProcessorEditor::applyBpmDelta (float delta)
{
    bpm = juce::jlimit (50.0f, 200.0f, bpm + delta);
    processor.setPerformanceBpm (bpm);
}

void FlowerStandaloneAudioProcessorEditor::cycleScale (int delta)
{
    scaleIndex = (scaleIndex + delta) % 5;
    if (scaleIndex < 0)
        scaleIndex += 5;

    processor.setConfiguredScale (scaleIndex);
    configScreen.setValues (rootClass, scaleIndex,
                            processor.getDefaultEffectsEnabled(),
                            audioBufferMode, makeAudioBufferStatus());
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

void FlowerStandaloneAudioProcessorEditor::refreshAudioBufferStatus()
{
#if JUCE_ANDROID
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        holder->deviceManager.getAudioDeviceSetup (setup);

        auto* device = holder->deviceManager.getCurrentAudioDevice();
        const int currentFrames = device != nullptr
            ? device->getCurrentBufferSizeSamples()
            : setup.bufferSize;
        const double currentRate = device != nullptr
            ? device->getCurrentSampleRate()
            : setup.sampleRate;

        if (audioDefaultBufferFrames < 0)
            audioDefaultBufferFrames = currentFrames;

        audioFramesPerBurst = getAndroidOutputFramesPerBuffer();
        if (audioFramesPerBurst <= 0 && currentFrames > 0)
            audioFramesPerBurst = currentFrames;

        audioActualBufferFrames = currentFrames;
        audioActualSampleRate = currentRate;
    }
#endif
}

juce::String FlowerStandaloneAudioProcessorEditor::makeAudioBufferStatus() const
{
    if (audioActualBufferFrames <= 0 || audioActualSampleRate <= 0.0)
        return "AUDIO: output buffer information unavailable"
             + (audioBufferError.isNotEmpty()
                    ? "\n" + audioBufferError
                    : juce::String());

    const double ms =
        1000.0 * static_cast<double> (audioActualBufferFrames)
        / audioActualSampleRate;
    const double bursts = audioFramesPerBurst > 0
        ? static_cast<double> (audioActualBufferFrames)
            / static_cast<double> (audioFramesPerBurst)
        : 0.0;

    juce::String result =
        "AUDIO: " + juce::String (audioActualBufferFrames)
        + " frames / " + juce::String (ms, 2) + " ms"
        + "\nBASE BURST: " + juce::String (audioFramesPerBurst)
        + " frames / ACTUAL: " + juce::String (bursts, 2) + " bursts";

    if (audioBufferError.isNotEmpty())
        result += "\n" + audioBufferError;

    return result;
}

bool FlowerStandaloneAudioProcessorEditor::applyAudioBufferMode (int mode)
{
#if JUCE_ANDROID
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
    {
        audioBufferError = "Buffer request failed: standalone holder unavailable";
        return false;
    }

    mode = juce::jlimit (0, 11, mode);
    refreshAudioBufferStatus();

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    holder->deviceManager.getAudioDeviceSetup (setup);

    int targetFrames = audioDefaultBufferFrames;
    if (mode > 0)
    {
        const int base = audioFramesPerBurst;
        if (base <= 0)
        {
            audioBufferError = "BUFFER REQUEST: base burst unavailable";
            return false;
        }

        targetFrames = juce::jmax (
            1, juce::roundToInt (
                static_cast<float> (base) * audioBufferBursts[mode]));

        if (auto* device = holder->deviceManager.getCurrentAudioDevice())
        {
            const auto available = device->getAvailableBufferSizes();
            if (! available.isEmpty())
            {
                int nearest = available[0];
                int nearestDistance = std::abs (nearest - targetFrames);
                for (const int candidate : available)
                {
                    const int distance = std::abs (candidate - targetFrames);
                    if (distance < nearestDistance)
                    {
                        nearest = candidate;
                        nearestDistance = distance;
                    }
                }
                targetFrames = nearest;
            }
        }
    }

    setup.bufferSize = targetFrames;
    const auto error =
        holder->deviceManager.setAudioDeviceSetup (setup, true);

    if (error.isNotEmpty())
    {
        audioBufferError = "BUFFER REQUEST: " + error;
        refreshAudioBufferStatus();
        return false;
    }

    audioBufferError.clear();
    refreshAudioBufferStatus();
    return true;
#else
    juce::ignoreUnused (mode);
    audioBufferError = "AUDIO BUFFER is Android-only";
    return false;
#endif
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

        refreshAudioBufferStatus();
        configScreen.setValues (
            rootClass,
            scaleIndex,
            processor.getDefaultEffectsEnabled(),
            audioBufferMode,
            makeAudioBufferStatus());

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
        configScreen.setValues (rootClass, scaleIndex,
                                processor.getDefaultEffectsEnabled());
        return true;
    }

    if (ch == ' ' || code == juce::KeyPress::escapeKey)
    {
        stopAll();
        return true;
    }

    return false;
}
