#include "PluginEditor.h"
#include "ParameterIDs.h"
#include "BinaryData.h"

#if JUCE_ANDROID
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

#include <cmath>

LabelledKnob::LabelledKnob (juce::String name)
{
    control.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
   #if JUCE_ANDROID
    control.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 15);
   #else
    control.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 17);
   #endif
    addAndMakeVisible (control);

    label.setText (std::move (name), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
   #if JUCE_ANDROID
    label.setFont (juce::FontOptions (8.5f).withStyle ("Bold"));
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
    label.setBounds (area.removeFromTop (11));
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
    auto area = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (juce::Colour (0x33000000));
    g.fillRoundedRectangle (area.translated (1.5f, 2.0f), 4.0f);

    g.setColour (juce::Colour (0xffded4b6));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (juce::Colour (0xff8e846c));
    g.drawRoundedRectangle (area, 4.0f, 1.0f);

    auto rocker = area.reduced (9.0f, 7.0f);
    juce::ColourGradient face (on ? juce::Colour (0xfff5edd7) : juce::Colour (0xffc8bea5),
                               rocker.getX(), rocker.getY(),
                               on ? juce::Colour (0xffc8bea5) : juce::Colour (0xfff5edd7),
                               rocker.getX(), rocker.getBottom(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (rocker, 2.0f);
    g.setColour (juce::Colour (0xff746b59));
    g.drawRoundedRectangle (rocker, 2.0f, 1.0f);

    if (isMouseOverButton || isButtonDown)
    {
        g.setColour (juce::Colour (0x16000000));
        g.fillRoundedRectangle (rocker, 2.0f);
    }

    g.setColour (on ? juce::Colour (0xffb3443f) : juce::Colour (0xff777064));
    const auto led = juce::Rectangle<float> (5.0f, 5.0f)
                         .withCentre ({ area.getRight() - 8.0f, area.getY() + 8.0f });
    g.fillEllipse (led);

    if (namedStateStyle)
    {
        auto textArea = rocker.toNearestInt().reduced (2, 0);
        auto nameArea = textArea.removeFromTop (juce::jmax (8, textArea.getHeight() / 2));

        g.setColour (juce::Colour (0xff273249));
        g.setFont (juce::FontOptions ("Comic Sans MS", 7.2f, juce::Font::bold));
        g.drawFittedText (getButtonText(), nameArea, juce::Justification::centred, 1);

        g.setColour (on ? juce::Colour (0xffa33b36) : juce::Colour (0xff777064));
        g.setFont (juce::FontOptions ("Comic Sans MS", 7.8f, juce::Font::bold));
        g.drawFittedText (on ? "ON" : "OFF", textArea, juce::Justification::centred, 1);
    }
    else
    {
        g.setColour (juce::Colour (0xff273249));
        g.setFont (juce::FontOptions ("Comic Sans MS", 8.0f, juce::Font::bold));
        g.drawFittedText (on ? "ON" : "OFF", rocker.toNearestInt(), juce::Justification::centred, 1);
    }
}

void FlowerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1713));

    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff7c6d52));
    g.drawRoundedRectangle (bounds, 12.0f, 1.2f);

    g.setColour (juce::Colour (0xffddd0a5));
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    g.drawText ("FLOWER", getLocalBounds().removeFromTop (34).reduced (12, 0),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff91866e));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("LOOPER / GRANULAR",
                getLocalBounds().removeFromTop (34).reduced (94, 0),
                juce::Justification::centredLeft);
}

void SynthPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1713));
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff7c6d52));
    g.drawRoundedRectangle (bounds, 12.0f, 1.2f);

    g.setColour (juce::Colour (0xffddd0a5));
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    g.drawText ("SYNTH", getLocalBounds().removeFromTop (34).reduced (12, 0),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff91866e));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("SINE / ADSR / FILTER / LFO",
                getLocalBounds().removeFromTop (34).reduced (78, 0),
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
    auto bounds = getLocalBounds().reduced (8);
    g.setColour (juce::Colour (0xff080808));
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    g.setColour (juce::Colour (0xff8e8e89));
    g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);

    auto graph = bounds.reduced (10, 13);
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
        const float radius = 3.0f + static_cast<float> (i % 2);
        g.setColour (juce::Colour (0xffbcbcb7).withAlpha (0.92f - i * 0.12f));
        g.fillEllipse (gx - radius, graph.getY() + 5.0f + i * 7.0f,
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
    g.setFont (juce::FontOptions (8.5f));
    g.drawText ("SIZE " + juce::String (sizeAmount * 1000.0f, 0) + " ms",
                bounds.removeFromBottom (13), juce::Justification::centredRight);
}

void FlowerWaveformComponent::mouseDown (const juce::MouseEvent& e)
{
    mouseDrag (e);
}

void FlowerWaveformComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! onPositionChanged)
        return;

    auto graph = getLocalBounds().reduced (8).reduced (10, 13);
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

    int targetWidth = 900;
    int targetHeight = 405;

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        const int longSide = juce::jmax (area.getWidth(), area.getHeight());
        const int shortSide = juce::jmin (area.getWidth(), area.getHeight());

        if (longSide > 0 && shortSide > 0)
        {
            targetWidth = juce::jlimit (640, 1100, longSide);
            targetHeight = juce::jlimit (300, 560,
                juce::roundToInt (static_cast<float> (targetWidth) * shortSide / longSide));
        }
    }

    setSize (targetWidth, targetHeight);
    setResizable (true, true);
    setResizeLimits (640, 300, 1400, 700);

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
    flowerPanel.setBounds (getLocalBounds().reduced (28, 22));

    auto flower = flowerPanel.getLocalBounds().reduced (14);
    auto flowerHeader = flower.removeFromTop (44);

    synthButton.setBounds (flowerHeader.removeFromRight (66).reduced (2, 7));
    flowerClear.setBounds (flowerHeader.removeFromRight (64).reduced (2, 7));
    flowerReverse.setBounds (flowerHeader.removeFromRight (82).reduced (2, 5));
    flowerOn.setBounds (flowerHeader.removeFromRight (74).reduced (2, 5));

    flower.removeFromTop (5);

    const float layoutAspect = static_cast<float> (juce::jmax (1, getWidth()))
                             / static_cast<float> (juce::jmax (1, getHeight()));
    const bool stackedLayout = layoutAspect < 1.45f;

    juce::Rectangle<int> flowerControls;
    juce::Rectangle<int> flowerVisuals;

    if (stackedLayout)
    {
        const int controlsHeight = juce::jlimit (
            168, 240,
            juce::roundToInt (static_cast<float> (flower.getHeight()) * 0.38f));
        flowerControls = flower.removeFromBottom (controlsHeight);
        flower.removeFromBottom (6);
        flowerVisuals = flower.reduced (4, 2);

        const int waveHeight = juce::jlimit (
            62, 110,
            juce::roundToInt (static_cast<float> (flowerVisuals.getHeight()) * 0.28f));
        auto waveformArea = flowerVisuals.removeFromBottom (waveHeight);
        flowerVisuals.removeFromBottom (5);
        flowerAnimation.setBounds (flowerVisuals.reduced (2));
        flowerWaveform.setBounds (waveformArea.reduced (2));
    }
    else
    {
        flowerControls = flower.removeFromRight (juce::jmax (240, flower.getWidth() / 3));
        flowerVisuals = flower.reduced (5);

        auto animationArea = flowerVisuals.removeFromTop (
            juce::roundToInt (static_cast<float> (flowerVisuals.getHeight()) * 0.59f));
        flowerAnimation.setBounds (animationArea.reduced (2));
        flowerVisuals.removeFromTop (6);
        flowerWaveform.setBounds (flowerVisuals.reduced (2));
    }

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
