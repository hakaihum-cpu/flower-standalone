#include "RetroLookAndFeel.h"

RetroLookAndFeel::RetroLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd8cfad));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff171511));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff544c3c));
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffe0d6b2));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff171511));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff6b6049));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff181611));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe0d6b2));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff4b4334));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void RetroLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPosProportional,
                                         float rotaryStartAngle,
                                         float rotaryEndAngle,
                                         juce::Slider&)
{
    const auto diameter = static_cast<float> (juce::jmin (width, height)) - 8.0f;
    const auto radius = diameter * 0.5f;
    const auto centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const auto centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const auto bounds = juce::Rectangle<float> (centreX - radius, centreY - radius, diameter, diameter);

    g.setColour (juce::Colour (0x50000000));
    g.fillEllipse (bounds.translated (2.0f, 3.0f));

    juce::ColourGradient knobGradient (juce::Colour (0xff5d594f), bounds.getX(), bounds.getY(),
                                       juce::Colour (0xff1b1a17), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (knobGradient);
    g.fillEllipse (bounds);
    g.setColour (juce::Colour (0xff93896f));
    g.drawEllipse (bounds, 1.3f);

    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -radius * 0.76f, 3.0f, radius * 0.48f, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (juce::Colour (0xffe2c674));
    g.fillPath (pointer);
    g.setColour (juce::Colour (0x55ffffff));
    g.drawEllipse (bounds.reduced (4.0f), 0.8f);
}

void RetroLookAndFeel::drawComboBox (juce::Graphics& g,
                                     int width, int height,
                                     bool,
                                     int buttonX, int buttonY,
                                     int buttonW, int buttonH,
                                     juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    juce::Path arrow;
    const float cx = buttonX + buttonW * 0.5f;
    const float cy = buttonY + buttonH * 0.5f;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (juce::Colour (0xffd8cfad));
    g.fillPath (arrow);
}
