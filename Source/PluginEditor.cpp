#include "PluginEditor.h"
#include "ParameterIDs.h"
#include "BinaryData.h"

#if JUCE_ANDROID
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
 #define STBI_ONLY_PNG
 #define STBI_NO_STDIO
 #define STB_IMAGE_IMPLEMENTATION
 #include "third_party/stb_image.h"
#endif

#include <cmath>
#include <limits>

#if JUCE_ANDROID
namespace
{
constexpr float androidReferencePixels = 720.0f;

float androidReferenceScale()
{
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        if (display->scale > 0.0)
            return static_cast<float> (1.0 / display->scale);

    return 1.0f;
}

int androidMetric (float referencePixels)
{
    return juce::jmax (1, juce::roundToInt (referencePixels * androidReferenceScale()));
}

juce::Image decodeAndroidPngWithStb (const void* data, size_t size)
{
    if (data == nullptr || size == 0 || size > static_cast<size_t> (std::numeric_limits<int>::max()))
        return {};

    int width = 0;
    int height = 0;
    int channels = 0;
    auto* rgba = stbi_load_from_memory (
        static_cast<const stbi_uc*> (data),
        static_cast<int> (size),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha);

    if (rgba == nullptr || width <= 0 || height <= 0)
    {
        if (rgba != nullptr)
            stbi_image_free (rgba);
        return {};
    }

    juce::Image image (juce::Image::ARGB, width, height, true);
    juce::Image::BitmapData bitmap (image, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < height; ++y)
    {
        auto* line = bitmap.getLinePointer (y);
        const auto* src = rgba + static_cast<size_t> (y) * static_cast<size_t> (width) * 4u;

        for (int x = 0; x < width; ++x)
        {
            auto* pixel = reinterpret_cast<juce::PixelARGB*> (
                line + static_cast<ptrdiff_t> (x) * bitmap.pixelStride);
            const auto* p = src + static_cast<size_t> (x) * 4u;
            pixel->setARGB (p[3], p[0], p[1], p[2]);
            pixel->premultiply();
        }
    }

    stbi_image_free (rgba);
    return image;
}
}
#endif

LabelledKnob::LabelledKnob (juce::String name)
{
    control.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
   #if JUCE_ANDROID
    control.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
   #else
    control.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 17);
   #endif
    addAndMakeVisible (control);

    label.setText (std::move (name), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
   #if JUCE_ANDROID
    label.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
   #else
    label.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
   #endif
    label.setColour (juce::Label::textColourId, juce::Colour (0xffd6cba5));
    addAndMakeVisible (label);
}

void LabelledKnob::resized()
{
    auto area = getLocalBounds();
   #if JUCE_ANDROID
    const auto scale = androidReferenceScale();
    label.setFont (juce::FontOptions (juce::jmax (5.0f, 11.0f * scale)).withStyle ("Bold"));
    control.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                             androidMetric (72.0f), androidMetric (20.0f));
    label.setBounds (area.removeFromTop (androidMetric (16.0f)));
   #else
    label.setBounds (area.removeFromTop (15));
   #endif
    control.setBounds (area);
}

RetroToggleSwitch::RetroToggleSwitch (const juce::String& name)
    : juce::Button (name)
{
    setClickingTogglesState (true);
    setTooltip (name);
}

