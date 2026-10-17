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

    struct VongonControl { const char* name; int cc; };
    static constexpr VongonControl ultrasheerControls[] {
        { "VIBRATO DEPTH", 100 }, { "VIBRATO RATE", 101 },
        { "CYCLE / RANDOM", 102 }, { "REVERB BRIGHT", 103 },
        { "REVERB MIX", 104 }, { "REVERB DECAY", 105 },
        { "VIB BYPASS", 106 }, { "REV BYPASS", 107 },
        { "FX ORDER", 108 }, { "REVERB TRAILS", 109 }
    };
    static constexpr VongonControl rosetonesControls[] {
        { "BYPASS", 13 }, { "MOD MODE", 14 },
        { "MIX", 80 }, { "SYNTH LEVEL", 81 },
        { "TONE", 82 }, { "GLIDE TIME", 83 },
        { "MODULATION", 84 }, { "SENS", 85 },
        { "FEEDBACK", 86 }, { "OCTAVE", 87 },
        { "INTERVAL", 90 }
    };
    static constexpr VongonControl solarfazeControls[] {
        { "DETUNE BYPASS", 13 }, { "SAT BYPASS", 14 },
        { "DETUNE MIX", 80 }, { "DETUNE AMOUNT", 81 },
        { "DETUNE ENV", 82 }, { "DETUNE SIZE", 96 },
        { "BASS LEVEL", 83 }, { "MID LEVEL", 84 },
        { "TREB LEVEL", 85 }, { "BASS DRIVE", 89 },
        { "MID DRIVE", 90 }, { "TREB DRIVE", 91 },
        { "BASS FREQ", 93 }, { "MID FREQ", 94 },
        { "TREB FREQ", 95 }, { "SAT MIX", 86 },
        { "SAT TONE", 87 }, { "SAT DRIVE", 88 },
        { "GATE BYPASS", 92 }, { "GATE THRESH", 98 },
        { "FX ORDER", 97 }
    };
    static constexpr VongonControl polyphraseControls[] {
        { "REPEATS", 1 }, { "TONE", 2 },
        { "DEPTH", 3 }, { "RATE", 4 },
        { "DELAY LEFT", 5 }, { "DELAY RIGHT", 6 },
        { "MIX", 7 }, { "TIME", 8 },
        { "ECHO STYLE", 9 }, { "SYNC", 10 },
        { "TRIG MODE", 11 }, { "BYPASS", 13 },
        { "INFINITE", 14 }, { "TRIG / TAP", 15 },
        { "TRAILS", 16 }, { "MIDI CLOCK", 17 }
    };
    const VongonControl* getVongonControls (int mode, int& count)
    {
        if (mode == 5) { count = 10; return ultrasheerControls; }
        if (mode == 6) { count = 11; return rosetonesControls; }
        if (mode == 7) { count = 21; return solarfazeControls; }
        count = 16; return polyphraseControls;
    }

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

                if (! name.containsIgnoreCase ("RG Rotate"))
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
    juce::StringArray getAndroidPhysicalInputNames() { return {}; }
    int getAndroidOutputFramesPerBuffer() { return 0; }
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

    vongonFrames.load (
        BinaryData::vongon_frames_pack,
        BinaryData::vongon_frames_packSize);
    if (vongonFrames.getFrameCount() > 0)
    {
        currentVongonFrame = vongonFrames.getFrame (0);
        loadedVongonFrame = 0;
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

    audioBufferMode = juce::jlimit (0, 11, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::audioBufferMode)->load()));
    refreshAudioOutputs();

    bool burstAppliedByOutputRestore = false;
    const auto savedOutputType =
        processor.state().state.getProperty ("audioOutputType").toString();
    const auto savedOutputName =
        processor.state().state.getProperty ("audioOutputName").toString();

    if (savedOutputType.isNotEmpty() && savedOutputName.isNotEmpty())
    {
        for (int i = 0; i < (int) audioOutputs.size(); ++i)
        {
            const auto& option = audioOutputs[(size_t) i];
            if (option.type == savedOutputType
                && option.name == savedOutputName)
            {
                if (i != selectedAudioOutput)
                {
                    selectAudioOutput (i);
                    burstAppliedByOutputRestore = audioBufferMode != 0;
                }
                break;
            }
        }
    }

    refreshAudioBufferStatus();
    if (! burstAppliedByOutputRestore && audioBufferMode != 0)
        applyAudioBufferMode (audioBufferMode);

    startTimerHz (30);
}

RealtimeChordFxAudioProcessorEditor::~RealtimeChordFxAudioProcessorEditor() { stopTimer(); }

juce::Point<float> RealtimeChordFxAudioProcessorEditor::toDesign (juce::Point<float> p) const
{
    return { p.x * design / (float) juce::jmax (1, getWidth()),
             p.y * design / (float) juce::jmax (1, getHeight()) };
}

bool RealtimeChordFxAudioProcessorEditor::dotModeEnabled() const
{
    if (const auto* value =
            processor.state().getRawParameterValue (ParamID::displayMode))
        return juce::roundToInt (value->load()) == 1;

    return false;
}

juce::Colour RealtimeChordFxAudioProcessorEditor::uiColour (float alpha) const
{
    return dotModeEnabled()
        ? juce::Colour (0xffffdb46).withAlpha (alpha)
        : juce::Colours::white.withAlpha (alpha);
}

void RealtimeChordFxAudioProcessorEditor::rebuildDotLuma (
    const juce::Image& source, int sourceKind, int sourceRevision)
{
    if (! source.isValid())
        return;

    juce::Image small (
        juce::Image::RGB, dotSourceW, dotSourceH, true);
    juce::Graphics downsampler (small);
    downsampler.setImageResamplingQuality (
        juce::Graphics::mediumResamplingQuality);
    downsampler.drawImage (
        source,
        0, 0, dotSourceW, dotSourceH,
        0, 0, source.getWidth(), source.getHeight());

    juce::Image::BitmapData data (
        small, juce::Image::BitmapData::readOnly);

    for (int y = 0; y < dotSourceH; ++y)
    {
        for (int x = 0; x < dotSourceW; ++x)
        {
            const auto c = data.getPixelColour (x, y);
            dotLuma[(size_t) (y * dotSourceW + x)] =
                0.2126f * c.getFloatRed()
                + 0.7152f * c.getFloatGreen()
                + 0.0722f * c.getFloatBlue();
        }
    }

    dotLumaSourceKind = sourceKind;
    dotLumaRevision = sourceRevision;
    dotRenderedRevision = -1;
}

void RealtimeChordFxAudioProcessorEditor::rebuildDotDisplay (
    int width, int height, int sourceKind, int sourceRevision)
{
    width = juce::jlimit (1, dotSourceW, width);
    height = juce::jlimit (1, dotSourceH, height);

    if (! dotDisplayCache.isValid()
        || dotDisplayCache.getWidth() != width
        || dotDisplayCache.getHeight() != height)
    {
        dotDisplayCache = juce::Image (
            juce::Image::RGB, width, height, true);
        dotRenderedRevision = -1;
    }

    static constexpr int bayer4[16] = {
         0,  8,  2, 10,
        12,  4, 14,  6,
         3, 11,  1,  9,
        15,  7, 13,  5
    };

    const juce::Colour dark   (0xff100e08);
    const juce::Colour mid    (0xff80671e);
    const juce::Colour amber  (0xffffdb46);
    const juce::Colour bright (0xffffe57a);

    juce::Image::BitmapData out (
        dotDisplayCache, juce::Image::BitmapData::readWrite);

    const float activity = juce::jlimit (0.0f, 1.0f, dotActivity);
    // Only the two video banks lose the old moving scanline.
    // Keep the original DOT treatment for CHORD/DREAMY/CLOUDS.
    const bool videoBank = sourceKind == 1 || sourceKind == 2;
    const float flicker =
        1.0f + std::sin ((float) dotUiTick * 0.42f)
                     * ((videoBank ? 0.0f : 0.035f)
                        + 0.045f * activity);

    for (int y = 0; y < height; ++y)
    {
        const int sy0 = juce::jlimit (
            0, dotSourceH - 1,
            (y * dotSourceH) / juce::jmax (1, height));

        const uint32_t rowHash =
            (uint32_t) y * 1103515245u
            + (uint32_t) dotUiTick * 2654435761u;

        int rowShift = 0;
        if (activity > 0.08f
            && (rowHash & 31u) < (uint32_t) (activity * 5.0f))
            rowShift = (int) ((rowHash >> 8) % 5u) - 2;

        for (int x = 0; x < width; ++x)
        {
            const int shiftedX = juce::jlimit (
                0, width - 1, x + rowShift);
            const int sx = juce::jlimit (
                0, dotSourceW - 1,
                (shiftedX * dotSourceW) / juce::jmax (1, width));

            float luminance =
                dotLuma[(size_t) (sy0 * dotSourceW + sx)] * flicker;

            if (! videoBank && ((y + dotUiTick) % 8) == 0)
                luminance *= 1.0f - 0.12f * activity;

            const uint32_t pixelHash =
                rowHash ^ ((uint32_t) x * 2246822519u);
            if ((pixelHash & 2047u)
                < (uint32_t) (activity * 8.0f))
                luminance = 1.0f - luminance;

            luminance = juce::jlimit (0.0f, 1.0f, luminance);

            const float threshold =
                0.13f
                + 0.63f
                    * (float) bayer4[
                        (x & 3) + ((y & 3) << 2)]
                    / 16.0f;

            const auto colour =
                luminance < threshold ? dark
                : luminance > 0.88f ? bright
                : luminance > 0.63f ? amber
                                    : mid;

            out.setPixelColour (x, y, colour);
        }
    }

    dotRenderedSourceKind = sourceKind;
    dotRenderedRevision = sourceRevision;
    dotRenderedTick = dotUiTick;
}

