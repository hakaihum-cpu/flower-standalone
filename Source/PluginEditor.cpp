#include "PluginEditor.h"
#include "ParameterIDs.h"
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

   #if JUCE_ANDROID
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        holder->stopPlaying();
        holder->deviceManager.closeAudioDevice();

        const auto audioError = holder->deviceManager.initialise (1, 2, nullptr, true);

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

juce::Rectangle<float> RealtimeChordFxAudioProcessorEditor::parameterBounds (int i) const
{
    const float x = 28.0f + i * 173.0f;
    return { x, 625.0f, 148.0f, 64.0f };
}

juce::String RealtimeChordFxAudioProcessorEditor::noteText (int midi) const
{
    if (midi < 0) return "--";
    static constexpr const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[midi % 12]) + juce::String (midi / 12 - 1);
}

void RealtimeChordFxAudioProcessorEditor::timerCallback()
{
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();

    const int frame = processor.getVisualFrame();
    if (frame != loadedFrame && frames.getFrameCount() > 0)
    {
        currentFrame = frames.getFrame (frame);
        loadedFrame = frame;
    }
    repaint();
}

void RealtimeChordFxAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / design, (float) getHeight() / design));
    if (midiControlConfigVisible) paintMidiControlConfig (g); else if (configVisible) paintConfig (g); else paintMain (g);
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