void RetroToggleSwitch::paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const bool on = getToggleState();
   #if JUCE_ANDROID
    const float ui = androidReferenceScale();
   #else
    constexpr float ui = 1.0f;
   #endif
    auto area = getLocalBounds().toFloat().reduced (2.0f * ui);

    g.setColour (juce::Colour (0x33000000));
    g.fillRoundedRectangle (area.translated (1.5f * ui, 2.0f * ui), 4.0f * ui);

    g.setColour (juce::Colour (0xffded4b6));
    g.fillRoundedRectangle (area, 4.0f * ui);
    g.setColour (juce::Colour (0xff8e846c));
    g.drawRoundedRectangle (area, 4.0f * ui, 1.0f * ui);

    auto rocker = area.reduced (9.0f * ui, 7.0f * ui);
    juce::ColourGradient face (on ? juce::Colour (0xfff5edd7) : juce::Colour (0xffc8bea5),
                               rocker.getX(), rocker.getY(),
                               on ? juce::Colour (0xffc8bea5) : juce::Colour (0xfff5edd7),
                               rocker.getX(), rocker.getBottom(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (rocker, 2.0f * ui);
    g.setColour (juce::Colour (0xff746b59));
    g.drawRoundedRectangle (rocker, 2.0f * ui, 1.0f * ui);

    if (isMouseOverButton || isButtonDown)
    {
        g.setColour (juce::Colour (0x16000000));
        g.fillRoundedRectangle (rocker, 2.0f * ui);
    }

    g.setColour (on ? juce::Colour (0xffb3443f) : juce::Colour (0xff777064));
    const auto led = juce::Rectangle<float> (5.0f * ui, 5.0f * ui)
                         .withCentre ({ area.getRight() - 8.0f * ui, area.getY() + 8.0f * ui });
    g.fillEllipse (led);

    if (namedStateStyle)
    {
        auto textArea = rocker.toNearestInt().reduced (2, 0);
        auto nameArea = textArea.removeFromTop (juce::jmax (8, textArea.getHeight() / 2));

        g.setColour (juce::Colour (0xff273249));
        g.setFont (juce::FontOptions ("Comic Sans MS", juce::jmax (4.0f, 7.2f * ui), juce::Font::bold));
        g.drawFittedText (getButtonText(), nameArea, juce::Justification::centred, 1);

        g.setColour (on ? juce::Colour (0xffa33b36) : juce::Colour (0xff777064));
        g.setFont (juce::FontOptions ("Comic Sans MS", juce::jmax (4.0f, 7.8f * ui), juce::Font::bold));
        g.drawFittedText (on ? "ON" : "OFF", textArea, juce::Justification::centred, 1);
    }
    else
    {
        g.setColour (juce::Colour (0xff273249));
        g.setFont (juce::FontOptions ("Comic Sans MS", juce::jmax (4.0f, 8.0f * ui), juce::Font::bold));
        g.drawFittedText (on ? "ON" : "OFF", rocker.toNearestInt(), juce::Justification::centred, 1);
    }
}

void FlowerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1713));

    #if JUCE_ANDROID
    const float ui = androidReferenceScale();
   #else
    constexpr float ui = 1.0f;
   #endif
    auto bounds = getLocalBounds().toFloat().reduced (1.0f * ui);
    g.setColour (juce::Colour (0xff7c6d52));
    g.drawRoundedRectangle (bounds, 12.0f * ui, 1.2f * ui);

    g.setColour (juce::Colour (0xffddd0a5));
    g.setFont (juce::FontOptions (juce::jmax (7.0f, 18.0f * ui)).withStyle ("Bold"));
    g.drawText ("FLOWER", getLocalBounds().removeFromTop (juce::roundToInt (34.0f * ui)).reduced (juce::roundToInt (12.0f * ui), 0),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff91866e));
    g.setFont (juce::FontOptions (juce::jmax (5.0f, 10.0f * ui)));
    g.drawText ("LOOPER / GRANULAR",
                getLocalBounds().removeFromTop (juce::roundToInt (34.0f * ui)).reduced (juce::roundToInt (94.0f * ui), 0),
                juce::Justification::centredLeft);
}

void SynthPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1713));
    #if JUCE_ANDROID
    const float ui = androidReferenceScale();
   #else
    constexpr float ui = 1.0f;
   #endif
    auto bounds = getLocalBounds().toFloat().reduced (1.0f * ui);
    g.setColour (juce::Colour (0xff7c6d52));
    g.drawRoundedRectangle (bounds, 12.0f * ui, 1.2f * ui);

    g.setColour (juce::Colour (0xffddd0a5));
    g.setFont (juce::FontOptions (juce::jmax (7.0f, 18.0f * ui)).withStyle ("Bold"));
    g.drawText ("SYNTH", getLocalBounds().removeFromTop (juce::roundToInt (34.0f * ui)).reduced (juce::roundToInt (12.0f * ui), 0),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff91866e));
    g.setFont (juce::FontOptions (juce::jmax (5.0f, 10.0f * ui)));
    g.drawText ("SINE / ADSR / FILTER / LFO",
                getLocalBounds().removeFromTop (juce::roundToInt (34.0f * ui)).reduced (juce::roundToInt (78.0f * ui), 0),
                juce::Justification::centredLeft);
}

void FlowerWaveformComponent::setState (
    const std::array<float, FlowerStandaloneAudioProcessor::flowerWaveformBins>& newWaveform,
    float validFraction,
    float recordProgress,
    bool isRecording,
    float basePosition,
    const std::array<float, FlowerStandaloneAudioProcessor::flowerGrainCount>& grainPositions,
    int activeGrains,
    float spread,
    float grainSize)
{
    waveform = newWaveform;
    grains = grainPositions;
    valid = juce::jlimit (0.0f, 1.0f, validFraction);
    progress = juce::jlimit (0.0f, 1.0f, recordProgress);
    recording = isRecording;
    position = juce::jlimit (0.0f, 1.0f, basePosition);
    grainCount = juce::jlimit (0, FlowerStandaloneAudioProcessor::flowerGrainCount, activeGrains);
    spreadAmount = juce::jlimit (0.0f, 1.0f, spread);
    sizeAmount = juce::jlimit (0.008f, 0.50f, grainSize);
    repaint();
}