void RealtimeChordFxAudioProcessorEditor::paintMediaFrame (
    juce::Graphics& g,
    const juce::Image& source,
    juce::Rectangle<float> bounds,
    int sourceKind,
    int sourceRevision)
{
    if (! source.isValid())
        return;

    if (! dotModeEnabled())
    {
        g.drawImage (
            source, bounds,
            juce::RectanglePlacement::stretchToFit);
        return;
    }

    if (dotLumaSourceKind != sourceKind
        || dotLumaRevision != sourceRevision)
        rebuildDotLuma (source, sourceKind, sourceRevision);

    const int width = juce::jmin (
        dotSourceW, juce::jmax (1, juce::roundToInt (bounds.getWidth())));
    const int height = juce::jmin (
        dotSourceH, juce::jmax (1, juce::roundToInt (bounds.getHeight())));

    if (! dotDisplayCache.isValid()
        || dotRenderedSourceKind != sourceKind
        || dotRenderedRevision != sourceRevision
        || dotRenderedTick != dotUiTick
        || dotDisplayCache.getWidth() != width
        || dotDisplayCache.getHeight() != height)
    {
        rebuildDotDisplay (
            width, height, sourceKind, sourceRevision);
    }

    g.setImageResamplingQuality (
        juce::Graphics::lowResamplingQuality);
    g.drawImage (
        dotDisplayCache, bounds,
        juce::RectanglePlacement::stretchToFit);
}

void RealtimeChordFxAudioProcessorEditor::paintDotUiFrame (
    juce::Graphics& g)
{
    if (! dotModeEnabled())
        return;

    const auto amber = juce::Colour (0xffffdb46);
    g.setColour (amber.withAlpha (0.44f));

    for (int x = 12; x < 708; x += 16)
    {
        g.fillRect ((float) x, 8.0f, 5.0f, 2.0f);
        g.fillRect ((float) x, 710.0f, 5.0f, 2.0f);
    }

    for (int y = 20; y < 700; y += 16)
    {
        g.fillRect (8.0f, (float) y, 2.0f, 5.0f);
        g.fillRect (710.0f, (float) y, 2.0f, 5.0f);
    }

    g.setColour (amber.withAlpha (0.86f));
    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    g.drawText ("DOT", 310, 18, 48, 20,
                juce::Justification::centred);
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
    g.setColour (uiColour (0.84f));
    g.setFont (dotModeEnabled()
        ? juce::FontOptions (13.0f).withStyle ("Bold")
        : juce::FontOptions (13.0f));
    g.drawText (label, r.removeFromTop (20.0f), juce::Justification::centredLeft);
    auto line = r.removeFromTop (20.0f).reduced (0.0f, 8.0f);
    g.setColour (uiColour (0.30f));
    g.fillRect (line.withHeight (1.0f));
    g.setColour (uiColour (0.88f));
    g.fillRect (line.withWidth (
        line.getWidth() * juce::jlimit (0.0f, 1.0f, value))
        .withHeight (dotModeEnabled() ? 4.0f : 2.0f));

    if (dotModeEnabled())
    {
        g.fillRect (
            line.getX() + line.getWidth() * value - 4.0f,
            line.getCentreY() - 4.0f, 8.0f, 8.0f);
    }
    else
    {
        g.fillEllipse (
            line.getX() + line.getWidth() * value - 3.0f,
            line.getCentreY() - 3.0f, 6.0f, 6.0f);
    }

    g.setFont (dotModeEnabled()
        ? juce::FontOptions (13.0f).withStyle ("Bold")
        : juce::FontOptions (13.0f));
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
    processor.applyPendingMidiInputChanges();
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();

    const int effectMode = juce::jlimit (0, 8, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));

    const float activity = juce::jmax (
        processor.getInputPeakRaw(),
        processor.getInputRmsRaw() * 2.2f);

    if ((effectMode == 0 || effectMode == 4) && frames.getFrameCount() > 0)
    {
        if (chordVisualCooldown > 0)
            --chordVisualCooldown;

        const bool attack =
            activity > 0.007f
            && activity > chordVisualPreviousActivity * 1.35f + 0.002f;

        // Event-led cuts only. Continuous notes must not cycle frames
        // at a fixed slideshow rate.
        if (attack && chordVisualCooldown <= 0)
        {
            const int frame =
                nextVisualFrame (frames.getFrameCount(), loadedFrame);
            currentFrame = frames.getFrame (frame);
            loadedFrame = frame;
            ++dotFrameRevision;
            chordVisualCooldown = 21; // >=0.7 s between actual note onsets
        }

        chordVisualPreviousActivity =
            chordVisualPreviousActivity * 0.60f + activity * 0.40f;
    }
    else if (effectMode == 1 && frames.getFrameCount() > 0)
    {
        const int frame = processor.getVisualFrame();
        if (frame != loadedFrame)
        {
            currentFrame = frames.getFrame (frame);
            loadedFrame = frame;
            ++dotFrameRevision;
        }
    }
    else if (effectMode == 2 && eurekaFrames.getFrameCount() > 0)
    {
        // The supplied MP4 was stored as ordered 36-frame/8-second clips.
        // Play each clip sequentially; never choose random stills.
        const bool attack =
            activity > 0.006f
            && activity > eurekaVisualPreviousActivity * 1.28f + 0.0015f;
        eurekaSwitchClipPending |= attack;
        const int total = eurekaFrames.getFrameCount();
        const int clipFrames = 36;
        const int clips = total / clipFrames;
        eurekaFramePhase += 4.5f / 30.0f;
        while (eurekaFramePhase >= 1.0f)
        {
            eurekaFramePhase -= 1.0f;
            const int position = eurekaFrameIndex % clipFrames;
            const int clip = eurekaFrameIndex / clipFrames;
            if (position == clipFrames - 1)
            {
                const int nextClip =
                    eurekaSwitchClipPending && clips > 1
                    ? (clip + 1) % clips : clip;
                eurekaFrameIndex = nextClip * clipFrames;
                eurekaSwitchClipPending = false;
            }
            else
                ++eurekaFrameIndex;
        }
        if (eurekaFrameIndex != loadedEurekaFrame)
        {
            currentEurekaFrame = eurekaFrames.getFrame (
                eurekaFrameIndex);
            loadedEurekaFrame = eurekaFrameIndex;
            ++dotEurekaRevision;
        }
        eurekaVisualPreviousActivity =
            0.60f * eurekaVisualPreviousActivity + 0.40f * activity;
    }
    else if (effectMode >= 5
             && effectMode <= 8
             && vongonFrames.getFrameCount() > 0)
    {
        const float x = juce::jlimit (0.0f, 1.0f, processor.getVongonX());
        const float y = juce::jlimit (0.0f, 1.0f, processor.getVongonY());
        const float xyDelta = std::abs (x - vongonPreviousX)
                            + std::abs (y - vongonPreviousY);
        vongonPreviousX = x;
        vongonPreviousY = y;

        // Activity and physical XY movement control time itself. Silence
        // with stationary parameters must settle to a held video frame.
        const float musicalEnergy = juce::jlimit (
            0.0f, 1.0f, (activity - 0.004f) * 12.0f);
        const float gestureEnergy = juce::jlimit (
            0.0f, 1.0f, xyDelta * 7.0f);
        const float drive = juce::jmax (musicalEnergy, gestureEnergy);
        const bool attack =
            activity > 0.009f
            && activity > vongonVisualPreviousActivity * 1.35f + 0.002f;
        const bool gesture = gestureEnergy > 0.15f;

        const int total = vongonFrames.getFrameCount();
        const bool fullVideo = total == 9 * 192 || total == 6 * 192;
        const int clipFrames = fullVideo ? 192 : 36;
        const int clips = juce::jmax (1, total / clipFrames);
        const float maximumFps = fullVideo ? 24.0f : 4.5f;

        if (vongonClipCooldown > 0)
            --vongonClipCooldown;
        if (vongonFxCooldown > 0)
            --vongonFxCooldown;

        if (attack || gesture)
            vongonVisualFx = juce::jmax (
                vongonVisualFx,
                juce::jlimit (0.34f, 1.0f,
                    0.46f + 0.38f * drive + 0.30f * y));

        if ((attack || gesture) && vongonFxCooldown == 0)
        {
            // Hold each treatment for at least 270ms even while dragging:
            // rapid parameter changes must not become a slideshow of FX.
            vongonFxCooldown = 8;
            vongonGhostFrame = currentVongonFrame;
            visualRandomState ^= visualRandomState << 13;
            visualRandomState ^= visualRandomState >> 17;
            visualRandomState ^= visualRandomState << 5;
            vongonFxStyle = (int) (visualRandomState % 5u);
            ++vongonFxEvent;
            // Short tape pull-back on some transients; not a running loop.
            if (vongonFxStyle == 1 || vongonFxStyle == 4)
                vongonRewindFrames = 4
                    + juce::roundToInt (x * 10.0f);
        }
        vongonVisualFx = juce::jmax (
            vongonVisualFx * 0.925f, musicalEnergy * 0.12f);

        // A new distinct clip can be selected at an onset, but only after
        // a long cooldown. Within the clip, motion and degradation are
        // much more important than repeatedly changing videos.
        if (attack && clips > 1 && vongonClipCooldown == 0)
        {
            const int previousClip = vongonFrameIndex / clipFrames;
            const int nextClip = nextVisualFrame (clips, previousClip);
            vongonFrameIndex = nextClip * clipFrames;
            vongonFramePhase = 0.0f;
            vongonClipCooldown = 120; // >=4 seconds between clip cuts
            vongonRewindFrames = 0;
        }

        const float targetFps = drive < 0.018f ? 0.0f
            : juce::jmin (maximumFps,
                (2.0f + maximumFps * drive)
                * (0.48f + 0.52f * x));
        vongonVideoSpeed += 0.23f
            * (targetFps - vongonVideoSpeed);
        if (targetFps == 0.0f && vongonVideoSpeed < 0.14f)
            vongonVideoSpeed = 0.0f;

        vongonFramePhase += vongonVideoSpeed / 30.0f;
        while (vongonFramePhase >= 1.0f)
        {
            vongonFramePhase -= 1.0f;
            const int clip = vongonFrameIndex / clipFrames;
            const int position = vongonFrameIndex % clipFrames;
            if (vongonRewindFrames > 0)
            {
                vongonFrameIndex = clip * clipFrames
                    + (position + clipFrames - 1) % clipFrames;
                --vongonRewindFrames;
            }
            else
            {
                // A held note advances smoothly through its own clip.
                // Reaching the end loops within that clip, never a
                // fixed-rate mandatory switch to the next clip.
                vongonFrameIndex = clip * clipFrames
                    + (position + 1) % clipFrames;
            }
        }

        if (vongonFrameIndex != loadedVongonFrame)
        {
            currentVongonFrame = vongonFrames.getFrame (
                vongonFrameIndex);
            loadedVongonFrame = vongonFrameIndex;
            ++dotVongonRevision;
        }
        vongonVisualPreviousActivity =
            0.60f * vongonVisualPreviousActivity + 0.40f * activity;
    }
    dotActivity = juce::jlimit (0.0f, 1.0f, activity * 18.0f);
    if (dotModeEnabled() && ++dotTimerDivider >= 2)
    {
        dotTimerDivider = 0;
        ++dotUiTick;
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
    else if (audioConfigVisible)
        paintAudioConfig (g);
    else if (configVisible)
        paintConfig (g);
    else
    {
        paintMain (g);
        paintGlobalControls (g);
    }

    paintDotUiFrame (g);
}

