#include "PluginEditor.h"
#include "ParameterIDs.h"
#include <cmath>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#if JUCE_ANDROID
 #include <jni.h>
 #include <juce_core/native/juce_JNIHelpers_android.h>
#endif

namespace
{
#if JUCE_ANDROID
    juce::StringArray getAndroidPhysicalInputNames()
    {
        juce::StringArray result;
        auto* env = juce::getEnv();
        if (env == nullptr)
            return result;

        const auto context = juce::getAppContext();
        if (context == nullptr)
            return result;

        jclass contextClass = env->GetObjectClass (context.get());
        if (contextClass == nullptr)
            return result;

        jmethodID getSystemService = env->GetMethodID (
            contextClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
        if (getSystemService == nullptr)
        {
            env->DeleteLocalRef (contextClass);
            return result;
        }

        jstring audioService = env->NewStringUTF ("audio");
        jobject audioManager = env->CallObjectMethod (context.get(), getSystemService, audioService);
        env->DeleteLocalRef (audioService);
        env->DeleteLocalRef (contextClass);
        if (audioManager == nullptr)
            return result;

        jclass audioManagerClass = env->FindClass ("android/media/AudioManager");
        jmethodID getDevices = audioManagerClass != nullptr
            ? env->GetMethodID (audioManagerClass, "getDevices", "(I)[Landroid/media/AudioDeviceInfo;")
            : nullptr;

        // AudioManager.GET_DEVICES_INPUTS == 1 (API 23+, minSdk is 24).
        jobjectArray devices = getDevices != nullptr
            ? static_cast<jobjectArray> (env->CallObjectMethod (audioManager, getDevices, 1))
            : nullptr;

        jclass infoClass = env->FindClass ("android/media/AudioDeviceInfo");
        jmethodID getProductName = infoClass != nullptr
            ? env->GetMethodID (infoClass, "getProductName", "()Ljava/lang/CharSequence;")
            : nullptr;
        jmethodID getType = infoClass != nullptr
            ? env->GetMethodID (infoClass, "getType", "()I")
            : nullptr;

        jclass charSequenceClass = env->FindClass ("java/lang/CharSequence");
        jmethodID toStringMethod = charSequenceClass != nullptr
            ? env->GetMethodID (charSequenceClass, "toString", "()Ljava/lang/String;")
            : nullptr;

        if (devices != nullptr && getProductName != nullptr && toStringMethod != nullptr)
        {
            const auto count = env->GetArrayLength (devices);
            for (jsize i = 0; i < count; ++i)
            {
                jobject info = env->GetObjectArrayElement (devices, i);
                if (info == nullptr)
                    continue;

                jobject product = env->CallObjectMethod (info, getProductName);
                jstring productString = product != nullptr
                    ? static_cast<jstring> (env->CallObjectMethod (product, toStringMethod))
                    : nullptr;

                juce::String name;
                if (productString != nullptr)
                {
                    const char* chars = env->GetStringUTFChars (productString, nullptr);
                    if (chars != nullptr)
                    {
                        name = juce::String::fromUTF8 (chars);
                        env->ReleaseStringUTFChars (productString, chars);
                    }
                }

                if (name.isEmpty())
                    name = "Android input device";

                if (getType != nullptr)
                    name << "  [type " << (int) env->CallIntMethod (info, getType) << "]";

                result.addIfNotAlreadyThere (name);

                if (productString != nullptr) env->DeleteLocalRef (productString);
                if (product != nullptr) env->DeleteLocalRef (product);
                env->DeleteLocalRef (info);
            }
        }

        if (devices != nullptr) env->DeleteLocalRef (devices);
        if (charSequenceClass != nullptr) env->DeleteLocalRef (charSequenceClass);
        if (infoClass != nullptr) env->DeleteLocalRef (infoClass);
        if (audioManagerClass != nullptr) env->DeleteLocalRef (audioManagerClass);
        env->DeleteLocalRef (audioManager);
        return result;
    }
#else
    juce::StringArray getAndroidPhysicalInputNames() { return {}; }
#endif
}

RealtimeChordFxAudioProcessorEditor::RealtimeChordFxAudioProcessorEditor (RealtimeChordFxAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
#if JUCE_ANDROID
    // On Android use the host's actual logical content bounds instead of forcing
    // a 720x720 density-independent editor, which can be clipped on 720px panels.
    setResizable (true, false);
#else
    setSize (720, 720);
    setResizable (false, false);
    setResizeLimits (720, 720, 720, 720);
#endif
    frames.load (BinaryData::classroom_frames_pack, BinaryData::classroom_frames_packSize);
    currentFrame = frames.getFrame (0);
    loadedFrame = 0;
    eurekaFrames.load (
        BinaryData::eureka_frames_pack,
        BinaryData::eureka_frames_packSize);
    if (eurekaFrames.getFrameCount() > 0)
    {
        currentEurekaFrame = eurekaFrames.getFrame (0);
        loadedEurekaFrame = 0;
    }

   #if JUCE_ANDROID
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        holder->stopPlaying();
        holder->deviceManager.closeAudioDevice();

        auto audioError = holder->deviceManager.initialise (2, 2, nullptr, true);

        if (audioError.isNotEmpty())
        {
            juce::Logger::writeToLog (
                "RealtimeChordFX stereo input initialise failed; falling back to mono: " + audioError);
            audioError = holder->deviceManager.initialise (1, 2, nullptr, true);
        }

        if (audioError.isNotEmpty())
            juce::Logger::writeToLog ("RealtimeChordFX Android audio initialise failed: " + audioError);

        holder->getMuteInputValue().setValue (false);
        holder->startPlaying();
    }
   #else
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->getMuteInputValue().setValue (false);
   #endif

    startTimerHz (30);
}

RealtimeChordFxAudioProcessorEditor::~RealtimeChordFxAudioProcessorEditor() { stopTimer(); }

juce::Point<float> RealtimeChordFxAudioProcessorEditor::toDesign (juce::Point<float> p) const
{
    return { p.x * design / (float) juce::jmax (1, getWidth()),
             p.y * design / (float) juce::jmax (1, getHeight()) };
}

void RealtimeChordFxAudioProcessorEditor::setParameterFromX (DragParam which, float x)
{
    auto setNorm = [&] (const char* id, float norm)
    {
        if (auto* p = processor.state().getParameter (id)) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    };

    int idx = 0;
    if (which == DragParam::bar) idx = 1;
    else if (which == DragParam::width) idx = 2;
    else if (which == DragParam::length) idx = 3;

    int hazeIdx = -1;
    if (which == DragParam::hazeMix) hazeIdx = 0;
    else if (which == DragParam::hazeTime) hazeIdx = 1;
    else if (which == DragParam::hazeAmount) hazeIdx = 2;
    else if (which == DragParam::hazeFilter) hazeIdx = 3;
    else if (which == DragParam::hazeRepeat) hazeIdx = 4;
    else if (which == DragParam::hazeMod) hazeIdx = 5;
    else if (which == DragParam::hazeReverb) hazeIdx = 6;

    auto r = hazeIdx >= 0
        ? hazeParameterBounds (hazeIdx)
        : ((which == DragParam::hold || which == DragParam::effect)
            ? holdBounds() : parameterBounds (idx));

    const float norm =
        juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth());

    if (which == DragParam::complex) setNorm (ParamID::complex, norm);
    else if (which == DragParam::width) setNorm (ParamID::width, norm);
    else if (which == DragParam::length) setNorm (ParamID::length, norm);
    else if (which == DragParam::hold) setNorm (ParamID::hold, norm);
    else if (which == DragParam::effect) setNorm (ParamID::effect, norm);
    else if (which == DragParam::hazeMix) setNorm (ParamID::hazeMix, norm);
    else if (which == DragParam::hazeTime) setNorm (ParamID::hazeTime, norm);
    else if (which == DragParam::hazeAmount) setNorm (ParamID::hazeAmount, norm);
    else if (which == DragParam::hazeFilter) setNorm (ParamID::hazeFilter, norm);
    else if (which == DragParam::hazeRepeat) setNorm (ParamID::hazeRepeat, norm);
    else if (which == DragParam::hazeMod) setNorm (ParamID::hazeMod, norm);
    else if (which == DragParam::hazeReverb) setNorm (ParamID::hazeReverb, norm);
    else if (which == DragParam::bar)
    {
        const int step =
            juce::jlimit (0, 3, juce::roundToInt (norm * 3.0f));
        setNorm (ParamID::bar, step / 3.0f);
    }
}