void RealtimeChordFxAudioProcessorEditor::paintMain (juce::Graphics& g)
{
    if (currentFrame.isValid())
        g.drawImage (currentFrame, juce::Rectangle<float> (0, 0, design, design), juce::RectanglePlacement::stretchToFit);

    // Only a soft readability strip; the supplied frames remain the visual focus.
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

    const bool midiControlOn = processor.state().getRawParameterValue (ParamID::midiControl)->load() >= 0.5f;
    if (midiControlOn)
    {
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (juce::Colours::white.withAlpha (0.74f));
        g.drawText ("X " + juce::String (processor.getMidiControllerX()).paddedLeft ('0', 3)
                    + "   Y " + juce::String (processor.getMidiControllerY()).paddedLeft ('0', 3),
                    28, 548, 240, 22, juce::Justification::centredLeft);
        if (! processor.isRunning())
            g.drawText ("REC", 24, 20, 80, 28, juce::Justification::centredLeft);

        const int motion = processor.getMotionState();
        if (motion != 0)
        {
            g.setColour (motion == 1 ? juce::Colour (0xfff23a36)
                                     : juce::Colours::white.withAlpha (0.82f));
            g.drawText (motion == 1 ? "MOTION REC" : "MOTION PLAY",
                        270, 548, 170, 22, juce::Justification::centredLeft);
        }
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
    g.drawText ("CLOSE", 620, 30, 70, 24, juce::Justification::centredRight);

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

    const float rawLevel = processor.getInputLevel();
    const float level = juce::jlimit (0.0f, 1.0f, rawLevel * 3.0f);
    const float db = rawLevel > 0.0000001f ? juce::Decibels::gainToDecibels (rawLevel) : -120.0f;
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillRect (54.0f, 423.0f, 420.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.86f));
    g.fillRect (54.0f, 422.0f, 420.0f * level, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.58f));
    g.drawText ("INPUT LEVEL  " + juce::String (db, 1) + " dBFS", 488, 410, 190, 26, juce::Justification::centredLeft);

    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        juce::AudioDeviceManager::AudioDeviceSetup route;
        holder->deviceManager.getAudioDeviceSetup (route);
        auto* dev = holder->deviceManager.getCurrentAudioDevice();
        const int inputChans = dev != nullptr ? dev->getActiveInputChannels().countNumberOfSetBits() : 0;
        const double sr = dev != nullptr ? dev->getCurrentSampleRate() : 0.0;

        g.setFont (juce::FontOptions (10.5f));
        g.setColour (juce::Colours::white.withAlpha (0.46f));
        g.drawFittedText ("ROUTE IN: " + (route.inputDeviceName.isNotEmpty() ? route.inputDeviceName : juce::String ("NONE")),
                          54, 438, 600, 18, juce::Justification::centredLeft, 1);
        g.drawFittedText ("ROUTE OUT: " + (route.outputDeviceName.isNotEmpty() ? route.outputDeviceName : juce::String ("NONE")),
                          54, 456, 600, 18, juce::Justification::centredLeft, 1);
        g.drawText ("IN CH " + juce::String (inputChans) + "    SR " + juce::String ((int) sr)
                    + (lastAudioRouteError.isNotEmpty() ? "    ERR " + lastAudioRouteError : juce::String()),
                    54, 474, 610, 18, juce::Justification::centredLeft);
    }

    const bool midiControl = processor.state().getRawParameterValue (ParamID::midiControl)->load() >= 0.5f;
    const int clock = juce::roundToInt (processor.state().getRawParameterValue (ParamID::clockMode)->load());
    const int bpm = juce::roundToInt (processor.state().getRawParameterValue (ParamID::internalBpm)->load());

    g.setColour (juce::Colours::white.withAlpha (0.78f));
    g.setFont (juce::FontOptions (16.0f));
    g.drawText ("MIDI CONTROL", 54, 506, 220, 34, juce::Justification::centredLeft);
    g.drawText (midiControl ? "ON" : "OFF", 450, 506, 180, 34, juce::Justification::centredRight);
    g.drawText ("MIDI SETTINGS", 54, 542, 220, 34, juce::Justification::centredLeft);
    g.drawText (">", 450, 542, 180, 34, juce::Justification::centredRight);
    g.drawText ("CLOCK", 54, 578, 180, 34, juce::Justification::centredLeft);
    g.drawText (clock == 0 ? "Internal" : "MIDI", 450, 578, 180, 34, juce::Justification::centredRight);
    if (clock == 0)
    {
        g.drawText ("BPM", 54, 614, 180, 34, juce::Justification::centredLeft);
        g.drawText (juce::String (bpm), 450, 614, 180, 34, juce::Justification::centredRight);
    }
    g.setFont (juce::FontOptions (11.5f));
    g.setColour (juce::Colours::white.withAlpha (0.42f));
    g.drawText ("MIDI CONTROL is independent from the chord CLOCK setting.", 54, 672, 560, 20, juce::Justification::centredLeft);
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
    g.drawText ("L1: REC -> PLAY    R1: CLEAR    96 motion ticks / bar", 54, 592, 610, 20, juce::Justification::centredLeft);
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
    auto r = parameterBounds (idx);
    const float norm = juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth());
    if (which == DragParam::complex) setNorm (ParamID::complex, norm);
    else if (which == DragParam::width) setNorm (ParamID::width, norm);
    else if (which == DragParam::length) setNorm (ParamID::length, norm);
    else if (which == DragParam::bar)
    {
        const int step = juce::jlimit (0, 3, juce::roundToInt (norm * 3.0f));
        setNorm (ParamID::bar, step / 3.0f);
    }
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

        if (p.y >= 504 && p.y < 540)
        {
            auto* par = processor.state().getParameter (ParamID::midiControl);
            par->setValueNotifyingHost (par->getValue() < 0.5f ? 1.0f : 0.0f);
            processor.notifyMidiControllerConfigChanged();
        }
        else if (p.y >= 540 && p.y < 576)
        {
            midiControlConfigVisible = true;
            configVisible = false;
            refreshMidiOutputs();
        }
        else if (p.y >= 576 && p.y < 612)
        {
            auto* par = processor.state().getParameter (ParamID::clockMode);
            par->setValueNotifyingHost (par->getValue() < 0.5f ? 1.0f : 0.0f);
        }
        else if (p.y >= 612 && p.y < 650 && processor.state().getRawParameterValue (ParamID::clockMode)->load() < 0.5f)
        {
            auto* par = processor.state().getParameter (ParamID::internalBpm);
            par->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, par->getValue() + (p.x > 360 ? 1.0f/200.0f : -1.0f/200.0f)));
        }
        repaint();
        return;
    }

    if (p.x >= 575 && p.y <= 70) { configVisible = true; refreshAudioInputs(); repaint(); return; }

    for (int i = 0; i < 4; ++i)
    {
        if (parameterBounds (i).contains (p))
        {
            dragging = (i == 0 ? DragParam::complex : i == 1 ? DragParam::bar : i == 2 ? DragParam::width : DragParam::length);
            setParameterFromX (dragging, p.x); repaint(); return;
        }
    }

    const bool midiControlOn = processor.state().getRawParameterValue (ParamID::midiControl)->load() >= 0.5f;
    if (midiControlOn)
    {
        if (p.x <= 170 && p.y <= 70) processor.toggleRunState();
        else { xyDragging = true; updateMidiControllerFromPoint (p); }
    }
    else processor.toggleRunState();
    repaint();
}

void RealtimeChordFxAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (midiControlConfigVisible || configVisible) return;
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
    if (xyDragging)
    {
        xyDragging = false;
        processor.releaseMidiControllerTouch();
        repaint();
    }
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