void RealtimeChordFxAudioProcessorEditor::paintGlobalControls (juce::Graphics& g)
{
    const int effectMode = juce::jlimit (0, 8, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));
    if (effectMode == 3)
        return;

    const float boostDb = juce::jlimit (
        0.0f, 14.0f,
        processor.state().getRawParameterValue (ParamID::boostDb)->load());
    const juce::Rectangle<float> boostBounds (390.0f, 16.0f, 76.0f, 38.0f);
    g.setColour (juce::Colours::black.withAlpha (0.50f));
    g.fillRoundedRectangle (boostBounds, 6.0f);
    g.setColour (uiColour (boostDb > 0.01f ? 0.96f : 0.62f));
    g.drawRoundedRectangle (boostBounds, 6.0f, boostDb > 0.01f ? 1.8f : 1.0f);
    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    g.drawFittedText (
        boostDb > 0.01f ? "BOOST +" + juce::String (boostDb, 1)
                        : "BOOST",
        boostBounds.toNearestInt(), juce::Justification::centred, 1);

    if (l1Latched || r1Latched)
    {
        const float wet = juce::jlimit (
            0.0f, 1.0f,
            processor.state().getRawParameterValue (ParamID::wet)->load());
        const juce::Rectangle<float> wetBounds (250.0f, 76.0f, 220.0f, 52.0f);
        g.setColour (juce::Colours::black.withAlpha (0.80f));
        g.fillRoundedRectangle (wetBounds, 8.0f);
        g.setColour (uiColour (0.94f));
        g.drawRoundedRectangle (wetBounds, 8.0f, 1.2f);
        g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
        g.drawText ("WET " + juce::String (juce::roundToInt (wet * 100.0f)) + "%",
                    wetBounds, juce::Justification::centred);
    }
}

void RealtimeChordFxAudioProcessorEditor::paintMain (juce::Graphics& g)
{
    const int effectMode = juce::jlimit (0, 8, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));
    const int chordMode = juce::jlimit (0, 1, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::chordMode)->load()));

    if (effectMode == 3)
    {
        paintChordBot (g);
        return;
    }

    if (effectMode == 4)
    {
        paintClouds (g);
        return;
    }

    if (effectMode >= 5 && effectMode <= 8)
    {
        paintVongonMode (g, effectMode);
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
            paintMediaFrame (
                g, currentFrame,
                juce::Rectangle<float> (0, 0, design, design),
                0, dotFrameRevision);

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

        g.setColour (uiColour (0.88f));
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
        paintMediaFrame (
            g, currentFrame,
            juce::Rectangle<float> (0, 0, design, design),
            0, dotFrameRevision);
    
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
            g.setColour (uiColour (0.74f));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("REC", 24, 20, 80, 28, juce::Justification::centredLeft);
        }
    
        g.setColour (uiColour (0.88f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("CONFIG", 604, 22, 88, 24, juce::Justification::centredRight);
    
        const float dreamyX = juce::jlimit (0.0f, 1.0f, processor.getMidiControllerX() / 127.0f);
        const float dreamyY = juce::jlimit (0.0f, 1.0f, processor.getMidiControllerY() / 127.0f);
        const float reverbAmount = std::pow (dreamyX * dreamyY, 1.35f);
        const int reverbSteps = juce::jlimit (0, 10, juce::roundToInt (reverbAmount * 10.0f));

        g.setFont (juce::FontOptions (11.5f));
        g.setColour (uiColour (0.74f));
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
        g.setColour (uiColour (0.74f));
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
                                     : uiColour (0.82f));
            g.drawText (motion == 1 ? "MOTION REC" : "MOTION PLAY",
                        430, 660, 170, 22, juce::Justification::centredLeft);
        }
}


void RealtimeChordFxAudioProcessorEditor::paintClouds (juce::Graphics& g)
{
    if (currentFrame.isValid())
        paintMediaFrame (
            g, currentFrame,
            juce::Rectangle<float> (0, 0, design, design),
            0, dotFrameRevision);

    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillRect (0.0f, 0.0f, 720.0f, 86.0f);
    g.setColour (juce::Colours::black.withAlpha (0.58f));
    g.fillRect (0.0f, 560.0f, 720.0f, 160.0f);

    const float x = juce::jlimit (0.0f, 1.0f, processor.getCloudsX());
    const float y = juce::jlimit (0.0f, 1.0f, processor.getCloudsY());
    const bool touch = processor.getCloudsTouch();

    const float position = juce::jlimit (0.0f, 1.0f, 0.08f + x * 0.84f);
    const float size = juce::jlimit (0.0f, 1.0f, 0.14f + y * 0.72f);
    const float density = juce::jlimit (0.0f, 1.0f, 0.28f + y * 0.44f);
    const float texture = juce::jlimit (0.0f, 1.0f, 0.24f + x * 0.52f);
    const float pitch =
        x < 0.10f ? -12.0f
        : x < 0.22f ? -7.0f
        : x > 0.90f ? 12.0f
        : x > 0.78f ? 7.0f
                    : 0.0f;
    const float spread = juce::jlimit (0.0f, 1.0f, 0.18f + x * 0.58f);
    const float feedback = juce::jlimit (0.0f, 0.42f, 0.03f + y * 0.34f);
    const float reverb = juce::jlimit (
        0.0f, 0.72f, 0.02f + std::pow (x * y, 1.35f) * 0.68f);
    const float dryWet = juce::jlimit (
        0.18f, 0.88f, 0.24f + 0.58f * (0.30f * x + 0.70f * y));
    const bool freeze = y > 0.93f && x < 0.24f;

    g.setColour (uiColour (0.96f));
    g.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
    g.drawText ("CLOUDS", 28, 18, 180, 34,
                juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.5f));
    g.setColour (uiColour (0.70f));
    g.drawText ("GRANULAR XY", 164, 24, 180, 24,
                juce::Justification::centredLeft);
    g.setColour (uiColour (0.94f));
    g.drawText ("CONFIG", 604, 22, 88, 24,
                juce::Justification::centredRight);

    // Active XY field is y=70..720.  The marker and guide lines are display-only.
    const float px = x * design;
    const float py = 70.0f + (1.0f - y) * 650.0f;
    g.setColour (uiColour (touch ? 0.82f : 0.34f));
    g.drawLine (px, 72.0f, px, 558.0f, 1.0f);
    g.drawLine (0.0f, py, 720.0f, py, 1.0f);
    g.fillRect (px - 4.0f, py - 4.0f, 8.0f, 8.0f);

    g.setFont (juce::FontOptions (10.5f).withStyle ("Bold"));
    g.setColour (uiColour (freeze ? 0.98f : 0.66f));
    g.drawText (freeze ? "FREEZE" : "FREEZE: TOP-LEFT",
                24, 86, 170, 20, juce::Justification::centredLeft);
    g.setColour (uiColour (touch ? 0.94f : 0.62f));
    g.drawText (touch ? "GATE ON / TRIGGER" : "TOUCH = GATE / TAP = TRIGGER",
                430, 86, 264, 20, juce::Justification::centredRight);

    g.setColour (uiColour (0.80f));
    g.setFont (juce::FontOptions (10.5f));
    g.drawFittedText (
        "POSITION " + juce::String (position, 2)
        + "   SIZE " + juce::String (size, 2)
        + "   DENSITY " + juce::String (density, 2)
        + "   TEXTURE " + juce::String (texture, 2),
        24, 574, 672, 22, juce::Justification::centredLeft, 1);

    g.drawFittedText (
        "PITCH " + juce::String ((int) pitch)
        + "st   SPREAD " + juce::String (spread, 2)
        + "   FEEDBACK " + juce::String (feedback, 2)
        + "   REVERB " + juce::String (reverb, 2),
        24, 602, 672, 22, juce::Justification::centredLeft, 1);

    g.drawFittedText (
        "DRY/WET " + juce::String (dryWet, 2)
        + "   X " + juce::String (juce::roundToInt (x * 127.0f)).paddedLeft ('0', 3)
        + "   Y " + juce::String (juce::roundToInt (y * 127.0f)).paddedLeft ('0', 3),
        24, 630, 672, 22, juce::Justification::centredLeft, 1);

    g.setColour (uiColour (0.56f));
    g.setFont (juce::FontOptions (9.8f));
    g.drawFittedText (
        "X: POSITION / TEXTURE / PITCH(0, +/-7, +/-12) / SPREAD    Y: SIZE / DENSITY / FEEDBACK    XY: REVERB / DRY-WET",
        24, 666, 672, 24, juce::Justification::centredLeft, 1);
}