juce::Rectangle<float> RealtimeChordFxAudioProcessorEditor::parameterBounds (int i) const
{
    const float x = 28.0f + i * 173.0f;
    return { x, 625.0f, 148.0f, 64.0f };
}

juce::Rectangle<float> RealtimeChordFxAudioProcessorEditor::holdBounds() const
{
    return { 548.0f, 516.0f, 144.0f, 56.0f };
}

juce::Rectangle<float> RealtimeChordFxAudioProcessorEditor::hazeParameterBounds (int index) const
{
    index = juce::jlimit (0, 6, index);
    const int row = index / 2;
    const int col = index % 2;
    return {
        34.0f + col * 344.0f,
        96.0f + row * 79.0f,
        308.0f,
        62.0f
    };
}

juce::Rectangle<float> RealtimeChordFxAudioProcessorEditor::hazeToggleBounds (int index) const
{
    index = juce::jlimit (0, 11, index);
    const int row = index / 4;
    const int col = index % 4;
    return {
        30.0f + col * 171.0f,
        424.0f + row * 72.0f,
        150.0f,
        52.0f
    };
}

juce::Rectangle<float> RealtimeChordFxAudioProcessorEditor::chordBotPadBounds (int index) const
{
    index = juce::jlimit (0, 8, index);
    constexpr float cell = 200.0f;
    constexpr float gap = 10.0f;
    const int row = index / 3;
    const int col = index % 3;
    return {
        50.0f + col * (cell + gap),
        80.0f + row * (cell + gap),
        cell, cell
    };
}

int RealtimeChordFxAudioProcessorEditor::chordBotPadAtPoint (
    juce::Point<float> p) const
{
    for (int i = 0; i < 9; ++i)
        if (chordBotPadBounds (i).contains (p))
            return i;
    return -1;
}

juce::String RealtimeChordFxAudioProcessorEditor::noteText (int midi) const
{
    if (midi < 0) return "--";
    static constexpr const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[midi % 12]) + juce::String (midi / 12 - 1);
}

void RealtimeChordFxAudioProcessorEditor::paintBar (juce::Graphics& g,
                                                     juce::Rectangle<float> r,
                                                     const juce::String& label,
                                                     float value,
                                                     const juce::String& text)
{
    g.setColour (juce::Colours::white.withAlpha (0.84f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (label, r.removeFromTop (20.0f), juce::Justification::centredLeft);
    auto line = r.removeFromTop (20.0f).reduced (0.0f, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.30f));
    g.fillRect (line.withHeight (1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.88f));
    g.fillRect (line.withWidth (line.getWidth() * juce::jlimit (0.0f, 1.0f, value)).withHeight (2.0f));
    g.fillEllipse (line.getX() + line.getWidth() * value - 3.0f, line.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (text, r, juce::Justification::centredLeft);
}

int RealtimeChordFxAudioProcessorEditor::nextVisualFrame (
    int count, int avoid)
{
    if (count <= 1)
        return 0;

    visualRandomState ^= visualRandomState << 13;
    visualRandomState ^= visualRandomState >> 17;
    visualRandomState ^= visualRandomState << 5;

    int frame = (int) (visualRandomState % (uint32_t) count);
    if (frame == avoid)
        frame = (frame + 1) % count;
    return frame;
}

void RealtimeChordFxAudioProcessorEditor::timerCallback()
{
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();

    const int effectMode = juce::jlimit (0, 3, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));

    const float activity = juce::jmax (
        processor.getInputPeakRaw(),
        processor.getInputRmsRaw() * 2.2f);

    if (effectMode == 0 && frames.getFrameCount() > 0)
    {
        if (chordVisualCooldown > 0)
            --chordVisualCooldown;

        const bool attack =
            activity > 0.007f
            && activity > chordVisualPreviousActivity * 1.35f + 0.002f;

        if (activity > 0.010f
            && (attack || chordVisualCooldown <= 0))
        {
            const int frame =
                nextVisualFrame (frames.getFrameCount(), loadedFrame);
            currentFrame = frames.getFrame (frame);
            loadedFrame = frame;
            chordVisualCooldown = 5; // ~167 ms at 30 Hz
        }

        chordVisualPreviousActivity = activity;
    }
    else if (effectMode == 1 && frames.getFrameCount() > 0)
    {
        const int frame = processor.getVisualFrame();
        if (frame != loadedFrame)
        {
            currentFrame = frames.getFrame (frame);
            loadedFrame = frame;
        }
    }
    else if (effectMode == 2 && eurekaFrames.getFrameCount() > 0)
    {
        if (eurekaVisualCooldown > 0)
            --eurekaVisualCooldown;

        const bool attack =
            activity > 0.006f
            && activity > eurekaVisualPreviousActivity * 1.28f + 0.0015f;

        if (activity > 0.009f
            && (attack || eurekaVisualCooldown <= 0))
        {
            eurekaFrameIndex =
                nextVisualFrame (eurekaFrames.getFrameCount(), loadedEurekaFrame);
            eurekaVisualCooldown = 4; // ~133 ms
        }

        if (eurekaFrameIndex != loadedEurekaFrame)
        {
            currentEurekaFrame =
                eurekaFrames.getFrame (eurekaFrameIndex);
            loadedEurekaFrame = eurekaFrameIndex;
        }

        eurekaVisualPreviousActivity = activity;
    }

    repaint();
}

void RealtimeChordFxAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / design, (float) getHeight() / design));
    if (midiControlConfigVisible)
        paintMidiControlConfig (g);
    else if (configVisible)
        paintConfig (g);
    else
    {
        paintMain (g);
        paintGlobalControls (g);
    }
}

void RealtimeChordFxAudioProcessorEditor::paintGlobalControls (juce::Graphics& g)
{
    const float boostDb = juce::jlimit (
        0.0f, 14.0f,
        processor.state().getRawParameterValue (ParamID::boostDb)->load());
    const juce::Rectangle<float> boostBounds (395.0f, 16.0f, 90.0f, 38.0f);

    g.setColour (juce::Colours::black.withAlpha (0.46f));
    g.fillRoundedRectangle (boostBounds, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (boostDb > 0.01f ? 0.96f : 0.58f));
    g.drawRoundedRectangle (boostBounds, 6.0f, boostDb > 0.01f ? 1.8f : 1.0f);
    g.setFont (juce::FontOptions (10.5f).withStyle ("Bold"));
    g.drawFittedText (
        boostDb > 0.01f
            ? "BOOST +" + juce::String (boostDb, 1)
            : "BOOST OFF",
        boostBounds.toNearestInt(), juce::Justification::centred, 1);

    if (l1Latched || r1Latched)
    {
        const float wet = juce::jlimit (
            0.0f, 1.0f,
            processor.state().getRawParameterValue (ParamID::wet)->load());
        const juce::Rectangle<float> wetBox (250.0f, 76.0f, 220.0f, 52.0f);
        g.setColour (juce::Colours::black.withAlpha (0.78f));
        g.fillRoundedRectangle (wetBox, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.94f));
        g.drawRoundedRectangle (wetBox, 8.0f, 1.2f);
        g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
        g.drawText (
            "WET  " + juce::String (juce::roundToInt (wet * 100.0f)) + "%",
            wetBox, juce::Justification::centred);
    }
}