void FlowerWaveformComponent::paint (juce::Graphics& g)
{
   #if JUCE_ANDROID
    const float ui = androidReferenceScale();
   #else
    constexpr float ui = 1.0f;
   #endif
    auto bounds = getLocalBounds().reduced (juce::roundToInt (8.0f * ui));
    g.setColour (juce::Colour (0xff080808));
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f * ui);
    g.setColour (juce::Colour (0xff8e8e89));
    g.drawRoundedRectangle (bounds.toFloat(), 4.0f * ui, 1.0f * ui);

    auto graph = bounds.reduced (juce::roundToInt (10.0f * ui), juce::roundToInt (13.0f * ui));
    const int centreY = graph.getCentreY();

    g.setColour (juce::Colour (0xff3b3b38));
    g.drawHorizontalLine (centreY, static_cast<float> (graph.getX()),
                          static_cast<float> (graph.getRight()));

    const int validBins = juce::jlimit (
        1, FlowerStandaloneAudioProcessor::flowerWaveformBins,
        juce::roundToInt (juce::jmax (
            valid, 1.0f / FlowerStandaloneAudioProcessor::flowerWaveformBins)
            * FlowerStandaloneAudioProcessor::flowerWaveformBins));

    juce::Path upper;
    juce::Path lower;

    for (int x = 0; x < graph.getWidth(); ++x)
    {
        const float nx = graph.getWidth() > 1
            ? static_cast<float> (x) / static_cast<float> (graph.getWidth() - 1)
            : 0.0f;
        const int bin = juce::jlimit (
            0, validBins - 1,
            juce::roundToInt (nx * static_cast<float> (validBins - 1)));
        const float magnitude = juce::jlimit (
            0.0f, 1.0f, waveform[static_cast<size_t> (bin)]);
        const float xPos = static_cast<float> (graph.getX() + x);
        const float topY = static_cast<float> (centreY)
            - magnitude * static_cast<float> (graph.getHeight()) * 0.43f;
        const float bottomY = static_cast<float> (centreY)
            + magnitude * static_cast<float> (graph.getHeight()) * 0.43f;

        if (x == 0)
        {
            upper.startNewSubPath (xPos, topY);
            lower.startNewSubPath (xPos, bottomY);
        }
        else
        {
            upper.lineTo (xPos, topY);
            lower.lineTo (xPos, bottomY);
        }
    }

    g.setColour (juce::Colour (0xffd9d9d3));
    g.strokePath (upper, juce::PathStrokeType (1.3f));
    g.setColour (juce::Colour (0xff777774));
    g.strokePath (lower, juce::PathStrokeType (0.8f));

    const float spreadHalf = spreadAmount * 0.5f;
    const float spreadStart = juce::jlimit (0.0f, 1.0f, position - spreadHalf);
    const float spreadEnd = juce::jlimit (0.0f, 1.0f, position + spreadHalf);
    const float sx = graph.getX() + spreadStart * graph.getWidth();
    const float ex = graph.getX() + spreadEnd * graph.getWidth();
    g.setColour (juce::Colour (0x22ffffff));
    g.fillRect (juce::Rectangle<float> (
        sx, static_cast<float> (graph.getY()),
        juce::jmax (1.0f, ex - sx), static_cast<float> (graph.getHeight())));

    const float px = graph.getX() + position * graph.getWidth();
    g.setColour (juce::Colour (0xfff0f0e9));
    g.drawVerticalLine (juce::roundToInt (px),
                        static_cast<float> (graph.getY()),
                        static_cast<float> (graph.getBottom()));

    for (int i = 0; i < grainCount; ++i)
    {
        const float gx = graph.getX()
            + juce::jlimit (0.0f, 1.0f, grains[static_cast<size_t> (i)]) * graph.getWidth();
        const float radius = (3.0f + static_cast<float> (i % 2)) * ui;
        g.setColour (juce::Colour (0xffbcbcb7).withAlpha (0.92f - i * 0.12f));
        g.fillEllipse (gx - radius, graph.getY() + 5.0f * ui + i * 7.0f * ui,
                       radius * 2.0f, radius * 2.0f);
    }

    if (recording)
    {
        const float rx = graph.getX() + progress * graph.getWidth();
        g.setColour (juce::Colour (0xffd6d6cf));
        g.drawVerticalLine (juce::roundToInt (rx),
                            static_cast<float> (graph.getY()),
                            static_cast<float> (graph.getBottom()));
    }

    g.setColour (juce::Colour (0xff6b6b67));
    g.setFont (juce::FontOptions (juce::jmax (4.5f, 8.5f * ui)));
    g.drawText ("SIZE " + juce::String (sizeAmount * 1000.0f, 0) + " ms",
                bounds.removeFromBottom (juce::roundToInt (13.0f * ui)), juce::Justification::centredRight);
}