void RealtimeChordFxAudioProcessorEditor::paintVideoEffect (
    juce::Graphics& g, const juce::Image& source, float x, float y)
{
    if (! source.isValid())
        return;

    // Do not superimpose scrolling scanlines over the video. EUREKA keeps
    // its own ordered visual bank. Only Vongon video gets these live
    // treatments, and they decay completely during quiet inactivity.
    const int mode = juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load());
    if (mode < 5 || mode > 8 || vongonVisualFx < 0.025f)
        return;

    const float energy = juce::jlimit (
        0.0f, 1.0f,
        vongonVisualFx * (0.45f + 0.35f * x + 0.30f * y));
    const bool dot = dotModeEnabled();
    const juce::Image& video = dot && dotDisplayCache.isValid()
        ? dotDisplayCache : source;

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (juce::Rectangle<int> (0, 0, 720, 720));
    const float bandY = (float) ((vongonFxEvent * 137 + 83) % 580);
    const float bandH = 24.0f + 100.0f * y * energy;
    const float offset = (x >= 0.5f ? 1.0f : -1.0f)
        * (8.0f + 65.0f * x) * energy;
    const int style = vongonFxStyle;

    // Video feedback: an earlier source frame is briefly superimposed,
    // rather than replacing the moving picture with another still.
    if (style == 1 || style == 3 || style == 4)
    {
        const juce::Image& echo = ! dot && vongonGhostFrame.isValid()
            ? vongonGhostFrame : video;
        g.setOpacity (0.15f + 0.23f * energy);
        g.drawImage (echo,
            juce::Rectangle<float> (offset * 0.5f, -offset * 0.15f,
                                    720.0f, 720.0f),
            juce::RectanglePlacement::stretchToFit);
    }

    // A portion of the real video image is displaced horizontally: a
    // tracking error or tape shear, never a static line overlaid on top.
    if (style == 0 || style == 2 || style == 4)
    {
        g.setOpacity (juce::jlimit (0.20f, 0.92f, 0.35f + energy));
        const float srcY = bandY * (float) video.getHeight() / 720.0f;
        const float srcH = bandH * (float) video.getHeight() / 720.0f;
        const int srcTop = juce::jlimit (
            0, video.getHeight() - 1, juce::roundToInt (srcY));
        const int srcHeight = juce::jlimit (
            1, video.getHeight() - srcTop, juce::roundToInt (srcH));
        g.drawImage (
            video,
            juce::roundToInt (offset), juce::roundToInt (bandY),
            720, juce::jmax (1, juce::roundToInt (bandH)),
            0, srcTop, video.getWidth(), srcHeight);
        // A second independent tear adds a VHS head-switching feel.
        g.setOpacity (0.30f * energy);
        g.drawImage (
            video,
            juce::roundToInt (-offset * 0.6f),
            juce::roundToInt (bandY + bandH + 14.0f),
            720, juce::jmax (1, juce::roundToInt (bandH * 0.46f)),
            0,
            juce::jlimit (0, video.getHeight() - 1,
                juce::roundToInt (
                    (bandY + bandH + 14.0f)
                    * (float) video.getHeight() / 720.0f)),
            video.getWidth(),
            juce::jmax (1, juce::roundToInt (
                bandH * 0.46f * (float) video.getHeight() / 720.0f)));
    }

    // Chromatic misregistration: red and cyan channels have independent
    // displacement. Small cached images keep pixel work off the audio
    // thread and avoid full-resolution processing on each paint call.
    if (! dot && (style == 2 || style == 3 || style == 4))
    {
        if (vongonFringeRevision != dotVongonRevision
            || ! vongonRedFringe.isValid()
            || ! vongonCyanFringe.isValid())
        {
            constexpr int w = 160, h = 90;
            juce::Image small (juce::Image::RGB, w, h, true);
            juce::Graphics downsample (small);
            downsample.drawImage (
                source,
                juce::Rectangle<float> (
                    0.0f, 0.0f, (float) w, (float) h),
                juce::RectanglePlacement::stretchToFit);

            vongonRedFringe = juce::Image (
                juce::Image::ARGB, w, h, true);
            vongonCyanFringe = juce::Image (
                juce::Image::ARGB, w, h, true);
            juce::Image::BitmapData input (
                small, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData red (
                vongonRedFringe, juce::Image::BitmapData::readWrite);
            juce::Image::BitmapData cyan (
                vongonCyanFringe, juce::Image::BitmapData::readWrite);

            for (int row = 0; row < h; ++row)
                for (int col = 0; col < w; ++col)
                {
                    const auto pixel = input.getPixelColour (col, row);
                    red.setPixelColour (col, row,
                        juce::Colour::fromRGBA (
                            pixel.getRed(), 0, 0, 185));
                    cyan.setPixelColour (col, row,
                        juce::Colour::fromRGBA (
                            0, pixel.getGreen(), pixel.getBlue(), 185));
                }
            vongonFringeRevision = dotVongonRevision;
        }

        const float split = 3.0f + 13.0f * energy * (0.3f + x);
        g.setOpacity (0.14f + 0.40f * energy);
        g.drawImage (
            vongonRedFringe,
            juce::Rectangle<float> (
                -split, 0.0f, 720.0f, 720.0f),
            juce::RectanglePlacement::stretchToFit);
        g.drawImage (
            vongonCyanFringe,
            juce::Rectangle<float> (
                split, 0.0f, 720.0f, 720.0f),
            juce::RectanglePlacement::stretchToFit);
    }

    // Sparse tape tracking noise appears on events only. No evenly
    // spaced lines and no monotonically scrolling 1-mm stripe pattern.
    if (style == 0 || style == 4)
    {
        g.setOpacity (1.0f);
        for (int i = 0; i < 3; ++i)
        {
            const int yy = (vongonFxEvent * 173 + i * 239) % 718;
            g.setColour (
                juce::Colours::white.withAlpha (
                    (0.08f + 0.18f * energy) / (float) (i + 1)));
            g.fillRect (
                18.0f + (float) (i * 97), (float) yy,
                160.0f + 400.0f * energy, 1.0f);
        }
    }
}

void RealtimeChordFxAudioProcessorEditor::paintVongonMode (
    juce::Graphics& g, int effectMode)
{
    if (currentVongonFrame.isValid())
        paintMediaFrame (
            g, currentVongonFrame,
            juce::Rectangle<float> (0, 0, design, design),
            2, dotVongonRevision);
    paintVideoEffect (
        g, currentVongonFrame,
        processor.getVongonX(), processor.getVongonY());

    const float x = juce::jlimit (0.0f, 1.0f, processor.getVongonX());
    const float y = juce::jlimit (0.0f, 1.0f, processor.getVongonY());

    const juce::String name =
        effectMode == 5 ? "ULTRASHEER"
        : effectMode == 6 ? "ROSETONES"
        : effectMode == 7 ? "SOLARFAZE"
                          : "POLYPHRASE";

    const juce::String descriptor =
        effectMode == 5 ? "PLATE / VIBRATO"
        : effectMode == 6 ? "CHROMATIC RESONATOR"
        : effectMode == 7 ? "MULTIBAND SAT / DETUNE"
                          : "DUAL DELAY / LOOPER";

    g.setColour (uiColour (0.96f));
    g.setFont (juce::FontOptions (21.0f).withStyle ("Bold"));
    g.drawText (name, 28, 18, 260, 34,
                juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.5f));
    g.setColour (uiColour (0.70f));
    g.drawText (descriptor, 270, 24, 240, 24,
                juce::Justification::centredLeft);
    g.setColour (uiColour (0.94f));
    g.drawText ("CONFIG", 604, 22, 88, 24,
                juce::Justification::centredRight);
    g.drawText (vongonPanelVisible ? "CLOSE" : "PARAM",
                500, 20, 74, 30, juce::Justification::centred);

    if (vongonPanelVisible)
    {
        paintVongonParameterPanel (g, effectMode);
        return;
    }

    g.setColour (uiColour (0.80f));
    g.setFont (juce::FontOptions (10.8f));

    juce::String line1, line2;
    if (effectMode == 5)
    {
        line1 = "X: VIBRATO DEPTH / RATE / CYCLE→RANDOM";
        line2 = "Y: PLATE DECAY / BRIGHTNESS / MIX";
    }
    else if (effectMode == 6)
    {
        line1 = "X: RESONATOR REGISTER / TONE";
        line2 = "Y: SENSITIVITY / FEEDBACK / SYNTH MIX";
    }
    else if (effectMode == 7)
    {
        line1 = "X: BASS / MID / TREB SATURATION";
        line2 = "Y: ENVELOPE DETUNE / STEREO WIDTH / MIX";
    }
    else
    {
        line1 = "X: DELAY TIME 85ms→22s / ROUTING";
        line2 = "Y: FEEDBACK / TONE / MOD / MIX   TOP: HOLD";
    }

    g.drawFittedText (
        line1, 24, 604, 672, 22,
        juce::Justification::centredLeft, 1);
    g.drawFittedText (
        line2, 24, 634, 672, 22,
        juce::Justification::centredLeft, 1);

    g.setColour (uiColour (0.60f));
    g.drawText (
        "X " + juce::String (juce::roundToInt (x * 127.0f)).paddedLeft ('0', 3)
        + "   Y " + juce::String (juce::roundToInt (y * 127.0f)).paddedLeft ('0', 3),
        24, 670, 220, 20,
        juce::Justification::centredLeft);
}