void RealtimeChordFxAudioProcessorEditor::paintMain (juce::Graphics& g)
{
    const int effectMode = juce::jlimit (0, 3, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));
    const int chordMode = juce::jlimit (0, 1, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::chordMode)->load()));

    if (effectMode == 3)
    {
        paintChordBot (g);
        return;
    }

    if (effectMode == 2)
    {
        paintHaze (g);
        return;
    }

    if (effectMode == 0)
    {
        if (currentFrame.isValid())
            g.drawImage (currentFrame, juce::Rectangle<float> (0, 0, design, design), juce::RectanglePlacement::stretchToFit);

        // Exact CHORD visual restored from golden/chordfx-android-2026-10-02.
        juce::ColourGradient shade (juce::Colours::transparentBlack, 360.0f, 520.0f,
                                    juce::Colours::black.withAlpha (0.70f), 360.0f, 720.0f, false);
        g.setGradientFill (shade);
        g.fillRect (0.0f, 500.0f, 720.0f, 220.0f);

        if (processor.isRunning())
        {
            g.setColour (juce::Colour (0xfff23a36));
            g.drawRect (juce::Rectangle<float> (8.0f, 8.0f, 704.0f, 704.0f), 3.0f);
            g.setFont (juce::FontOptions (17.0f).withStyle ("Bold"));
            g.drawText (juce::String::fromUTF8 (u8"● REC"), 24, 20, 130, 28, juce::Justification::centredLeft);
        }

        g.setColour (juce::Colours::white.withAlpha (0.88f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("IN  " + noteText (processor.getDetectedMidi()) + "    CHORD  " + processor.getChordLabel(),
                    28, 578, 520, 28, juce::Justification::centredLeft);
        g.drawText ("CONFIG", 604, 22, 88, 24, juce::Justification::centredRight);

        if (chordMode == 0)
        {
            const float hold = juce::jlimit (0.0f, 1.0f,
                processor.state().getRawParameterValue (ParamID::hold)->load());
            const int captureMs = juce::roundToInt (240.0f + hold * 1760.0f);
            const int crossfadeMs = juce::roundToInt (20.0f + hold * 140.0f);
            paintBar (g, holdBounds(), "HOLD", hold,
                      juce::String (captureMs) + "/" + juce::String (crossfadeMs) + "ms");
        }
        else
        {
            const float effect = juce::jlimit (0.0f, 1.0f,
                processor.state().getRawParameterValue (ParamID::effect)->load());
            paintBar (g, holdBounds(), "EFFECT", effect,
                      juce::String (juce::roundToInt (effect * 100.0f)));
        }

        const float complex = processor.state().getRawParameterValue (ParamID::complex)->load();
        const int bar = juce::jlimit (0, 3, juce::roundToInt (processor.state().getRawParameterValue (ParamID::bar)->load()));
        const float width = processor.state().getRawParameterValue (ParamID::width)->load();
        const float length = processor.state().getRawParameterValue (ParamID::length)->load();
        static constexpr const char* bars[] { "1/4", "1/2", "1 BAR", "2 BAR" };

        paintBar (g, parameterBounds (0), "COMPLEX", complex, juce::String (juce::roundToInt (complex * 100.0f)));
        paintBar (g, parameterBounds (1), "BAR", bar / 3.0f, bars[bar]);
        paintBar (g, parameterBounds (2), "WIDTH", width, juce::String (juce::roundToInt (width * 100.0f)));
        paintBar (g, parameterBounds (3), "LENGTH", length,
                  length >= 0.995f ? "INF" : juce::String (juce::roundToInt (length * 100.0f)));
        return;
    }

    if (currentFrame.isValid())
            g.drawImage (currentFrame, juce::Rectangle<float> (0, 0, design, design),
                         juce::RectanglePlacement::stretchToFit);
    
        juce::ColourGradient shade (juce::Colours::transparentBlack, 360.0f, 555.0f,
                                    juce::Colours::black.withAlpha (0.58f),
                                    360.0f, 720.0f, false);
        g.setGradientFill (shade);
        g.fillRect (0.0f, 535.0f, 720.0f, 185.0f);
    
        if (processor.isRunning())
        {
            g.setColour (juce::Colour (0xfff23a36));
            g.drawRect (juce::Rectangle<float> (8.0f, 8.0f, 704.0f, 704.0f), 3.0f);
            g.setFont (juce::FontOptions (17.0f).withStyle ("Bold"));
            g.drawText (juce::String::fromUTF8 (u8"● REC"),
                        24, 20, 130, 28, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.74f));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("REC", 24, 20, 80, 28, juce::Justification::centredLeft);
        }
    
        g.setColour (juce::Colours::white.withAlpha (0.88f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("CONFIG", 604, 22, 88, 24, juce::Justification::centredRight);
    
        const float dreamyX = juce::jlimit (0.0f, 1.0f, processor.getMidiControllerX() / 127.0f);
        const float dreamyY = juce::jlimit (0.0f, 1.0f, processor.getMidiControllerY() / 127.0f);
        const float reverbAmount = std::pow (dreamyX * dreamyY, 1.35f);
        const int reverbSteps = juce::jlimit (0, 10, juce::roundToInt (reverbAmount * 10.0f));

        g.setFont (juce::FontOptions (11.5f));
        g.setColour (juce::Colours::white.withAlpha (0.74f));
        g.drawText ("REVERB", 28, 626, 90, 18, juce::Justification::centredLeft);
        for (int i = 0; i < 10; ++i)
        {
            const auto r = juce::Rectangle<float> (102.0f + i * 14.0f, 631.0f, 9.0f, 7.0f);
            if (i < reverbSteps)
                g.fillRect (r);
            else
                g.drawRect (r, 1.0f);
        }
        g.drawText (juce::String (juce::roundToInt (reverbAmount * 100.0f)),
                    248, 626, 52, 18, juce::Justification::centredRight);

        g.setFont (juce::FontOptions (12.0f));
        g.setColour (juce::Colours::white.withAlpha (0.74f));
        g.drawText ("X " + juce::String (processor.getMidiControllerX()).paddedLeft ('0', 3)
                    + "   Y " + juce::String (processor.getMidiControllerY()).paddedLeft ('0', 3),
                    28, 660, 240, 22, juce::Justification::centredLeft);
    
        if (processor.isRunning())
            g.drawText ("DREAMY",
                        286, 660, 120, 22, juce::Justification::centredLeft);
    
        const int motion = processor.getMotionState();
        if (motion != 0)
        {
            g.setColour (motion == 1 ? juce::Colour (0xfff23a36)
                                     : juce::Colours::white.withAlpha (0.82f));
            g.drawText (motion == 1 ? "MOTION REC" : "MOTION PLAY",
                        430, 660, 170, 22, juce::Justification::centredLeft);
        }
}


void RealtimeChordFxAudioProcessorEditor::paintHaze (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (currentEurekaFrame.isValid())
    {
        g.drawImage (
            currentEurekaFrame,
            juce::Rectangle<float> (0.0f, 0.0f, design, design),
            juce::RectanglePlacement::stretchToFit);
    }

    juce::ColourGradient topShade (
        juce::Colours::black.withAlpha (0.58f), 360.0f, 0.0f,
        juce::Colours::transparentBlack, 360.0f, 135.0f, false);
    g.setGradientFill (topShade);
    g.fillRect (0.0f, 0.0f, 720.0f, 145.0f);

    g.setColour (juce::Colours::white.withAlpha (0.96f));
    g.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
    g.drawText ("EUREKA", 28, 18, 180, 34,
                juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.72f));
    g.drawText ("DUAL LOOP / GLITCH / LO-FI",
                176, 24, 210, 24, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.94f));
    g.drawText (eurekaPanelVisible ? "CLOSE" : "PANEL",
                490, 22, 78, 24, juce::Justification::centred);
    g.drawText ("CONFIG", 604, 22, 88, 24,
                juce::Justification::centredRight);

    if (! eurekaPanelVisible)
    {
        const float repeat = juce::jlimit (
            0.0f, 1.0f,
            processor.state().getRawParameterValue (ParamID::hazeRepeat)->load());
        const float hall = juce::jlimit (
            0.0f, 1.0f,
            processor.state().getRawParameterValue (ParamID::hazeReverb)->load());

        g.setFont (juce::FontOptions (11.0f));
        g.setColour (juce::Colours::white.withAlpha (0.74f));
        g.drawText (
            "XY STUTTER  " + juce::String (juce::roundToInt (repeat * 100.0f))
            + "%   ·   UP-RIGHT REVERB  "
            + juce::String (juce::roundToInt (hall * 100.0f)) + "%",
            30, 674, 500, 20, juce::Justification::centredLeft);
        return;
    }

    g.setColour (juce::Colours::black.withAlpha (0.76f));
    g.fillRoundedRectangle (
        juce::Rectangle<float> (18.0f, 72.0f, 684.0f, 612.0f), 12.0f);
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawRoundedRectangle (
        juce::Rectangle<float> (18.0f, 72.0f, 684.0f, 612.0f), 12.0f, 1.0f);

    const float mix =
        processor.state().getRawParameterValue (ParamID::hazeMix)->load();
    const float time =
        processor.state().getRawParameterValue (ParamID::hazeTime)->load();
    const float haze =
        processor.state().getRawParameterValue (ParamID::hazeAmount)->load();
    const float filter =
        processor.state().getRawParameterValue (ParamID::hazeFilter)->load();
    const float repeat =
        processor.state().getRawParameterValue (ParamID::hazeRepeat)->load();
    const float mod =
        processor.state().getRawParameterValue (ParamID::hazeMod)->load();
    const float hall =
        processor.state().getRawParameterValue (ParamID::hazeReverb)->load();

    const float loopA = 10.0f * time;
    const float loopB = 10.0f * (1.0f - time);

    paintBar (g, hazeParameterBounds (0), "MIX", mix,
              juce::String (juce::roundToInt (mix * 100.0f)) + "%");
    paintBar (g, hazeParameterBounds (1), "TIME", time,
              juce::String (loopA, 1) + "s / "
              + juce::String (loopB, 1) + "s");
    paintBar (g, hazeParameterBounds (2), "HAZE", haze,
              haze < 0.485f
                  ? "JUMP " + juce::String (
                        juce::roundToInt ((0.5f - haze) * 200.0f))
                  : haze > 0.515f
                      ? "LO-FI " + juce::String (
                            juce::roundToInt ((haze - 0.5f) * 200.0f))
                      : "NEUTRAL");
    paintBar (g, hazeParameterBounds (3), "FILTER", filter,
              filter < 0.485f
                  ? "LOWPASS"
                  : filter > 0.515f ? "BANDPASS" : "NEUTRAL");
    paintBar (g, hazeParameterBounds (4), "REPEAT", repeat,
              juce::String (juce::roundToInt (repeat * 100.0f)) + "%");
    paintBar (g, hazeParameterBounds (5), "MOD", mod,
              mod < 0.485f
                  ? "TIME / REPEAT"
                  : mod > 0.515f ? "HAZE" : "NEUTRAL");
    paintBar (g, hazeParameterBounds (6), "REVERB / HALL", hall,
              juce::String (juce::roundToInt (hall * 100.0f)) + "%");

    static constexpr const char* speedText[] = { ".5x", "1x", "2x" };
    static constexpr const char* loopText[] = { "1", "2", "2+" };
    static constexpr const char* warbleText[] = { "OFF", "LIGHT", "HEAVY" };
    static constexpr const char* toggleNames[] = {
        "SPEED", "LOOPS", "WARBLE", "PATH",
        "TRANSPOSE", "ECHO", "OG", "LOCK",
        "BYPASS", "GAIN", "CLEAR", "SHUFFLE"
    };

    const int speed = juce::jlimit (0, 2, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::hazeSpeed)->load()));
    const int loops = juce::jlimit (0, 2, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::hazeLoops)->load()));
    const int warble = juce::jlimit (0, 2, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::hazeWarble)->load()));
    const bool transpose =
        processor.state().getRawParameterValue (
            ParamID::hazeTranspose)->load() >= 0.5f;
    const bool echo =
        processor.state().getRawParameterValue (
            ParamID::hazeEcho)->load() >= 0.5f;
    const bool og =
        processor.state().getRawParameterValue (
            ParamID::hazeOg)->load() >= 0.5f;
    const bool lock =
        processor.state().getRawParameterValue (
            ParamID::hazeLock)->load() >= 0.5f;
    const bool bypass =
        processor.state().getRawParameterValue (
            ParamID::hazeBypass)->load() >= 0.5f;
    const bool gain =
        processor.state().getRawParameterValue (
            ParamID::hazeGain)->load() >= 0.5f;
    const bool stereo =
        processor.state().getRawParameterValue (
            ParamID::hazePath)->load() >= 0.5f;

    const juce::String values[] = {
        speedText[speed],
        loopText[loops],
        warbleText[warble],
        stereo ? "STEREO" : "MONO",
        transpose ? "0.75x" : "OFF",
        echo ? "50/50" : "OVERWRITE",
        og ? "ON" : "OFF",
        lock ? "LOCKED" : "REC",
        bypass ? "DRY" : "ACTIVE",
        gain ? "+12 dB" : "UNITY",
        "ERASE",
        "REROLL"
    };

    for (int i = 0; i < 12; ++i)
    {
        auto r = hazeToggleBounds (i);
        const bool emphasized =
            (i == 6 && og) || (i == 7 && lock) || (i == 8 && bypass);

        g.setColour (juce::Colours::white.withAlpha (
            emphasized ? 0.18f : 0.06f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (
            emphasized ? 0.94f : 0.46f));
        g.drawRoundedRectangle (
            r, 6.0f, emphasized ? 2.0f : 1.0f);

        g.setFont (juce::FontOptions (10.0f));
        g.setColour (juce::Colours::white.withAlpha (0.52f));
        g.drawText (
            toggleNames[i], r.removeFromTop (19.0f),
            juce::Justification::centred);
        g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        g.setColour (juce::Colours::white.withAlpha (0.90f));
        g.drawText (values[i], r, juce::Justification::centred);
    }

    g.setFont (juce::FontOptions (10.5f));
    g.setColour (juce::Colours::white.withAlpha (0.58f));
    g.drawText (
        "BYPASS keeps recording. LOCK stops record heads. XY: STUTTER / UP-RIGHT REVERB.",
        30, 658, 660, 20, juce::Justification::centred);
}