void FlowerWaveformComponent::mouseDown (const juce::MouseEvent& e)
{
    mouseDrag (e);
}

void FlowerWaveformComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! onPositionChanged)
        return;

   #if JUCE_ANDROID
    const auto ui = androidReferenceScale();
    auto graph = getLocalBounds()
                     .reduced (juce::roundToInt (8.0f * ui))
                     .reduced (juce::roundToInt (10.0f * ui),
                               juce::roundToInt (13.0f * ui));
   #else
    auto graph = getLocalBounds().reduced (8).reduced (10, 13);
   #endif
    if (graph.getWidth() <= 0)
        return;

    const float normalized = juce::jlimit (
        0.0f, 1.0f,
        static_cast<float> (e.x - graph.getX()) / static_cast<float> (graph.getWidth()));
    onPositionChanged (normalized);
}

void FlowerStandaloneAudioProcessorEditor::configureSlider (
    LabelledKnob& knob, double min, double max, double step)
{
    knob.slider().setRange (min, max, step);
}

void FlowerStandaloneAudioProcessorEditor::setParameterNormalized (
    juce::RangedAudioParameter* parameter, float normalized)
{
    if (parameter == nullptr)
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
    parameter->endChangeGesture();
}

FlowerStandaloneAudioProcessorEditor::FlowerStandaloneAudioProcessorEditor (
    FlowerStandaloneAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      keyboard (processor.getKeyboardState(),
                juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&retroLookAndFeel);
    setOpaque (true);

   #if JUCE_ANDROID
    // Keep the standalone editor inside the actual Android display bounds.
    // This mirrors the proven MIYAKO Android startup path rather than keeping
    // the desktop-only fixed 960x720 editor size.
    // Do not force orientation during startup. On the target 720x720 Android
    // device this can tear down the JUCE activity window before it is drawn.

    // 720x720 is the physical-pixel target. JUCE component bounds are logical
    // pixels, so forcing 720 logical pixels overflows high-density 720x720
    // Android displays. Convert the physical target through Display::scale and
    // let the fullscreen standalone wrapper supply the final logical bounds.
    int logicalCanvasSide = juce::roundToInt (720.0 / juce::jmax (0.1, juce::Desktop::getInstance()
        .getDisplays().getPrimaryDisplay() != nullptr
            ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale
            : 1.0));

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const int availableSide = juce::roundToInt (
            juce::jmin (display->userBounds.getWidth(), display->userBounds.getHeight()));

        if (availableSide > 0)
            logicalCanvasSide = juce::jmin (logicalCanvasSide, availableSide);
    }

    logicalCanvasSide = juce::jmax (1, logicalCanvasSide);
    setSize (logicalCanvasSide, logicalCanvasSide);
    setResizable (false, false);

    // MIYAKO's proven Android standalone path explicitly discards any device
    // setup chosen by the generic standalone holder and reopens the default
    // Android stereo output as 0-in / 2-out. Keep the sequence identical.
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        holder->stopPlaying();
        holder->deviceManager.closeAudioDevice();

        const auto audioError =
            holder->deviceManager.initialise (0, 2, nullptr, true);

        if (audioError.isNotEmpty())
            juce::Logger::writeToLog (
                "FLOWER Android audio initialise failed: " + audioError);

        holder->startPlaying();
    }
   #else
    setSize (960, 720);
   #endif

    addAndMakeVisible (flowerPanel);
    flowerPanel.addAndMakeVisible (flowerWaveform);
    flowerPanel.addAndMakeVisible (flowerAnimation);
    flowerPanel.addAndMakeVisible (flowerOn);
    flowerPanel.addAndMakeVisible (flowerReverse);
    flowerPanel.addAndMakeVisible (flowerClear);
    flowerPanel.addAndMakeVisible (synthButton);

    flowerOn.setNamedStateStyle (true);
    flowerReverse.setNamedStateStyle (true);
    flowerOn.setTooltip ("FLOWER EFFECT ON / OFF");
    flowerReverse.setTooltip ("REVERSE PLAYBACK ON / OFF");

    const auto addFlowerKnob = [this] (LabelledKnob& knob)
    {
        flowerPanel.addAndMakeVisible (knob);
    };
    addFlowerKnob (flowerPosition);
    addFlowerKnob (flowerSize);
    addFlowerKnob (flowerDensity);
    addFlowerKnob (flowerSpread);
    addFlowerKnob (flowerHold);
    addFlowerKnob (flowerPitch);
    addFlowerKnob (flowerMix);
    addFlowerKnob (flowerFeedback);

    addAndMakeVisible (synthPanel);
    synthPanel.setVisible (false);
    synthPanel.addAndMakeVisible (closeSynthButton);
    synthPanel.addAndMakeVisible (synthLevel);
    synthPanel.addAndMakeVisible (synthAttack);
    synthPanel.addAndMakeVisible (synthDecay);
    synthPanel.addAndMakeVisible (synthSustain);
    synthPanel.addAndMakeVisible (synthRelease);
    synthPanel.addAndMakeVisible (synthCutoff);
    synthPanel.addAndMakeVisible (synthResonance);
    synthPanel.addAndMakeVisible (synthLfoRate);
    synthPanel.addAndMakeVisible (synthLfoDepth);
    synthPanel.addAndMakeVisible (synthLfoTarget);
    synthPanel.addAndMakeVisible (synthLfoTargetLabel);
    synthPanel.addAndMakeVisible (keyboard);

    synthLfoTargetLabel.setText ("LFO TARGET", juce::dontSendNotification);
    synthLfoTargetLabel.setJustificationType (juce::Justification::centred);
    synthLfoTargetLabel.setFont (juce::FontOptions (9.0f).withStyle ("Bold"));
    synthLfoTargetLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd6cba5));
    synthLfoTarget.addItemList (
        juce::StringArray { "OFF", "PITCH", "CUTOFF", "RESONANCE", "LEVEL" }, 1);

    configureSlider (synthLevel, 0.0, 1.0, 0.001);
    configureSlider (synthAttack, 0.001, 5.0, 0.001);
    configureSlider (synthDecay, 0.001, 5.0, 0.001);
    configureSlider (synthSustain, 0.0, 1.0, 0.001);
    configureSlider (synthRelease, 0.001, 8.0, 0.001);
    configureSlider (synthCutoff, 20.0, 20000.0, 1.0);
    synthCutoff.slider().setSkewFactorFromMidPoint (3000.0);
    configureSlider (synthResonance, 0.1, 12.0, 0.001);
    configureSlider (synthLfoRate, 0.01, 20.0, 0.001);
    synthLfoRate.slider().setSkewFactorFromMidPoint (1.0);
    configureSlider (synthLfoDepth, 0.0, 1.0, 0.001);

    configureSlider (flowerPosition, 0.0, 1.0, 0.001);
    configureSlider (flowerSize, 0.008, 0.50, 0.001);
    flowerSize.slider().setSkewFactorFromMidPoint (0.08);
    configureSlider (flowerDensity, 0.0, 1.0, 0.001);
    configureSlider (flowerSpread, 0.0, 1.0, 0.001);
    configureSlider (flowerHold, 0.0, 1.0, 0.001);
    configureSlider (flowerPitch, -12.0, 12.0, 0.01);
    configureSlider (flowerMix, 0.0, 1.0, 0.001);
    configureSlider (flowerFeedback, 0.0, 1.0, 0.001);

    auto& state = processor.getAPVTS();
    const auto attachSlider = [&] (LabelledKnob& knob, const char* id)
    {
        sliderAttachments.push_back (
            std::make_unique<SliderAttachment> (state, id, knob.slider()));
    };
    const auto attachButton = [&] (juce::Button& button, const char* id)
    {
        buttonAttachments.push_back (
            std::make_unique<ButtonAttachment> (state, id, button));
    };

    attachSlider (flowerPosition, ParamIDs::flowerPosition);
    attachSlider (flowerSize, ParamIDs::flowerSize);
    attachSlider (flowerDensity, ParamIDs::flowerDensity);
    attachSlider (flowerSpread, ParamIDs::flowerSpread);
    attachSlider (flowerHold, ParamIDs::flowerHold);
    attachSlider (flowerPitch, ParamIDs::flowerPitch);
    attachSlider (flowerMix, ParamIDs::flowerMix);
    attachSlider (flowerFeedback, ParamIDs::flowerFeedback);
    attachButton (flowerOn, ParamIDs::flowerEnabled);
    attachButton (flowerReverse, ParamIDs::flowerReverse);

    attachSlider (synthLevel, ParamIDs::level);
    attachSlider (synthAttack, ParamIDs::attack);
    attachSlider (synthDecay, ParamIDs::decay);
    attachSlider (synthSustain, ParamIDs::sustain);
    attachSlider (synthRelease, ParamIDs::release);
    attachSlider (synthCutoff, ParamIDs::cutoff);
    attachSlider (synthResonance, ParamIDs::resonance);
    attachSlider (synthLfoRate, ParamIDs::lfoRate);
    attachSlider (synthLfoDepth, ParamIDs::lfoDepth);
    comboAttachments.push_back (
        std::make_unique<ComboAttachment> (state, ParamIDs::lfoTarget, synthLfoTarget));

    flowerClear.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff302a22));
    flowerClear.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd6cba5));
    synthButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff302a22));
    synthButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd6cba5));
    closeSynthButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff302a22));
    closeSynthButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd6cba5));

    flowerClear.onClick = [this] { processor.clearFlowerLoop(); };
    synthButton.onClick = [this] { showSynth (true); };
    closeSynthButton.onClick = [this] { showSynth (false); };

    flowerWaveform.onPositionChanged = [this] (float position)
    {
        if (auto* parameter = processor.getAPVTS().getParameter (ParamIDs::flowerPosition))
            setParameterNormalized (parameter, position);
    };

   #if ! JUCE_ANDROID
    const bool atlasLoaded = flowerAnimation.loadEmbeddedAtlas (
        BinaryData::flower_embedded_atlas_png,
        static_cast<size_t> (BinaryData::flower_embedded_atlas_pngSize));

    if (atlasLoaded)
    {
        flowerAnimation.loadHighResWalkStrip (
            BinaryData::flower_actor_v3_walk_student01_png,
            static_cast<size_t> (BinaryData::flower_actor_v3_walk_student01_pngSize),
            0, true);
    }
   #endif

    startTimerHz (20);
}