void RealtimeChordFxAudioProcessorEditor::paintVongonParameterPanel (
    juce::Graphics& g, int mode)
{
    int count = 0;
    const auto* items = getVongonControls (mode, count);
    const int pageCount = (count + 7) / 8;
    vongonPanelPage = juce::jlimit (0, pageCount - 1, vongonPanelPage);

    g.setColour (juce::Colours::black.withAlpha (0.90f));
    g.fillRoundedRectangle (
        juce::Rectangle<float> (14, 70, 692, 636), 10.0f);
    g.setColour (uiColour (0.92f));
    g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
    g.drawText ("PARAMETERS - MIDI CC / MANUAL",
                32, 80, 560, 34, juce::Justification::centredLeft);

    for (int k = 0; k < 8; ++k)
    {
        const int index = vongonPanelPage * 8 + k;
        if (index >= count) break;
        const auto& item = items[index];
        const int current = processor.getVongonCc (mode, item.cc);
        const float fraction = current < 0 ? 0.5f
            : (float) current / 127.0f;
        const int col = k % 2;
        const int row = k / 2;
        const juce::Rectangle<float> bounds (
            36.0f + col * 346.0f, 139.0f + row * 112.0f,
            300.0f, 78.0f);
        paintBar (g, bounds,
                  juce::String (item.name) + "  CC#" + juce::String (item.cc),
                  fraction,
                  mode == 8 && item.cc == 15 ? "TAP / TOGGLE"
                  : current < 0 ? "XY / DEFAULT" : juce::String (current));
    }

    g.setColour (uiColour (0.9f));
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    g.drawText ("<", 142, 618, 74, 48, juce::Justification::centred);
    g.drawText ("PAGE " + juce::String (vongonPanelPage + 1)
        + " / " + juce::String (pageCount),
        260, 618, 200, 48, juce::Justification::centred);
    g.drawText (">", 502, 618, 74, 48, juce::Justification::centred);
}

void RealtimeChordFxAudioProcessorEditor::setVongonParameterFromX (
    int mode, int index, float x)
{
    int count = 0;
    const auto* items = getVongonControls (mode, count);
    if (! juce::isPositiveAndBelow (index, count)) return;
    const int k = index % 8;
    const int col = k % 2;
    const float left = 36.0f + col * 346.0f;
    const float norm = juce::jlimit (
        0.0f, 1.0f, (x - left) / 300.0f);
    if (mode == 8 && items[index].cc == 15)
    {
        processor.triggerVongonPolyphrase();
        return;
    }
    processor.setVongonCc (
        mode, items[index].cc, juce::roundToInt (norm * 127.0f));
}

void RealtimeChordFxAudioProcessorEditor::paintHaze (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (currentEurekaFrame.isValid())
    {
        paintMediaFrame (
            g, currentEurekaFrame,
            juce::Rectangle<float> (0.0f, 0.0f, design, design),
            1, dotEurekaRevision);
    }
    paintVideoEffect (g, currentEurekaFrame, eurekaXYX, eurekaXYY);

    juce::ColourGradient topShade (
        juce::Colours::black.withAlpha (0.58f), 360.0f, 0.0f,
        juce::Colours::transparentBlack, 360.0f, 135.0f, false);
    g.setGradientFill (topShade);
    g.fillRect (0.0f, 0.0f, 720.0f, 145.0f);

    g.setColour (uiColour (0.96f));
    g.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
    g.drawText ("EUREKA", 28, 18, 180, 34,
                juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (uiColour (0.72f));
    g.drawText ("DUAL LOOP / GLITCH / LO-FI",
                176, 24, 260, 24, juce::Justification::centredLeft);

    g.setColour (uiColour (0.94f));
    g.drawText (eurekaPanelVisible ? "CLOSE" : "PANEL",
                490, 22, 78, 24, juce::Justification::centred);
    g.drawText ("CONFIG", 604, 22, 88, 24,
                juce::Justification::centredRight);

    if (! eurekaPanelVisible)
    {
        const float stutter =
            processor.state().getRawParameterValue (ParamID::hazeRepeat)->load();
        const float hall =
            processor.state().getRawParameterValue (ParamID::hazeReverb)->load();

        g.setFont (juce::FontOptions (11.0f));
        g.setColour (uiColour (0.72f));
        g.drawText (
            "XY STUTTER " + juce::String (juce::roundToInt (stutter * 100.0f))
            + "%   REVERB " + juce::String (juce::roundToInt (hall * 100.0f)) + "%",
            30, 674, 660, 20, juce::Justification::centred);
        return;
    }

    g.setColour (juce::Colours::black.withAlpha (0.76f));
    g.fillRoundedRectangle (
        juce::Rectangle<float> (18.0f, 72.0f, 684.0f, 612.0f), 12.0f);
    g.setColour (uiColour (0.28f));
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

        g.setColour (uiColour (
            emphasized ? 0.18f : 0.06f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (uiColour (
            emphasized ? 0.94f : 0.46f));
        g.drawRoundedRectangle (
            r, 6.0f, emphasized ? 2.0f : 1.0f);

        g.setFont (juce::FontOptions (10.0f));
        g.setColour (uiColour (0.52f));
        g.drawText (
            toggleNames[i], r.removeFromTop (19.0f),
            juce::Justification::centred);
        g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        g.setColour (uiColour (0.90f));
        g.drawText (values[i], r, juce::Justification::centred);
    }

    g.setFont (juce::FontOptions (10.5f));
    g.setColour (uiColour (0.58f));
    g.drawText (
        "BYPASS keeps recording. LOCK stops record heads. XY controls STUTTER and REVERB.",
        30, 658, 660, 20, juce::Justification::centred);
}

void RealtimeChordFxAudioProcessorEditor::paintChordBot (juce::Graphics& g)
{
    static constexpr const char* roots[] =
        { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    static constexpr const char* qualities[] =
        { "MAJ","MIN","7","MAJ7","MIN7","DIM","SUS2","SUS4" };

    g.fillAll (juce::Colours::black);

    g.setColour (uiColour (0.92f));
    g.setFont (juce::FontOptions (19.0f).withStyle ("Bold"));
    g.drawText ("CHORDBOT", 28, 20, 220, 32,
                juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (uiColour (0.60f));
    g.drawText ("TOP-LEFT = THEORY ROOT",
                250, 24, 210, 24, juce::Justification::centredLeft);

    g.setColour (uiColour (
        chordBotEditMode ? 0.98f : 0.74f));
    g.drawText (chordBotEditMode ? "DONE" : "EDIT",
                500, 22, 72, 26, juce::Justification::centred);
    g.setColour (uiColour (0.84f));
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
            g.setColour (uiColour (0.16f));
            g.fillRoundedRectangle (r, 8.0f);
        }

        g.setColour (uiColour (
            isEditTarget ? 0.98f : 0.48f));
        g.drawRoundedRectangle (
            r.reduced (1.0f), 8.0f,
            isEditTarget || isActive ? 3.0f : 1.2f);

        if (i == 0)
        {
            g.setFont (juce::FontOptions (10.5f).withStyle ("Bold"));
            g.setColour (uiColour (0.58f));
            g.drawText ("ROOT",
                        r.getX() + 12.0f, r.getY() + 10.0f,
                        70.0f, 20.0f, juce::Justification::centredLeft);
        }

        const int root = juce::jlimit (
            0, 11, processor.getChordBotSlotRoot (i));
        const int quality = juce::jlimit (
            0, 7, processor.getChordBotSlotQuality (i));

        g.setColour (uiColour (0.94f));
        g.setFont (juce::FontOptions (36.0f).withStyle ("Bold"));
        g.drawText (roots[root],
                    r.getX() + 12.0f, r.getY() + 64.0f,
                    r.getWidth() - 24.0f, 52.0f,
                    juce::Justification::centred);

        g.setFont (juce::FontOptions (16.0f));
        g.setColour (uiColour (0.72f));
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
        g.setColour (uiColour (0.76f));
        g.drawRoundedRectangle (overlay, 12.0f, 1.5f);

        g.setColour (uiColour (0.94f));
        g.setFont (juce::FontOptions (17.0f).withStyle ("Bold"));
        g.drawText ("EDIT PAD " + juce::String (chordBotEditSlot + 1),
                    140, 202, 440, 30, juce::Justification::centred);

        g.setFont (juce::FontOptions (13.0f));
        g.setColour (uiColour (0.62f));
        g.drawText ("ROOT", 145, 248, 100, 34,
                    juce::Justification::centredLeft);
        g.drawText ("CHORD", 145, 326, 100, 34,
                    juce::Justification::centredLeft);

        g.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
        g.setColour (uiColour (0.92f));
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
        g.setColour (uiColour (0.66f));
        g.drawText ("CANCEL", 380, 454, 160, 48, juce::Justification::centred);

        if (chordBotEditSlot == 0)
        {
            g.setFont (juce::FontOptions (10.5f));
            g.setColour (uiColour (0.50f));
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

juce::String RealtimeChordFxAudioProcessorEditor::currentOutputName() const
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        holder->deviceManager.getAudioDeviceSetup (setup);
        if (setup.outputDeviceName.isNotEmpty())
            return setup.outputDeviceName;

        if (auto* dev = holder->deviceManager.getCurrentAudioDevice())
            return dev->getName();
    }

    return "Unavailable";
}

void RealtimeChordFxAudioProcessorEditor::refreshAudioOutputs()
{
    audioOutputs.clear();
    selectedAudioOutput = -1;

    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;

    auto& dm = holder->deviceManager;
    const auto currentType = dm.getCurrentAudioDeviceType();
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    dm.getAudioDeviceSetup (setup);

    for (auto* type : dm.getAvailableDeviceTypes())
    {
        if (type == nullptr)
            continue;

        type->scanForDevices();
        const auto names = type->getDeviceNames (false);

        for (const auto& name : names)
        {
            // RG Rotate is an Android internal endpoint, not an external route.
            if (name.containsIgnoreCase ("RG Rotate"))
                continue;

            const int index = (int) audioOutputs.size();
            audioOutputs.push_back ({ type->getTypeName(), name });

            if (type->getTypeName() == currentType
                && (name == setup.outputDeviceName
                    || (setup.outputDeviceName.isEmpty()
                        && name == currentOutputName())))
                selectedAudioOutput = index;
        }
    }
}

void RealtimeChordFxAudioProcessorEditor::selectAudioOutput (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) audioOutputs.size()))
        return;

    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;

    auto& dm = holder->deviceManager;
    const auto option = audioOutputs[(size_t) index];

    dm.setCurrentAudioDeviceType (option.type, false);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    dm.getAudioDeviceSetup (setup);
    setup.outputDeviceName = option.name;
    setup.useDefaultOutputChannels = true;

    const auto error = dm.setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty())
    {
        audioDeviceMessage = "OUTPUT FAILED: " + error;
    }
    else
    {
        selectedAudioOutput = index;
        audioDeviceMessage = "OUTPUT: " + option.name;
        holder->getMuteInputValue().setValue (false);

        processor.state().state.setProperty (
            "audioOutputType", option.type, nullptr);
        processor.state().state.setProperty (
            "audioOutputName", option.name, nullptr);

        audioDefaultBufferFrames = -1;
        refreshAudioBufferStatus();
        if (audioBufferMode != 0)
            applyAudioBufferMode (audioBufferMode);
    }

    refreshAudioInputs();
    refreshAudioOutputs();
    refreshAudioBufferStatus();
}