void RealtimeChordFxAudioProcessorEditor::paintChordBot (juce::Graphics& g)
{
    static constexpr const char* roots[] =
        { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    static constexpr const char* qualities[] =
        { "MAJ","MIN","7","MAJ7","MIN7","DIM","SUS2","SUS4" };

    g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (juce::FontOptions (19.0f).withStyle ("Bold"));
    g.drawText ("CHORDBOT", 28, 20, 220, 32,
                juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (juce::Colours::white.withAlpha (0.60f));
    g.drawText ("TOP-LEFT = ROOT",
                250, 24, 140, 24, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (
        chordBotEditMode ? 0.98f : 0.74f));
    g.drawText (chordBotEditMode ? "DONE" : "EDIT",
                500, 22, 72, 26, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.84f));
    g.drawText ("CONFIG",
                604, 22, 88, 26, juce::Justification::centredRight);

    const int active = processor.getChordBotActivePad();

    for (int i = 0; i < 9; ++i)
    {
        const auto r = chordBotPadBounds (i);
        const bool isActive = i == active;
        const bool isEditTarget =
            chordBotEditMode && i == chordBotEditSlot;

        if (isActive)
        {
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.fillRoundedRectangle (r, 8.0f);
        }

        g.setColour (juce::Colours::white.withAlpha (
            isEditTarget ? 0.98f : 0.48f));
        g.drawRoundedRectangle (
            r.reduced (1.0f), 8.0f,
            isEditTarget || isActive ? 3.0f : 1.2f);

        if (i == 0)
        {
            g.setFont (juce::FontOptions (10.5f).withStyle ("Bold"));
            g.setColour (juce::Colours::white.withAlpha (0.58f));
            g.drawText ("ROOT",
                        r.getX() + 12.0f, r.getY() + 10.0f,
                        70.0f, 20.0f, juce::Justification::centredLeft);
        }

        const int root = juce::jlimit (
            0, 11, processor.getChordBotSlotRoot (i));
        const int quality = juce::jlimit (
            0, 7, processor.getChordBotSlotQuality (i));

        g.setColour (juce::Colours::white.withAlpha (0.94f));
        g.setFont (juce::FontOptions (36.0f).withStyle ("Bold"));
        g.drawText (roots[root],
                    r.getX() + 12.0f, r.getY() + 64.0f,
                    r.getWidth() - 24.0f, 52.0f,
                    juce::Justification::centred);

        g.setFont (juce::FontOptions (16.0f));
        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.drawText (qualities[quality],
                    r.getX() + 12.0f, r.getY() + 119.0f,
                    r.getWidth() - 24.0f, 30.0f,
                    juce::Justification::centred);
    }

    if (chordBotEditMode && chordBotEditSlot >= 0)
    {
        const juce::Rectangle<float> overlay (105.0f, 180.0f, 510.0f, 360.0f);
        g.setColour (juce::Colours::black.withAlpha (0.96f));
        g.fillRoundedRectangle (overlay, 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.76f));
        g.drawRoundedRectangle (overlay, 12.0f, 1.5f);

        g.setColour (juce::Colours::white.withAlpha (0.94f));
        g.setFont (juce::FontOptions (17.0f).withStyle ("Bold"));
        g.drawText ("EDIT PAD " + juce::String (chordBotEditSlot + 1),
                    140, 202, 440, 30, juce::Justification::centred);

        g.setFont (juce::FontOptions (13.0f));
        g.setColour (juce::Colours::white.withAlpha (0.62f));
        g.drawText ("ROOT", 145, 248, 100, 34,
                    juce::Justification::centredLeft);
        g.drawText ("CHORD", 145, 326, 100, 34,
                    juce::Justification::centredLeft);

        g.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.drawText ("<", 150, 282, 58, 44, juce::Justification::centred);
        g.drawText (roots[juce::jlimit (0, 11, chordBotEditRoot)],
                    225, 282, 270, 44, juce::Justification::centred);
        g.drawText (">", 512, 282, 58, 44, juce::Justification::centred);

        g.drawText ("<", 150, 360, 58, 44, juce::Justification::centred);
        g.drawText (qualities[juce::jlimit (0, 7, chordBotEditQuality)],
                    225, 360, 270, 44, juce::Justification::centred);
        g.drawText (">", 512, 360, 58, 44, juce::Justification::centred);

        g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
        g.drawText ("SAVE", 180, 454, 160, 48, juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.66f));
        g.drawText ("CANCEL", 380, 454, 160, 48, juce::Justification::centred);

        if (chordBotEditSlot == 0)
        {
            g.setFont (juce::FontOptions (10.5f));
            g.setColour (juce::Colours::white.withAlpha (0.50f));
            g.drawText ("Saving ROOT re-generates the other 8 theory candidates.",
                        135, 510, 450, 18, juce::Justification::centred);
        }
    }
}