FlowerStandaloneAudioProcessorEditor::~FlowerStandaloneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FlowerStandaloneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff080807));
}

void FlowerStandaloneAudioProcessorEditor::showSynth (bool shouldShow)
{
    synthPanel.setVisible (shouldShow);
    if (shouldShow)
        synthPanel.toFront (true);
}

void FlowerStandaloneAudioProcessorEditor::resized()
{
   #if JUCE_ANDROID
    // Layout is authored against a 720x720 physical-pixel reference, then
    // converted to JUCE logical pixels. This keeps the complete interface
    // visible on high-density 720x720 Android panels.
    const float ui = juce::jmax (0.1f,
        static_cast<float> (juce::jmin (getWidth(), getHeight())) / androidReferencePixels);
    const auto px = [ui] (float value)
    {
        return juce::jmax (1, juce::roundToInt (value * ui));
    };

    const int referenceSide = px (720.0f);
    auto square = getLocalBounds().withSizeKeepingCentre (referenceSide, referenceSide);
    flowerPanel.setBounds (square.reduced (px (8.0f)));

    auto flower = flowerPanel.getLocalBounds().reduced (px (14.0f));
    auto flowerHeader = flower.removeFromTop (px (48.0f));

    const auto placeHeaderButton = [&flowerHeader, &px] (juce::Component& button, float width)
    {
        button.setBounds (flowerHeader.removeFromRight (px (width))
                                      .reduced (px (3.0f), px (7.0f)));
    };

    placeHeaderButton (synthButton, 78.0f);
    placeHeaderButton (flowerClear, 74.0f);
    placeHeaderButton (flowerReverse, 92.0f);
    placeHeaderButton (flowerOn, 82.0f);

    flower.removeFromTop (px (6.0f));

    auto animationArea = flower.removeFromTop (px (336.0f));
    flowerAnimation.setBounds (animationArea.reduced (px (2.0f)));

    flower.removeFromTop (px (6.0f));
    auto waveformArea = flower.removeFromTop (px (84.0f));
    flowerWaveform.setBounds (waveformArea.reduced (px (2.0f)));

    flower.removeFromTop (px (8.0f));
    auto flowerControls = flower.reduced (px (2.0f), 0);

    const int flowerRowH = juce::jmax (1, flowerControls.getHeight() / 2);
    auto flowerRow1 = flowerControls.removeFromTop (flowerRowH);
    auto flowerRow2 = flowerControls;

    const auto placeFour = [&px] (juce::Rectangle<int> row,
                                  LabelledKnob& a, LabelledKnob& b,
                                  LabelledKnob& c, LabelledKnob& d)
    {
        const int quarter = juce::jmax (1, row.getWidth() / 4);
        a.setBounds (row.removeFromLeft (quarter).reduced (px (5.0f), px (3.0f)));
        b.setBounds (row.removeFromLeft (quarter).reduced (px (5.0f), px (3.0f)));
        c.setBounds (row.removeFromLeft (quarter).reduced (px (5.0f), px (3.0f)));
        d.setBounds (row.reduced (px (5.0f), px (3.0f)));
    };

    placeFour (flowerRow1,
               flowerPosition, flowerSize, flowerDensity, flowerSpread);
    placeFour (flowerRow2,
               flowerHold, flowerPitch, flowerMix, flowerFeedback);

    synthPanel.setBounds (flowerPanel.getBounds());
    auto synth = synthPanel.getLocalBounds().reduced (px (14.0f));
    auto synthHeader = synth.removeFromTop (px (48.0f));
    closeSynthButton.setBounds (
        synthHeader.removeFromRight (px (78.0f)).reduced (px (3.0f), px (7.0f)));

    synth.removeFromTop (px (8.0f));
    auto keyboardArea = synth.removeFromBottom (px (118.0f));
    keyboard.setBounds (keyboardArea.reduced (px (4.0f), px (6.0f)));
    synth.removeFromBottom (px (8.0f));

    const int rowH = juce::jmax (1, synth.getHeight() / 3);
    auto row1 = synth.removeFromTop (rowH);
    auto row2 = synth.removeFromTop (rowH);
    auto row3 = synth;

    const auto placeThree = [&px] (juce::Rectangle<int> row,
                                   LabelledKnob& a, LabelledKnob& b, LabelledKnob& c)
    {
        const int third = juce::jmax (1, row.getWidth() / 3);
        a.setBounds (row.removeFromLeft (third).reduced (px (7.0f), px (5.0f)));
        b.setBounds (row.removeFromLeft (third).reduced (px (7.0f), px (5.0f)));
        c.setBounds (row.reduced (px (7.0f), px (5.0f)));
    };

    placeThree (row1, synthLevel, synthAttack, synthDecay);
    placeThree (row2, synthSustain, synthRelease, synthCutoff);

    const int third = juce::jmax (1, row3.getWidth() / 3);
    synthResonance.setBounds (row3.removeFromLeft (third).reduced (px (7.0f), px (5.0f)));
    synthLfoRate.setBounds (row3.removeFromLeft (third).reduced (px (7.0f), px (5.0f)));

    auto lfoCell = row3.reduced (px (7.0f), px (5.0f));
    auto lfoTop = lfoCell.removeFromTop (juce::roundToInt (lfoCell.getHeight() * 0.66f));
    synthLfoDepth.setBounds (lfoTop);
    synthLfoTargetLabel.setFont (
        juce::FontOptions (juce::jmax (4.5f, 9.0f * ui)).withStyle ("Bold"));
    synthLfoTargetLabel.setBounds (lfoCell.removeFromTop (px (18.0f)));
    synthLfoTarget.setBounds (lfoCell.reduced (px (3.0f), px (2.0f)));
   #else
    flowerPanel.setBounds (getLocalBounds().reduced (28, 22));

    auto flower = flowerPanel.getLocalBounds().reduced (14);
    auto flowerHeader = flower.removeFromTop (44);

    synthButton.setBounds (flowerHeader.removeFromRight (66).reduced (2, 7));
    flowerClear.setBounds (flowerHeader.removeFromRight (64).reduced (2, 7));
    flowerReverse.setBounds (flowerHeader.removeFromRight (82).reduced (2, 5));
    flowerOn.setBounds (flowerHeader.removeFromRight (74).reduced (2, 5));

    flower.removeFromTop (5);

    auto flowerControls = flower.removeFromRight (juce::jmax (240, flower.getWidth() / 3));
    auto flowerVisuals = flower.reduced (5);

    auto animationArea = flowerVisuals.removeFromTop (
        juce::roundToInt (static_cast<float> (flowerVisuals.getHeight()) * 0.59f));
    flowerAnimation.setBounds (animationArea.reduced (2));
    flowerVisuals.removeFromTop (6);
    flowerWaveform.setBounds (flowerVisuals.reduced (2));

    flowerControls = flowerControls.reduced (8, 2);
    const int flowerRowH = juce::jmax (1, flowerControls.getHeight() / 4);

    const auto placePair = [] (juce::Rectangle<int> row, LabelledKnob& a, LabelledKnob& b)
    {
        const int half = row.getWidth() / 2;
        a.setBounds (row.removeFromLeft (half).reduced (3));
        b.setBounds (row.reduced (3));
    };

    placePair (flowerControls.removeFromTop (flowerRowH), flowerPosition, flowerSize);
    placePair (flowerControls.removeFromTop (flowerRowH), flowerDensity, flowerSpread);
    placePair (flowerControls.removeFromTop (flowerRowH), flowerHold, flowerPitch);
    placePair (flowerControls, flowerMix, flowerFeedback);

    synthPanel.setBounds (flowerPanel.getBounds());
    auto synth = synthPanel.getLocalBounds().reduced (14);
    auto synthHeader = synth.removeFromTop (44);
    closeSynthButton.setBounds (synthHeader.removeFromRight (64).reduced (2, 7));
    synth.removeFromTop (8);

    auto keyboardArea = synth.removeFromBottom (juce::jlimit (72, 120, synth.getHeight() / 4));
    keyboard.setBounds (keyboardArea.reduced (4, 6));
    synth.removeFromBottom (8);

    const int rowH = juce::jmax (1, synth.getHeight() / 3);
    auto row1 = synth.removeFromTop (rowH);
    auto row2 = synth.removeFromTop (rowH);
    auto row3 = synth;

    const auto placeThree = [] (juce::Rectangle<int> row,
                                LabelledKnob& a, LabelledKnob& b, LabelledKnob& c)
    {
        const int third = juce::jmax (1, row.getWidth() / 3);
        a.setBounds (row.removeFromLeft (third).reduced (4));
        b.setBounds (row.removeFromLeft (third).reduced (4));
        c.setBounds (row.reduced (4));
    };

    placeThree (row1, synthLevel, synthAttack, synthDecay);
    placeThree (row2, synthSustain, synthRelease, synthCutoff);

    const int third = juce::jmax (1, row3.getWidth() / 3);
    synthResonance.setBounds (row3.removeFromLeft (third).reduced (4));
    synthLfoRate.setBounds (row3.removeFromLeft (third).reduced (4));

    auto lfoCell = row3.reduced (4);
    auto lfoTop = lfoCell.removeFromTop (lfoCell.getHeight() * 2 / 3);
    synthLfoDepth.setBounds (lfoTop);
    synthLfoTargetLabel.setBounds (lfoCell.removeFromTop (14));
    synthLfoTarget.setBounds (lfoCell.reduced (3, 1));
   #endif
}