void RealtimeChordFxAudioProcessorEditor::refreshAudioBufferStatus()
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

        if (audioDefaultBufferFrames < 0 && currentFrames > 0)
            audioDefaultBufferFrames = currentFrames;

        audioFramesPerBurst = getAndroidOutputFramesPerBuffer();
        if (audioFramesPerBurst <= 0 && currentFrames > 0)
            audioFramesPerBurst = currentFrames;

        audioActualBufferFrames = currentFrames;
        audioActualSampleRate = currentRate;
    }
#endif
}

juce::String RealtimeChordFxAudioProcessorEditor::makeAudioBufferStatus() const
{
    if (audioActualBufferFrames <= 0 || audioActualSampleRate <= 0.0)
    {
        juce::String unavailable ("Buffer information unavailable");
        if (audioBufferError.isNotEmpty())
            unavailable += "\n" + audioBufferError;
        return unavailable;
    }

    const double ms =
        1000.0 * (double) audioActualBufferFrames / audioActualSampleRate;
    const double bursts = audioFramesPerBurst > 0
        ? (double) audioActualBufferFrames / (double) audioFramesPerBurst
        : 0.0;

    juce::String result =
        juce::String (audioActualBufferFrames) + " frames / "
        + juce::String (ms, 2) + " ms"
        + "\nBASE BURST " + juce::String (audioFramesPerBurst)
        + " / ACTUAL " + juce::String (bursts, 2);

    if (audioBufferError.isNotEmpty())
        result += "\n" + audioBufferError;

    return result;
}

bool RealtimeChordFxAudioProcessorEditor::applyAudioBufferMode (int mode)
{
#if JUCE_ANDROID
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
    {
        audioBufferError = "BUFFER FAILED: standalone holder unavailable";
        return false;
    }

    mode = juce::jlimit (0, 11, mode);
    refreshAudioBufferStatus();

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    holder->deviceManager.getAudioDeviceSetup (setup);

    int targetFrames = audioDefaultBufferFrames;
    if (mode > 0)
    {
        if (audioFramesPerBurst <= 0)
        {
            audioBufferError = "BUFFER FAILED: base burst unavailable";
            return false;
        }

        targetFrames = juce::jmax (
            1, juce::roundToInt (
                (float) audioFramesPerBurst * audioBufferBursts[mode]));

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

    if (targetFrames <= 0)
    {
        audioBufferError = "BUFFER FAILED: default unavailable";
        return false;
    }

    setup.bufferSize = targetFrames;
    const auto error =
        holder->deviceManager.setAudioDeviceSetup (setup, true);

    if (error.isNotEmpty())
    {
        audioBufferError = "BUFFER FAILED: " + error;
        refreshAudioBufferStatus();
        return false;
    }

    audioBufferError.clear();
    audioBufferMode = mode;
    refreshAudioBufferStatus();
    return true;
#else
    juce::ignoreUnused (mode);
    audioBufferError = "AUDIO BUFFER is Android-only";
    return false;
#endif
}

bool RealtimeChordFxAudioProcessorEditor::resetAudioDevice()
{
#if JUCE_ANDROID
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
    {
        audioDeviceMessage = "RESET FAILED: standalone holder unavailable";
        return false;
    }

    juce::AudioDeviceManager::AudioDeviceSetup previousSetup;
    holder->deviceManager.getAudioDeviceSetup (previousSetup);
    const auto previousType =
        holder->deviceManager.getCurrentAudioDeviceType();

    holder->stopPlaying();
    holder->deviceManager.closeAudioDevice();

    auto error =
        holder->deviceManager.initialise (2, 2, nullptr, true);
    if (error.isNotEmpty())
        error = holder->deviceManager.initialise (1, 2, nullptr, true);

    juce::String restoreError;
    if (error.isEmpty())
    {
        if (previousType.isNotEmpty())
            holder->deviceManager.setCurrentAudioDeviceType (
                previousType, false);

        restoreError =
            holder->deviceManager.setAudioDeviceSetup (
                previousSetup, true);
    }

    holder->getMuteInputValue().setValue (false);
    holder->startPlaying();

    refreshAudioInputs();
    refreshAudioOutputs();
    refreshAudioBufferStatus();

    if (error.isNotEmpty())
    {
        audioDeviceMessage = "RESET FAILED: " + error;
        return false;
    }

    if (restoreError.isNotEmpty())
    {
        audioDeviceMessage =
            "RESET OPEN / ROUTE RESTORE FAILED: " + restoreError;
        return false;
    }

    if (audioBufferMode != 0)
        applyAudioBufferMode (audioBufferMode);

    audioDeviceMessage = "AUDIO RESET OK";
    return true;
#else
    audioDeviceMessage = "AUDIO RESET is Android-only";
    return false;
#endif
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

void RealtimeChordFxAudioProcessorEditor::updateInputGainFromX (float x)
{
    const float norm = juce::jlimit (0.0f, 1.0f, (x - 238.0f) / 178.0f);
    const float db = std::round ((-24.0f + 48.0f * norm) * 2.0f) * 0.5f;
    if (auto* gain = processor.state().getParameter (ParamID::inputGainDb))
        gain->setValueNotifyingHost (gain->convertTo0to1 (db));
}

void RealtimeChordFxAudioProcessorEditor::paintConfig (juce::Graphics& g)
{
    if (currentFrame.isValid())
        paintMediaFrame (
            g, currentFrame,
            juce::Rectangle<float> (0, 0, design, design),
            0, dotFrameRevision);
    g.setColour (juce::Colours::black.withAlpha (0.76f));
    g.fillAll();

    g.setColour (uiColour (0.90f));
    g.setFont (juce::FontOptions (20.0f));
    g.drawText ("CONFIG", 34, 26, 180, 32, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("<  PRESET " + juce::String (globalPresetSlot) + "  >",
                230, 30, 126, 24, juce::Justification::centred);
    g.drawText ("SAVE", 370, 30, 72, 24, juce::Justification::centred);
    g.drawText ("LOAD", 458, 30, 72, 24, juce::Justification::centred);
    g.drawText ("CLOSE", 620, 30, 70, 24, juce::Justification::centredRight);

    if (globalPresetMessage.isNotEmpty())
    {
        g.setFont (juce::FontOptions (10.5f));
        g.setColour (uiColour (0.58f));
        g.drawText (globalPresetMessage, 344, 58, 214, 18,
                    juce::Justification::centred);
    }

    g.setFont (juce::FontOptions (13.0f));
    g.setColour (uiColour (0.62f));
    g.drawText ("AUDIO INPUT", 54, 94, 180, 24, juce::Justification::centredLeft);

    const float inputGainDb = processor.state().getRawParameterValue (
        ParamID::inputGainDb)->load();
    const float gainNorm = juce::jlimit (
        0.0f, 1.0f, (inputGainDb + 24.0f) / 48.0f);
    g.setColour (uiColour (0.9f));
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    g.drawText ("INPUT GAIN " + juce::String (inputGainDb, 1) + " dB",
                235, 87, 184, 20, juce::Justification::centred);
    g.setColour (uiColour (0.3f));
    g.fillRect (238.0f, 113.0f, 178.0f, 2.0f);
    g.setColour (uiColour (0.9f));
    g.fillRect (238.0f, 112.0f, 178.0f * gainNorm, 4.0f);
    g.fillRect (238.0f + gainNorm * 178.0f - 3.0f,
                109.0f, 6.0f, 10.0f);
    g.setColour (uiColour (0.82f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("AUDIO SETTINGS  >", 438, 94, 202, 24,
                juce::Justification::centredRight);

    int y = 126;
    const int maxRows = juce::jmin (6, (int) audioInputs.size());
    for (int i = 0; i < maxRows; ++i)
    {
        const bool selected = i == selectedAudioInput;
        g.setColour (uiColour (selected ? 0.92f : 0.68f));
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ((selected ? "●  " : "○  ") + audioInputs[(size_t)i].name,
                    64, y, 520, 34, juce::Justification::centredLeft);
        y += 38;
    }
    if (audioInputs.empty())
    {
        g.setColour (uiColour (0.55f));
        g.drawText ("No selectable input reported by Android/JUCE", 64, y, 520, 34, juce::Justification::centredLeft);
    }

    bool irigDetected = false;
    juce::String irigPhysicalName;
    for (const auto& o : audioInputs)
        if (o.name.containsIgnoreCase ("irig")) { irigDetected = true; irigPhysicalName = o.name; }
    for (const auto& name : physicalInputNames)
        if (name.containsIgnoreCase ("irig")) { irigDetected = true; irigPhysicalName = name; }

    g.setColour (irigDetected ? juce::Colour (0xffb8d9bf) : uiColour (0.55f));
    g.drawText ("iRig Streamer  " + juce::String (irigDetected ? "DETECTED" : "NOT DETECTED"),
                54, 350, 360, 26, juce::Justification::centredLeft);
    g.setColour (uiColour (0.48f));
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
    g.setColour (uiColour (0.25f));
    g.fillRect (54.0f, 423.0f, 420.0f, 2.0f);
    g.setColour (uiColour (0.86f));
    g.fillRect (54.0f, 422.0f, 420.0f * level, 4.0f);
    g.setColour (uiColour (0.58f));
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
        g.setColour (uiColour (0.46f));
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
            g.setColour (uiColour (0.46f));
            g.drawFittedText ("ERR " + lastAudioRouteError,
                              430, 492, 240, 18, juce::Justification::centredRight, 1);
        }
    }

    const int effectMode = juce::jlimit (0, 8, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));
    const int chordMode = juce::jlimit (0, 1, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::chordMode)->load()));
    const bool midiControl = processor.state().getRawParameterValue (ParamID::midiControl)->load() >= 0.5f;
    const int clock = juce::roundToInt (processor.state().getRawParameterValue (ParamID::clockMode)->load());
    const int bpm = juce::roundToInt (processor.state().getRawParameterValue (ParamID::internalBpm)->load());
    const int displayMode = juce::jlimit (0, 1, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::displayMode)->load()));

    g.setColour (uiColour (0.78f));
    g.setFont (juce::FontOptions (16.0f));
    g.drawText ("MODE", 54, 510, 220, 30, juce::Justification::centredLeft);
    const juce::String modeText =
        effectMode == 8 ? "POLYPHRASE"
        : effectMode == 7 ? "SOLARFAZE"
        : effectMode == 6 ? "ROSETONES"
        : effectMode == 5 ? "ULTRASHEER"
        : effectMode == 4 ? "CLOUDS"
        : effectMode == 3 ? "CHORDBOT"
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
    g.setFont (displayMode == 1
        ? juce::FontOptions (15.0f).withStyle ("Bold")
        : juce::FontOptions (15.0f));
    g.setColour (uiColour (displayMode == 1 ? 0.94f : 0.72f));
    g.drawText ("DISPLAY", 54, 674, 220, 28,
                juce::Justification::centredLeft);
    g.drawText (displayMode == 1 ? "DOT" : "MEDIA",
                450, 674, 180, 28,
                juce::Justification::centredRight);
}