juce::String RealtimeChordFxAudioProcessorEditor::currentInputName() const
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        if (auto* dev = holder->deviceManager.getCurrentAudioDevice()) return dev->getName();
    return "Unavailable";
}

void RealtimeChordFxAudioProcessorEditor::refreshAudioInputs()
{
    audioInputs.clear();
    physicalInputNames = getAndroidPhysicalInputNames();
    selectedAudioInput = -1;
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr) return;
    auto& dm = holder->deviceManager;
    const auto currentType = dm.getCurrentAudioDeviceType();
    juce::AudioDeviceManager::AudioDeviceSetup setup; dm.getAudioDeviceSetup (setup);

    for (auto* type : dm.getAvailableDeviceTypes())
    {
        if (type == nullptr) continue;
        type->scanForDevices();
        const auto names = type->getDeviceNames (true);
        for (const auto& name : names)
        {
            // RG Rotate exposes several internal/built-in routes that are not useful
            // as selectable sources for this app. Filter them before the six-row UI limit
            // so an external USB input can occupy the visible list when JUCE exposes it.
            if (name.containsIgnoreCase ("RG Rotate"))
                continue;

            const int idx = (int) audioInputs.size();
            audioInputs.push_back ({ type->getTypeName(), name });
            if (type->getTypeName() == currentType &&
                (name == setup.inputDeviceName || name == currentInputName())) selectedAudioInput = idx;
        }
    }
}

void RealtimeChordFxAudioProcessorEditor::selectAudioInput (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) audioInputs.size())) return;
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr) return;
    auto& dm = holder->deviceManager;
    const auto option = audioInputs[(size_t) index];
    dm.setCurrentAudioDeviceType (option.type, false);
    juce::AudioDeviceManager::AudioDeviceSetup setup; dm.getAudioDeviceSetup (setup);
    setup.inputDeviceName = option.name;
    setup.useDefaultInputChannels = true;
    const auto error = dm.setAudioDeviceSetup (setup, true);
    lastAudioRouteError = error;
    if (error.isEmpty())
    {
        holder->getMuteInputValue().setValue (false);
        selectedAudioInput = index;
    }
    refreshAudioInputs();
}

void RealtimeChordFxAudioProcessorEditor::paintConfig (juce::Graphics& g)
{
    if (currentFrame.isValid())
        g.drawImage (currentFrame, juce::Rectangle<float> (0,0,design,design), juce::RectanglePlacement::stretchToFit);
    g.setColour (juce::Colours::black.withAlpha (0.76f));
    g.fillAll();

    g.setColour (juce::Colours::white.withAlpha (0.90f));
    g.setFont (juce::FontOptions (20.0f));
    g.drawText ("CONFIG", 34, 26, 180, 32, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("SAVE", 360, 28, 80, 26, juce::Justification::centred);
    g.drawText ("LOAD", 452, 28, 80, 26, juce::Justification::centred);
    g.drawText ("CLOSE", 620, 30, 70, 24, juce::Justification::centredRight);

    if (globalPresetMessage.isNotEmpty())
    {
        g.setFont (juce::FontOptions (10.5f));
        g.setColour (juce::Colours::white.withAlpha (0.56f));
        g.drawText (globalPresetMessage, 330, 58, 250, 20,
                    juce::Justification::centred);
    }

    g.setFont (juce::FontOptions (13.0f));
    g.setColour (juce::Colours::white.withAlpha (0.62f));
    g.drawText ("AUDIO INPUT", 54, 94, 180, 24, juce::Justification::centredLeft);

    int y = 126;
    const int maxRows = juce::jmin (6, (int) audioInputs.size());
    for (int i = 0; i < maxRows; ++i)
    {
        const bool selected = i == selectedAudioInput;
        g.setColour (juce::Colours::white.withAlpha (selected ? 0.92f : 0.68f));
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ((selected ? "●  " : "○  ") + audioInputs[(size_t)i].name,
                    64, y, 520, 34, juce::Justification::centredLeft);
        y += 38;
    }
    if (audioInputs.empty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawText ("No selectable input reported by Android/JUCE", 64, y, 520, 34, juce::Justification::centredLeft);
    }

    bool irigDetected = false;
    juce::String irigPhysicalName;
    for (const auto& o : audioInputs)
        if (o.name.containsIgnoreCase ("irig")) { irigDetected = true; irigPhysicalName = o.name; }
    for (const auto& name : physicalInputNames)
        if (name.containsIgnoreCase ("irig")) { irigDetected = true; irigPhysicalName = name; }

    g.setColour (irigDetected ? juce::Colour (0xffb8d9bf) : juce::Colours::white.withAlpha (0.55f));
    g.drawText ("iRig Streamer  " + juce::String (irigDetected ? "DETECTED" : "NOT DETECTED"),
                54, 350, 360, 26, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.48f));
    g.setFont (juce::FontOptions (11.5f));
    g.drawFittedText (irigPhysicalName.isNotEmpty() ? irigPhysicalName
                                                    : "Android physical-input name unavailable / no iRig found",
                      54, 376, 520, 25, juce::Justification::centredLeft, 1);

    const float displayLevel = processor.getInputLevel();
    const float rawPeak = processor.getInputPeakRaw();
    const float rawRms = processor.getInputRmsRaw();
    const float nonZero = processor.getInputNonZeroRatio();
    const float level = juce::jlimit (0.0f, 1.0f, displayLevel * 3.0f);
    const float peakDb = rawPeak > 1.0e-8f ? juce::Decibels::gainToDecibels (rawPeak) : -160.0f;
    const float rmsDb = rawRms > 1.0e-8f ? juce::Decibels::gainToDecibels (rawRms) : -160.0f;
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillRect (54.0f, 423.0f, 420.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.86f));
    g.fillRect (54.0f, 422.0f, 420.0f * level, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.58f));
    g.drawText ("PEAK " + juce::String (peakDb, 1) + " dBFS   RMS " + juce::String (rmsDb, 1) + " dBFS",
                478, 410, 210, 26, juce::Justification::centredRight);

    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        juce::AudioDeviceManager::AudioDeviceSetup route;
        holder->deviceManager.getAudioDeviceSetup (route);
        auto* dev = holder->deviceManager.getCurrentAudioDevice();
        const int inputChans = dev != nullptr ? dev->getActiveInputChannels().countNumberOfSetBits() : 0;
        const double sr = dev != nullptr ? dev->getCurrentSampleRate() : 0.0;
        const bool irigRoute = route.inputDeviceName.containsIgnoreCase ("irig");
        const bool routeOpen = irigRoute && inputChans > 0;
        const bool signalNearZero = peakDb < -100.0f;

        g.setFont (juce::FontOptions (10.5f));
        g.setColour (juce::Colours::white.withAlpha (0.46f));
        g.drawFittedText ("ROUTE IN: " + (route.inputDeviceName.isNotEmpty() ? route.inputDeviceName : juce::String ("NONE")),
                          54, 438, 600, 18, juce::Justification::centredLeft, 1);
        g.drawFittedText ("ROUTE OUT: " + (route.outputDeviceName.isNotEmpty() ? route.outputDeviceName : juce::String ("NONE")),
                          54, 456, 600, 18, juce::Justification::centredLeft, 1);
        g.drawText ("IN CH " + juce::String (inputChans)
                    + "    SR " + juce::String ((int) sr)
                    + "    NONZERO " + juce::String (nonZero * 100.0f, 1) + "%",
                    54, 474, 610, 18, juce::Justification::centredLeft);
        if (irigRoute)
        {
            g.setColour (routeOpen && ! signalNearZero
                         ? juce::Colour (0xffb8d9bf)
                         : juce::Colour (0xffe0b39b));
            g.drawText (routeOpen
                        ? (signalNearZero ? "iRig ROUTE OPEN / SIGNAL < -100 dBFS"
                                          : "iRig ROUTE OPEN / SIGNAL ACTIVE")
                        : "iRig ROUTE NOT OPEN",
                        54, 492, 430, 18, juce::Justification::centredLeft);
        }
        if (lastAudioRouteError.isNotEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.46f));
            g.drawFittedText ("ERR " + lastAudioRouteError,
                              430, 492, 240, 18, juce::Justification::centredRight, 1);
        }
    }

    const int effectMode = juce::jlimit (0, 3, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));
    const int chordMode = juce::jlimit (0, 1, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::chordMode)->load()));
    const bool midiControl = processor.state().getRawParameterValue (ParamID::midiControl)->load() >= 0.5f;
    const int clock = juce::roundToInt (processor.state().getRawParameterValue (ParamID::clockMode)->load());
    const int bpm = juce::roundToInt (processor.state().getRawParameterValue (ParamID::internalBpm)->load());

    g.setColour (juce::Colours::white.withAlpha (0.78f));
    g.setFont (juce::FontOptions (16.0f));
    g.drawText ("MODE", 54, 510, 220, 30, juce::Justification::centredLeft);
    const juce::String modeText =
        effectMode == 3 ? "CHORDBOT"
        : effectMode == 2 ? "EUREKA"
        : effectMode == 1 ? "DREAMY"
                          : (chordMode == 0 ? "CHORD-A" : "CHORD-B");
    g.drawText (modeText,
                450, 510, 180, 30, juce::Justification::centredRight);
    g.drawText ("MIDI CONTROL", 54, 542, 220, 30, juce::Justification::centredLeft);
    g.drawText (midiControl ? "ON" : "OFF", 450, 542, 180, 30, juce::Justification::centredRight);
    g.drawText ("MIDI SETTINGS", 54, 574, 220, 30, juce::Justification::centredLeft);
    g.drawText (">", 450, 574, 180, 30, juce::Justification::centredRight);
    g.drawText ("CLOCK", 54, 606, 180, 30, juce::Justification::centredLeft);
    g.drawText (clock == 0 ? "Internal" : "MIDI", 450, 606, 180, 30, juce::Justification::centredRight);
    if (clock == 0)
    {
        g.drawText ("BPM", 54, 638, 180, 30, juce::Justification::centredLeft);
        g.drawText (juce::String (bpm), 450, 638, 180, 30, juce::Justification::centredRight);
    }
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colours::white.withAlpha (0.42f));
    g.drawText ("MODE: CHORD-A / CHORD-B / DREAMY / EUREKA / CHORDBOT", 54, 681, 590, 18, juce::Justification::centredLeft);
}