void FlowerStandaloneAudioProcessorEditor::timerCallback()
{
   #if JUCE_ANDROID
    // Let StandaloneFilterWindow finish attaching a visible Android window
    // before doing any PNG decoding on the message thread. This avoids both
    // the pre-window startup stall and the unsafe detached-thread decoder path.
    if (! androidVisualLoadAttempted)
    {
        ++androidStartupTicks;

        if (androidStartupTicks >= 4 && isShowing())
        {
            androidVisualLoadAttempted = true;

            const auto atlas = decodeAndroidPngWithStb (
                BinaryData::flower_embedded_atlas_png,
                static_cast<size_t> (BinaryData::flower_embedded_atlas_pngSize));
            const bool atlasLoaded = flowerAnimation.loadDecodedAtlas (atlas);

            if (atlasLoaded)
            {
                const auto walkStrip = decodeAndroidPngWithStb (
                    BinaryData::flower_actor_v3_walk_student01_png,
                    static_cast<size_t> (BinaryData::flower_actor_v3_walk_student01_pngSize));
                flowerAnimation.loadDecodedHighResWalkStrip (walkStrip, 0, true);
            }
        }
    }
   #endif

    std::array<float, FlowerStandaloneAudioProcessor::flowerWaveformBins> waveform {};
    std::array<float, FlowerStandaloneAudioProcessor::flowerGrainCount> grainPositions {};

    processor.getFlowerWaveform (waveform);
    for (int i = 0; i < FlowerStandaloneAudioProcessor::flowerGrainCount; ++i)
        grainPositions[static_cast<size_t> (i)] = processor.getFlowerGrainPosition (i);

    auto& state = processor.getAPVTS();
    const float spread = state.getRawParameterValue (ParamIDs::flowerSpread)->load();
    const float size = state.getRawParameterValue (ParamIDs::flowerSize)->load();
    const float density = state.getRawParameterValue (ParamIDs::flowerDensity)->load();
    const float hold = state.getRawParameterValue (ParamIDs::flowerHold)->load();
    const float pitch = state.getRawParameterValue (ParamIDs::flowerPitch)->load();
    const float mix = state.getRawParameterValue (ParamIDs::flowerMix)->load();
    const float feedback = state.getRawParameterValue (ParamIDs::flowerFeedback)->load();
    const bool reverse = state.getRawParameterValue (ParamIDs::flowerReverse)->load() > 0.5f;

    flowerWaveform.setState (
        waveform,
        processor.getFlowerLoopValidFraction(),
        processor.getFlowerRecordProgress(),
        processor.isFlowerRecording(),
        processor.getFlowerBasePosition(),
        grainPositions,
        processor.getFlowerActiveGrains(),
        spread,
        size);

    flowerAnimation.setState (
        density,
        processor.getFlowerBasePosition(),
        spread,
        hold,
        size,
        pitch,
        mix,
        feedback,
        reverse,
        processor.isFlowerRecording(),
        processor.getFlowerRecordProgress(),
        processor.hasFlowerLoop());
}