void RealtimeChordFxAudioProcessorEditor::paintAudioConfig (juce::Graphics& g)
{
    if (currentFrame.isValid())
        paintMediaFrame (
            g, currentFrame,
            juce::Rectangle<float> (0, 0, design, design),
            0, dotFrameRevision);

    g.setColour (juce::Colours::black.withAlpha (0.84f));
    g.fillAll();

    g.setColour (uiColour (0.94f));
    g.setFont (juce::FontOptions (20.0f).withStyle ("Bold"));
    g.drawText ("AUDIO SETTINGS", 34, 26, 260, 32,
                juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("CLOSE", 620, 30, 70, 24,
                juce::Justification::centredRight);

    g.setColour (uiColour (0.62f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("AUDIO OUTPUT", 54, 104, 180, 24,
                juce::Justification::centredLeft);

    g.setColour (uiColour (0.92f));
    g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
    g.drawText ("<", 54, 136, 44, 42, juce::Justification::centred);
    g.drawFittedText (
        currentOutputName(),
        108, 136, 504, 42,
        juce::Justification::centred, 1);
    g.drawText (">", 622, 136, 44, 42, juce::Justification::centred);

    g.setColour (uiColour (0.62f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("AUDIO BUFFER / BURST", 54, 216, 220, 24,
                juce::Justification::centredLeft);

    g.setColour (uiColour (0.92f));
    g.setFont (juce::FontOptions (17.0f).withStyle ("Bold"));
    g.drawText ("<", 54, 248, 44, 42, juce::Justification::centred);
    g.drawText (
        audioBufferNames[juce::jlimit (0, 11, audioBufferMode)],
        108, 248, 504, 42,
        juce::Justification::centred);
    g.drawText (">", 622, 248, 44, 42, juce::Justification::centred);

    const juce::Rectangle<float> resetBounds (210.0f, 334.0f, 300.0f, 54.0f);
    g.setColour (uiColour (0.10f));
    g.fillRoundedRectangle (resetBounds, 6.0f);
    g.setColour (uiColour (0.86f));
    g.drawRoundedRectangle (resetBounds, 6.0f, 1.4f);
    g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
    g.drawText ("RESET AUDIO", resetBounds, juce::Justification::centred);

    g.setColour (uiColour (0.70f));
    g.setFont (juce::FontOptions (12.5f));
    g.drawFittedText (
        "OUTPUT  " + currentOutputName(),
        54, 430, 612, 28,
        juce::Justification::centredLeft, 1);

    g.setColour (uiColour (0.62f));
    g.setFont (juce::FontOptions (11.5f));
    g.drawFittedText (
        makeAudioBufferStatus(),
        54, 466, 612, 72,
        juce::Justification::topLeft, 3);

    if (audioDeviceMessage.isNotEmpty())
    {
        g.setColour (uiColour (0.82f));
        g.drawFittedText (
            audioDeviceMessage,
            54, 548, 612, 48,
            juce::Justification::centredLeft, 2);
    }

    g.setColour (uiColour (0.46f));
    g.setFont (juce::FontOptions (10.5f));
    g.drawFittedText (
        "If EUREKA crackles, try 2–4 BURSTS. RESET reopens the audio device while keeping the current parameter settings.",
        54, 620, 612, 52,
        juce::Justification::centredLeft, 3);
}

void RealtimeChordFxAudioProcessorEditor::paintMidiControlConfig (juce::Graphics& g)
{
    if (currentFrame.isValid())
        paintMediaFrame (
            g, currentFrame,
            juce::Rectangle<float> (0, 0, design, design),
            0, dotFrameRevision);
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

    g.setColour (uiColour (0.92f));
    g.setFont (juce::FontOptions (20.0f));
    g.drawText ("MIDI CONTROL", 34, 26, 240, 32, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("BACK", 620, 30, 70, 24, juce::Justification::centredRight);

    auto row = [&] (int yy, const juce::String& label, const juce::String& value)
    {
        g.setFont (juce::FontOptions (14.0f));
        g.setColour (uiColour (0.70f));
        g.drawText (label, 54, yy, 250, 30, juce::Justification::centredLeft);
        g.setColour (uiColour (0.92f));
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

    g.setColour (uiColour (0.82f));
    g.drawText ("LOAD", 110, 496, 180, 36, juce::Justification::centred);
    g.drawText ("SAVE", 430, 496, 180, 36, juce::Justification::centred);

    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("X " + juce::String (processor.getMidiControllerX()).paddedLeft ('0', 3)
                + "     Y " + juce::String (processor.getMidiControllerY()).paddedLeft ('0', 3),
                180, 538, 360, 24, juce::Justification::centred);
    g.setFont (juce::FontOptions (11.5f));
    g.setColour (uiColour (0.52f));
    g.drawText ("CHORD OUT sends the generated ChordPlan notes to the selected MIDI OUT.", 54, 570, 620, 20, juce::Justification::centredLeft);
    g.drawText ("L1: WET -5%    R1: WET +5%    96 motion ticks / bar", 54, 592, 610, 20, juce::Justification::centredLeft);
    g.drawText ("X 0..127 / Y 0..127. CLOCK uses 24 PPQN.", 54, 614, 610, 20, juce::Justification::centredLeft);
    if (midiPresetMessage.isNotEmpty())
        g.drawText (midiPresetMessage, 54, 650, 560, 22, juce::Justification::centredLeft);
    g.setColour (uiColour (0.68f));
    g.drawText (
        "MIDI IN (ANDROID AUTO)    XY: CC110 / CC111    MODE: CC112",
        54, 674, 620, 22, juce::Justification::centredLeft);
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

void RealtimeChordFxAudioProcessorEditor::updateCloudsXYPoint (
    juce::Point<float> p, bool touchDown)
{
    const float x = juce::jlimit (0.0f, 1.0f, p.x / design);
    const float y = juce::jlimit (
        0.0f, 1.0f,
        1.0f - (p.y - 70.0f) / (design - 70.0f));
    processor.setCloudsXY (x, y, touchDown);
}

void RealtimeChordFxAudioProcessorEditor::updateVongonXYPoint (
    juce::Point<float> p, bool touchDown)
{
    const float x = juce::jlimit (0.0f, 1.0f, p.x / design);
    const float y = juce::jlimit (
        0.0f, 1.0f,
        1.0f - (p.y - 70.0f) / (design - 70.0f));
    processor.setVongonXY (x, y, touchDown);
}

void RealtimeChordFxAudioProcessorEditor::updateEurekaXYPoint (
    juce::Point<float> p)
{
    eurekaXYX = juce::jlimit (0.0f, 1.0f, p.x / design);
    eurekaXYY = juce::jlimit (0.0f, 1.0f, 1.0f - p.y / design);

    // X controls stutter density; Y moves the active loop position.
    // Reverb grows toward the top-right corner and stays at the release point.
    const float stutter = 0.18f + 0.82f * eurekaXYX;
    const float loopPosition = 0.12f + 0.76f * eurekaXYY;
    const float hall = std::pow (eurekaXYX * eurekaXYY, 1.25f);

    if (auto* repeat = processor.state().getParameter (ParamID::hazeRepeat))
        repeat->setValueNotifyingHost (stutter);
    if (auto* time = processor.state().getParameter (ParamID::hazeTime))
        time->setValueNotifyingHost (loopPosition);
    if (auto* reverb = processor.state().getParameter (ParamID::hazeReverb))
        reverb->setValueNotifyingHost (hall);
}

void RealtimeChordFxAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = toDesign (e.position);
    dragging = DragParam::none;

    if (audioConfigVisible)
    {
        if (p.x >= 590.0f && p.y <= 74.0f)
        {
            audioConfigVisible = false;
            configVisible = true;
            repaint();
            return;
        }

        if (p.y >= 126.0f && p.y < 188.0f)
        {
            refreshAudioOutputs();

            if (! audioOutputs.empty())
            {
                const int direction = p.x < 360.0f ? -1 : 1;
                int index = selectedAudioOutput;

                if (! juce::isPositiveAndBelow (index, (int) audioOutputs.size()))
                    index = direction > 0 ? -1 : 0;

                index = (index + direction + (int) audioOutputs.size())
                      % (int) audioOutputs.size();
                selectAudioOutput (index);
            }
            else
            {
                audioDeviceMessage = "No selectable AUDIO OUTPUT reported by JUCE";
            }

            repaint();
            return;
        }

        if (p.y >= 238.0f && p.y < 300.0f)
        {
            const int direction = p.x < 360.0f ? -1 : 1;
            const int next = juce::jlimit (
                0, 11, audioBufferMode + direction);

            if (next != audioBufferMode)
            {
                if (applyAudioBufferMode (next))
                {
                    if (auto* parameter =
                            processor.state().getParameter (
                                ParamID::audioBufferMode))
                    {
                        parameter->setValueNotifyingHost (
                            parameter->convertTo0to1 ((float) next));
                    }
                    audioDeviceMessage = "BUFFER APPLIED";
                }
                else
                {
                    audioDeviceMessage = "BUFFER REQUEST FAILED";
                }
            }

            repaint();
            return;
        }

        if (juce::Rectangle<float> (
                190.0f, 320.0f, 340.0f, 82.0f).contains (p))
        {
            resetAudioDevice();
            repaint();
            return;
        }

        return;
    }

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
        if (juce::Rectangle<float> (235.0f, 86.0f, 185.0f, 38.0f).contains (p))
        {
            inputGainDragging = true;
            updateInputGainFromX (p.x);
            repaint();
            return;
        }
        if (juce::Rectangle<float> (220.0f, 20.0f, 146.0f, 42.0f).contains (p))
        {
            const int direction = p.x < 293.0f ? -1 : 1;
            globalPresetSlot =
                ((globalPresetSlot - 1 + direction + 3) % 3) + 1;
            globalPresetMessage =
                processor.hasUserPreset (globalPresetSlot)
                    ? "PRESET " + juce::String (globalPresetSlot) + " SAVED"
                    : "PRESET " + juce::String (globalPresetSlot) + " EMPTY";
            repaint();
            return;
        }

        if (juce::Rectangle<float> (360.0f, 20.0f, 86.0f, 42.0f).contains (p))
        {
            globalPresetMessage =
                processor.saveUserPreset (globalPresetSlot)
                    ? "SAVED PRESET " + juce::String (globalPresetSlot)
                    : "SAVE FAILED";
            repaint();
            return;
        }
        if (juce::Rectangle<float> (450.0f, 20.0f, 86.0f, 42.0f).contains (p))
        {
            const bool loaded =
                processor.loadUserPreset (globalPresetSlot);
            globalPresetMessage = loaded
                ? "LOADED PRESET " + juce::String (globalPresetSlot)
                : (processor.hasUserPreset (globalPresetSlot)
                    ? "LOAD FAILED"
                    : "PRESET " + juce::String (globalPresetSlot) + " EMPTY");

            if (loaded)
            {
                audioBufferMode = juce::jlimit (0, 11, juce::roundToInt (
                    processor.state().getRawParameterValue (
                        ParamID::audioBufferMode)->load()));
                applyAudioBufferMode (audioBufferMode);
            }

            repaint();
            return;
        }
        if (p.x >= 590 && p.y <= 74) { configVisible = false; repaint(); return; }

        if (p.y >= 86.0f && p.y < 124.0f && p.x >= 410.0f)
        {
            refreshAudioOutputs();
            refreshAudioBufferStatus();
            audioBufferMode = juce::jlimit (0, 11, juce::roundToInt (
                processor.state().getRawParameterValue (
                    ParamID::audioBufferMode)->load()));
            configVisible = false;
            audioConfigVisible = true;
            repaint();
            return;
        }

        int y = 126;
        const int maxRows = juce::jmin (6, (int) audioInputs.size());
        for (int i = 0; i < maxRows; ++i, y += 38)
            if (p.y >= y && p.y < y + 34) { selectAudioInput (i); repaint(); return; }

        if (p.y >= 508 && p.y < 540)
        {
            const int effect = juce::jlimit (0, 8, juce::roundToInt (
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
            else if (effect == 3)
                setChoiceActual (ParamID::effectMode, 4);      // CHORDBOT -> CLOUDS
            else if (effect == 4)
                setChoiceActual (ParamID::effectMode, 5);      // CLOUDS -> ULTRASHEER
            else if (effect == 5)
                setChoiceActual (ParamID::effectMode, 6);      // ULTRASHEER -> ROSETONES
            else if (effect == 6)
                setChoiceActual (ParamID::effectMode, 7);      // ROSETONES -> SOLARFAZE
            else if (effect == 7)
                setChoiceActual (ParamID::effectMode, 8);      // SOLARFAZE -> POLYPHRASE
            else
            {
                setChoiceActual (ParamID::effectMode, 0);      // POLYPHRASE -> A
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
        else if (p.y >= 670 && p.y < 708)
        {
            const int current = juce::jlimit (0, 1, juce::roundToInt (
                processor.state().getRawParameterValue (ParamID::displayMode)->load()));
            setChoiceActual (ParamID::displayMode, 1 - current);
            dotRenderedRevision = -1;
            dotLumaRevision = -1;
        }
        repaint();
        return;
    }

    const int currentEffectMode = juce::jlimit (0, 8, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));

    if (currentEffectMode != 3
        && juce::Rectangle<float> (390.0f, 16.0f, 76.0f, 38.0f).contains (p))
    {
        const float current = juce::jlimit (
            0.0f, 14.0f,
            processor.state().getRawParameterValue (ParamID::boostDb)->load());
        const float next =
            current < 1.75f ? 3.5f
            : current < 5.25f ? 7.0f
            : current < 8.75f ? 10.5f
            : current < 12.25f ? 14.0f
                               : 0.0f;

        if (auto* boost = processor.state().getParameter (ParamID::boostDb))
            boost->setValueNotifyingHost (boost->convertTo0to1 (next));

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
        refreshAudioOutputs();
        refreshAudioBufferStatus();
        repaint();
        return;
    }

    const int effectMode = juce::jlimit (0, 8, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));

    if (effectMode >= 5 && effectMode <= 8)
    {
        if (p.x >= 485.0f && p.x < 580.0f && p.y <= 70.0f)
        {
            vongonPanelVisible = ! vongonPanelVisible;
            vongonPanelPage = 0;
            vongonDraggingParameter = -1;
            repaint();
            return;
        }
        if (vongonPanelVisible)
        {
            int count = 0;
            getVongonControls (effectMode, count);
            const int pages = (count + 7) / 8;
            if (p.y >= 606.0f)
            {
                if (p.x < 240.0f)
                    vongonPanelPage = (vongonPanelPage + pages - 1) % pages;
                else if (p.x > 472.0f)
                    vongonPanelPage = (vongonPanelPage + 1) % pages;
            }
            else
            {
                for (int k = 0; k < 8; ++k)
                {
                    const int row = k / 2;
                    const int col = k % 2;
                    const int index = vongonPanelPage * 8 + k;
                    if (index >= count) break;
                    if (juce::Rectangle<float> (
                            36.0f + col * 346.0f,
                            139.0f + row * 112.0f,
                            300.0f, 78.0f).contains (p))
                    {
                        vongonDraggingParameter = index;
                        setVongonParameterFromX (effectMode, index, p.x);
                        int controlCount = 0;
                        const auto* controls = getVongonControls (
                            effectMode, controlCount);
                        if (effectMode == 8 && controls[index].cc == 15)
                            vongonDraggingParameter = -1;
                        break;
                    }
                }
            }
            repaint();
            return;
        }
        if (p.y > 70.0f)
        {
            vongonXYTouchDown = true;
            updateVongonXYPoint (p, true);
        }
        repaint();
        return;
    }

    if (effectMode == 4)
    {
        if (p.y > 70.0f)
        {
            cloudsXYTouchDown = true;
            updateCloudsXYPoint (p, true);
        }
        repaint();
        return;
    }

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
            eurekaXYTouchDown = false;
            repaint();
            return;
        }

        if (! eurekaPanelVisible)
        {
            if (p.y > 70.0f)
            {
                eurekaXYTouchDown = true;
                updateEurekaXYPoint (p);
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
                eurekaXYTouchDown = false;
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
    if (configVisible && inputGainDragging)
    {
        updateInputGainFromX (toDesign (e.position).x);
        repaint();
        return;
    }
    if (midiControlConfigVisible || audioConfigVisible || configVisible)
        return;

    const int effectMode = juce::jlimit (0, 8, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));

    if (effectMode >= 5 && effectMode <= 8
        && vongonPanelVisible && vongonDraggingParameter >= 0)
    {
        setVongonParameterFromX (
            effectMode, vongonDraggingParameter, toDesign (e.position).x);
        repaint();
        return;
    }
    if (effectMode >= 5
        && effectMode <= 8
        && vongonXYTouchDown)
    {
        updateVongonXYPoint (toDesign (e.position), true);
        repaint();
        return;
    }

    if (effectMode == 4 && cloudsXYTouchDown)
    {
        updateCloudsXYPoint (toDesign (e.position), true);
        repaint();
        return;
    }

    if (effectMode == 2
        && ! eurekaPanelVisible
        && eurekaXYTouchDown)
    {
        updateEurekaXYPoint (toDesign (e.position));
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
    inputGainDragging = false;
    vongonDraggingParameter = -1;
    eurekaXYTouchDown = false;

    if (cloudsXYTouchDown)
    {
        cloudsXYTouchDown = false;
        processor.setCloudsXY (
            processor.getCloudsX(), processor.getCloudsY(), false);
    }

    if (vongonXYTouchDown)
    {
        vongonXYTouchDown = false;
        processor.setVongonXY (
            processor.getVongonX(), processor.getVongonY(), false);
    }

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
    const int effectMode = juce::jlimit (0, 8, juce::roundToInt (
        processor.state().getRawParameterValue (ParamID::effectMode)->load()));

    if (effectMode == 3)
        return false;

    if (key.getKeyCode() == juce::KeyPress::F17Key)
    {
        if (! l1Latched)
        {
            l1Latched = true;
            if (auto* wet = processor.state().getParameter (ParamID::wet))
                wet->setValueNotifyingHost (
                    juce::jlimit (0.0f, 1.0f, wet->getValue() - 0.05f));
            repaint();
        }
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::F18Key)
    {
        if (! r1Latched)
        {
            r1Latched = true;
            if (auto* wet = processor.state().getParameter (ParamID::wet))
                wet->setValueNotifyingHost (
                    juce::jlimit (0.0f, 1.0f, wet->getValue() + 0.05f));
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
        repaint();
    }
    return false;
}