void RealtimeChordFxAudioProcessorEditor::paintMidiControlConfig (juce::Graphics& g)
{
    if (currentFrame.isValid())
        g.drawImage (currentFrame, juce::Rectangle<float> (0,0,design,design), juce::RectanglePlacement::stretchToFit);
    g.setColour (juce::Colours::black.withAlpha (0.80f));
    g.fillAll();

    static constexpr const char* modes[] = { "CC", "NOTE", "CLOCK" };
    static constexpr const char* keys[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    static constexpr const char* scales[] = { "Chromatic","Major","Natural Minor","Major Pent","Minor Pent" };

    const bool enabled = processor.state().getRawParameterValue (ParamID::midiControl)->load() >= 0.5f;
    const int ch = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiChannel)->load());
    const bool chordOut = processor.state().getRawParameterValue (ParamID::chordMidiOut)->load() >= 0.5f;
    const int chordCh = juce::roundToInt (processor.state().getRawParameterValue (ParamID::chordMidiChannel)->load());
    const int xm = juce::jlimit (0, 2, juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiXMode)->load()));
    const int ym = juce::jlimit (0, 2, juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiYMode)->load()));
    const int xcc = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiXCC)->load());
    const int ycc = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiYCC)->load());
    const int key = juce::jlimit (0, 11, juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiKey)->load()));
    const int scale = juce::jlimit (0, 4, juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiScale)->load()));
    const int motionBars = juce::jlimit (1, 16, juce::roundToInt (processor.state().getRawParameterValue (ParamID::motionBars)->load()));

    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (juce::FontOptions (20.0f));
    g.drawText ("MIDI CONTROL", 34, 26, 240, 32, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("BACK", 620, 30, 70, 24, juce::Justification::centredRight);

    auto row = [&] (int yy, const juce::String& label, const juce::String& value)
    {
        g.setFont (juce::FontOptions (14.0f));
        g.setColour (juce::Colours::white.withAlpha (0.70f));
        g.drawText (label, 54, yy, 250, 30, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.drawText (value, 330, yy, 300, 30, juce::Justification::centredRight);
    };

    juce::String midiOut = "NONE";
    if (juce::isPositiveAndBelow (selectedMidiOutput, midiOutputs.size()))
        midiOut = midiOutputs.getReference (selectedMidiOutput).name;

    row (74,  "ENABLE", enabled ? "ON" : "OFF");
    row (106, "MIDI OUT", midiOut);
    row (138, "MIDI CH", juce::String (ch));
    row (170, "CHORD OUT", chordOut ? "ON" : "OFF");
    row (202, "CHORD CH", juce::String (chordCh));
    row (234, "X MODE", modes[xm]);
    row (266, "X CC", xm == 0 ? juce::String (xcc) : "--");
    row (298, "Y MODE", modes[ym]);
    row (330, "Y CC", ym == 0 ? juce::String (ycc) : "--");
    row (362, "KEY", keys[key]);
    row (394, "SCALE", scales[scale]);
    row (426, "PRESET", juce::String (midiPresetSlot)
                           + (processor.hasMidiControllerPreset (midiPresetSlot) ? "  SAVED" : "  EMPTY"));
    row (458, "MOTION BARS", juce::String (motionBars));

    g.setColour (juce::Colours::white.withAlpha (0.82f));
    g.drawText ("LOAD", 110, 496, 180, 36, juce::Justification::centred);
    g.drawText ("SAVE", 430, 496, 180, 36, juce::Justification::centred);

    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("X " + juce::String (processor.getMidiControllerX()).paddedLeft ('0', 3)
                + "     Y " + juce::String (processor.getMidiControllerY()).paddedLeft ('0', 3),
                180, 538, 360, 24, juce::Justification::centred);
    g.setFont (juce::FontOptions (11.5f));
    g.setColour (juce::Colours::white.withAlpha (0.52f));
    g.drawText ("CHORD OUT sends the generated ChordPlan notes to the selected MIDI OUT.", 54, 570, 620, 20, juce::Justification::centredLeft);
    g.drawText ("L1: WET -5%    R1: WET +5%    96 motion ticks / bar", 54, 592, 610, 20, juce::Justification::centredLeft);
    g.drawText ("X 0..127 / Y 0..127. CLOCK uses 24 PPQN.", 54, 614, 610, 20, juce::Justification::centredLeft);
    if (midiPresetMessage.isNotEmpty())
        g.drawText (midiPresetMessage, 54, 650, 560, 22, juce::Justification::centredLeft);
}

void RealtimeChordFxAudioProcessorEditor::refreshMidiOutputs()
{
    midiOutputs = juce::MidiOutput::getAvailableDevices();
    selectedMidiOutput = -1;
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        const auto current = holder->deviceManager.getDefaultMidiOutputIdentifier();
        for (int i = 0; i < midiOutputs.size(); ++i)
            if (midiOutputs.getReference (i).identifier == current) { selectedMidiOutput = i; break; }
    }
}

void RealtimeChordFxAudioProcessorEditor::cycleMidiOutput (int direction)
{
    refreshMidiOutputs();
    const int count = midiOutputs.size();
    int logical = selectedMidiOutput + 1;
    logical = (logical + (direction >= 0 ? 1 : count) + count + 1) % (count + 1);
    selectedMidiOutput = logical - 1;
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        const juce::String id = selectedMidiOutput >= 0 ? midiOutputs.getReference (selectedMidiOutput).identifier : juce::String();
        holder->deviceManager.setDefaultMidiOutputDevice (id);
        holder->player.setMidiOutput (holder->deviceManager.getDefaultMidiOutput());
    }
}

void RealtimeChordFxAudioProcessorEditor::setChoiceActual (const char* id, int value)
{
    if (auto* p = processor.state().getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
    processor.notifyMidiControllerConfigChanged();
}

void RealtimeChordFxAudioProcessorEditor::updateMidiControllerFromPoint (juce::Point<float> p)
{
    const int x = juce::jlimit (0, 127, juce::roundToInt (127.0f * p.x / design));
    const int y = juce::jlimit (0, 127, juce::roundToInt (127.0f * (design - p.y) / design));
    processor.setMidiControllerXY (x, y, true);
}

void RealtimeChordFxAudioProcessorEditor::updateEurekaMotionPoint (
    juce::Point<float> p)
{
    eurekaMotionTouchX =
        juce::jlimit (0.0f, 1.0f, p.x / design);
    eurekaMotionTouchY =
        juce::jlimit (0.0f, 1.0f, p.y / design);

    if (auto* mix = processor.state().getParameter (ParamID::hazeMix))
        mix->setValueNotifyingHost (eurekaMotionTouchX);

    if (auto* haze = processor.state().getParameter (ParamID::hazeAmount))
        haze->setValueNotifyingHost (1.0f - eurekaMotionTouchY);
}

void RealtimeChordFxAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = toDesign (e.position);
    dragging = DragParam::none;
    if (midiControlConfigVisible)
    {
        if (p.x >= 590 && p.y <= 66) { midiControlConfigVisible = false; repaint(); return; }
        const int dir = p.x >= 360 ? 1 : -1;

        if (p.y >= 72 && p.y < 104)
        {
            auto* par = processor.state().getParameter (ParamID::midiControl);
            par->setValueNotifyingHost (par->getValue() < 0.5f ? 1.0f : 0.0f);
            processor.notifyMidiControllerConfigChanged();
        }
        else if (p.y >= 104 && p.y < 136)
        {
            cycleMidiOutput (dir);
        }
        else if (p.y >= 136 && p.y < 168)
        {
            const int v = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiChannel)->load());
            setChoiceActual (ParamID::midiChannel, juce::jlimit (1, 16, v + dir));
        }
        else if (p.y >= 168 && p.y < 200)
        {
            auto* par = processor.state().getParameter (ParamID::chordMidiOut);
            par->setValueNotifyingHost (par->getValue() < 0.5f ? 1.0f : 0.0f);
        }
        else if (p.y >= 200 && p.y < 232)
        {
            const int v = juce::roundToInt (processor.state().getRawParameterValue (ParamID::chordMidiChannel)->load());
            setChoiceActual (ParamID::chordMidiChannel, juce::jlimit (1, 16, v + dir));
        }
        else if (p.y >= 232 && p.y < 264)
        {
            int v = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiXMode)->load());
            v = (v + dir + 3) % 3;
            setChoiceActual (ParamID::midiXMode, v);
            if (v == 2 && juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiYMode)->load()) == 2)
                setChoiceActual (ParamID::midiYMode, 0);
        }
        else if (p.y >= 264 && p.y < 296)
        {
            const int v = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiXCC)->load());
            setChoiceActual (ParamID::midiXCC, juce::jlimit (0, 127, v + dir));
        }
        else if (p.y >= 296 && p.y < 328)
        {
            int v = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiYMode)->load());
            v = (v + dir + 3) % 3;
            setChoiceActual (ParamID::midiYMode, v);
            if (v == 2 && juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiXMode)->load()) == 2)
                setChoiceActual (ParamID::midiXMode, 0);
        }
        else if (p.y >= 328 && p.y < 360)
        {
            const int v = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiYCC)->load());
            setChoiceActual (ParamID::midiYCC, juce::jlimit (0, 127, v + dir));
        }
        else if (p.y >= 360 && p.y < 392)
        {
            const int v = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiKey)->load());
            setChoiceActual (ParamID::midiKey, (v + dir + 12) % 12);
        }
        else if (p.y >= 392 && p.y < 424)
        {
            const int v = juce::roundToInt (processor.state().getRawParameterValue (ParamID::midiScale)->load());
            setChoiceActual (ParamID::midiScale, (v + dir + 5) % 5);
        }
        else if (p.y >= 424 && p.y < 456)
        {
            midiPresetSlot = juce::jlimit (1, 8, midiPresetSlot + dir);
            midiPresetMessage.clear();
        }
        else if (p.y >= 456 && p.y < 490)
        {
            const int v = juce::roundToInt (processor.state().getRawParameterValue (ParamID::motionBars)->load());
            setChoiceActual (ParamID::motionBars, juce::jlimit (1, 16, v + dir));
        }
        else if (p.y >= 492 && p.y < 536)
        {
            if (p.x < 360)
                midiPresetMessage = processor.loadMidiControllerPreset (midiPresetSlot)
                                  ? "LOADED PRESET " + juce::String (midiPresetSlot)
                                  : "PRESET " + juce::String (midiPresetSlot) + " IS EMPTY";
            else
                midiPresetMessage = processor.saveMidiControllerPreset (midiPresetSlot)
                                  ? "SAVED PRESET " + juce::String (midiPresetSlot)
                                  : "SAVE FAILED";
        }

        repaint();
        return;
    }

    if (configVisible)
    {
        if (p.x >= 590 && p.y <= 74) { configVisible = false; repaint(); return; }
        int y = 126;
        const int maxRows = juce::jmin (6, (int) audioInputs.size());
        for (int i = 0; i < maxRows; ++i, y += 38)
            if (p.y >= y && p.y < y + 34) { selectAudioInput (i); repaint(); return; }

        if (p.y >= 508 && p.y < 540)
        {
            const int effect = juce::jlimit (0, 3, juce::roundToInt (
                processor.state().getRawParameterValue (ParamID::effectMode)->load()));
            const int chord = juce::jlimit (0, 1, juce::roundToInt (
                processor.state().getRawParameterValue (ParamID::chordMode)->load()));

            if (effect == 0 && chord == 0)
                setChoiceActual (ParamID::chordMode, 1);       // A -> B
            else if (effect == 0)
                setChoiceActual (ParamID::effectMode, 1);      // B -> DREAMY
            else if (effect == 1)
                setChoiceActual (ParamID::effectMode, 2);      // DREAMY -> EUREKA
            else if (effect == 2)
                setChoiceActual (ParamID::effectMode, 3);      // EUREKA -> CHORDBOT
            else
            {
                setChoiceActual (ParamID::effectMode, 0);      // CHORDBOT -> A
                setChoiceActual (ParamID::chordMode, 0);
            }
        }
        else if (p.y >= 540 && p.y < 572)
        {
            auto* par = processor.state().getParameter (ParamID::midiControl);
            par->setValueNotifyingHost (par->getValue() < 0.5f ? 1.0f : 0.0f);
            processor.notifyMidiControllerConfigChanged();
        }
        else if (p.y >= 572 && p.y < 604)
        {
            midiControlConfigVisible = true;
            configVisible = false;
            refreshMidiOutputs();
        }
        else if (p.y >= 604 && p.y < 636)
        {
            auto* par = processor.state().getParameter (ParamID::clockMode);
            par->setValueNotifyingHost (par->getValue() < 0.5f ? 1.0f : 0.0f);
        }
        else if (p.y >= 636 && p.y < 670 && processor.state().getRawParameterValue (ParamID::clockMode)->load() < 0.5f)
        {
            auto* par = processor.state().getParameter (ParamID::internalBpm);
            par->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, par->getValue() + (p.x > 360 ? 1.0f/200.0f : -1.0f/200.0f)));
        }
        repaint();
        return;
    }

    if (p.x >= 575 && p.y <= 70)
    {
        if (chordBotPressedPad >= 0)
        {
            processor.triggerChordBotPad (chordBotPressedPad, false);
            chordBotPressedPad = -1;
        }
        chordBotEditSlot = -1;
        configVisible = true;
        refreshAudioInputs();
        repaint();
        return;
    }

    const int effectMode = juce::jlimit (0, 3, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));

    if (effectMode == 3)
    {
        if (p.x >= 480 && p.x < 580 && p.y <= 70)
        {
            chordBotEditMode = ! chordBotEditMode;
            chordBotEditSlot = -1;
            repaint();
            return;
        }

        if (chordBotEditMode && chordBotEditSlot >= 0)
        {
            if (juce::Rectangle<float> (150, 282, 58, 44).contains (p))
            {
                chordBotEditRoot = (chordBotEditRoot + 11) % 12;
                repaint();
                return;
            }
            if (juce::Rectangle<float> (512, 282, 58, 44).contains (p))
            {
                chordBotEditRoot = (chordBotEditRoot + 1) % 12;
                repaint();
                return;
            }
            if (juce::Rectangle<float> (150, 360, 58, 44).contains (p))
            {
                chordBotEditQuality = (chordBotEditQuality + 7) % 8;
                repaint();
                return;
            }
            if (juce::Rectangle<float> (512, 360, 58, 44).contains (p))
            {
                chordBotEditQuality = (chordBotEditQuality + 1) % 8;
                repaint();
                return;
            }
            if (juce::Rectangle<float> (180, 454, 160, 48).contains (p))
            {
                processor.setChordBotSlot (
                    chordBotEditSlot,
                    chordBotEditRoot,
                    chordBotEditQuality);
                chordBotEditSlot = -1;
                repaint();
                return;
            }
            if (juce::Rectangle<float> (380, 454, 160, 48).contains (p))
            {
                chordBotEditSlot = -1;
                repaint();
                return;
            }

            return;
        }

        const int pad = chordBotPadAtPoint (p);
        if (pad >= 0)
        {
            if (chordBotEditMode)
            {
                chordBotEditSlot = pad;
                chordBotEditRoot =
                    processor.getChordBotSlotRoot (pad);
                chordBotEditQuality =
                    processor.getChordBotSlotQuality (pad);
            }
            else
            {
                chordBotPressedPad = pad;
                processor.triggerChordBotPad (pad, true);
            }
            repaint();
        }
        return;
    }

    if (effectMode == 2)
    {
        if (p.x >= 470.0f && p.x < 575.0f && p.y <= 70.0f)
        {
            eurekaPanelVisible = ! eurekaPanelVisible;
            eurekaMotionTouchDown = false;
            repaint();
            return;
        }

        if (! eurekaPanelVisible)
        {
            if (! eurekaMotionArmed)
            {
                eurekaMotionPlaying = false;
                eurekaMotionRecording = false;
                eurekaMotionRecordIndex = 0;
                eurekaMotionPlaybackIndex = 0;
                eurekaMotionArmed = true;
                eurekaMotionTouchDown = false;
            }
            else
            {
                // The arm tap and the recording drag are intentionally
                // separate gestures. The second press may become the drag.
                eurekaMotionTouchDown = true;
                eurekaMotionTouchX =
                    juce::jlimit (0.0f, 1.0f, p.x / design);
                eurekaMotionTouchY =
                    juce::jlimit (0.0f, 1.0f, p.y / design);
            }

            repaint();
            return;
        }

        static constexpr DragParam hazeParams[] = {
            DragParam::hazeMix,
            DragParam::hazeTime,
            DragParam::hazeAmount,
            DragParam::hazeFilter,
            DragParam::hazeRepeat,
            DragParam::hazeMod,
            DragParam::hazeReverb
        };

        for (int i = 0; i < 7; ++i)
        {
            if (hazeParameterBounds (i).contains (p))
            {
                eurekaMotionArmed = false;
                eurekaMotionRecording = false;
                eurekaMotionPlaying = false;
                eurekaMotionTouchDown = false;
                dragging = hazeParams[i];
                setParameterFromX (dragging, p.x);
                repaint();
                return;
            }
        }

        auto toggleBool = [this] (const char* id)
        {
            if (auto* par = processor.state().getParameter (id))
                par->setValueNotifyingHost (
                    par->getValue() < 0.5f ? 1.0f : 0.0f);
        };

        for (int i = 0; i < 12; ++i)
        {
            if (! hazeToggleBounds (i).contains (p))
                continue;

            if (i == 0)
            {
                const int v = juce::jlimit (0, 2, juce::roundToInt (
                    processor.state().getRawParameterValue (
                        ParamID::hazeSpeed)->load()));
                setChoiceActual (ParamID::hazeSpeed, (v + 1) % 3);
            }
            else if (i == 1)
            {
                const int v = juce::jlimit (0, 2, juce::roundToInt (
                    processor.state().getRawParameterValue (
                        ParamID::hazeLoops)->load()));
                setChoiceActual (ParamID::hazeLoops, (v + 1) % 3);
            }
            else if (i == 2)
            {
                const int v = juce::jlimit (0, 2, juce::roundToInt (
                    processor.state().getRawParameterValue (
                        ParamID::hazeWarble)->load()));
                setChoiceActual (ParamID::hazeWarble, (v + 1) % 3);
            }
            else if (i == 3)
            {
                const int v = juce::jlimit (0, 1, juce::roundToInt (
                    processor.state().getRawParameterValue (
                        ParamID::hazePath)->load()));
                setChoiceActual (ParamID::hazePath, 1 - v);
            }
            else if (i == 4) toggleBool (ParamID::hazeTranspose);
            else if (i == 5) toggleBool (ParamID::hazeEcho);
            else if (i == 6) toggleBool (ParamID::hazeOg);
            else if (i == 7) toggleBool (ParamID::hazeLock);
            else if (i == 8) toggleBool (ParamID::hazeBypass);
            else if (i == 9) toggleBool (ParamID::hazeGain);
            else if (i == 10) processor.clearHazeBuffer();
            else if (i == 11) processor.shuffleHaze();

            repaint();
            return;
        }

        return;
    }

    if (effectMode == 0)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (parameterBounds (i).contains (p))
            {
                dragging = (i == 0 ? DragParam::complex
                                   : i == 1 ? DragParam::bar
                                            : i == 2 ? DragParam::width
                                                     : DragParam::length);
                setParameterFromX (dragging, p.x);
                repaint();
                return;
            }
        }

        const int chordMode = juce::jlimit (0, 1, juce::roundToInt (
            processor.state().getRawParameterValue (ParamID::chordMode)->load()));
        if (holdBounds().contains (p))
        {
            dragging = chordMode == 0 ? DragParam::hold : DragParam::effect;
            setParameterFromX (dragging, p.x);
            repaint();
            return;
        }

        const bool midiControlOn =
            processor.state().getRawParameterValue (ParamID::midiControl)->load() >= 0.5f;
        if (midiControlOn)
        {
            if (p.x <= 170 && p.y <= 70)
                processor.toggleRunState();
            else
            {
                xyDragging = true;
                updateMidiControllerFromPoint (p);
            }
        }
        else
        {
            processor.toggleRunState();
        }
    }
    else
    {
        if (p.x <= 170 && p.y <= 70)
            processor.toggleRunState();
        else
        {
            xyDragging = true;
            updateMidiControllerFromPoint (p);
        }
    }

    repaint();
}

void RealtimeChordFxAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (midiControlConfigVisible || configVisible)
        return;

    const int effectMode = juce::jlimit (0, 3, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));

    if (effectMode == 2
        && ! eurekaPanelVisible
        && eurekaMotionArmed
        && eurekaMotionTouchDown)
    {
        if (! eurekaMotionRecording)
        {
            eurekaMotionRecording = true;
            eurekaMotionPlaying = false;
            eurekaMotionArmed = false;
            eurekaMotionRecordIndex = 0;
            eurekaMotionPlaybackIndex = 0;
        }

        updateEurekaMotionPoint (toDesign (e.position));
        repaint();
        return;
    }

    if (effectMode == 2
        && ! eurekaPanelVisible
        && eurekaMotionRecording)
    {
        updateEurekaMotionPoint (toDesign (e.position));
        repaint();
        return;
    }

    if (xyDragging)
    {
        updateMidiControllerFromPoint (toDesign (e.position));
        repaint();
        return;
    }

    if (dragging != DragParam::none)
    {
        setParameterFromX (dragging, toDesign (e.position).x);
        repaint();
    }
}

void RealtimeChordFxAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    eurekaMotionTouchDown = false;

    if (chordBotPressedPad >= 0)
    {
        processor.triggerChordBotPad (chordBotPressedPad, false);
        chordBotPressedPad = -1;
    }

    if (xyDragging)
    {
        xyDragging = false;
        processor.releaseMidiControllerTouch();
    }

    if (dragging == DragParam::hold)
        processor.notifyChordAHoldChanged();

    if (dragging != DragParam::none)
        dragging = DragParam::none;

    repaint();
}

bool RealtimeChordFxAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::F17Key)
    {
        if (! l1Latched)
        {
            l1Latched = true;
            processor.toggleMotionRecord();
            repaint();
        }
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::F18Key)
    {
        if (! r1Latched)
        {
            r1Latched = true;
            processor.clearMotion();
            repaint();
        }
        return true;
    }

    return false;
}

bool RealtimeChordFxAudioProcessorEditor::keyStateChanged (bool isKeyDown)
{
    if (! isKeyDown)
    {
        l1Latched = false;
        r1Latched = false;
    }
    return false;
}
