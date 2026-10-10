#pragma once
#include <JuceHeader.h>
#include <array>

// Narrow 5x7 technical bitmap face based on the accepted MIYAKO Android #204 UI.
// Used only by FLOWER's optional DOT display mode; no font asset is embedded.
struct FlowerTechnicalFont
{
    static std::array<unsigned char, 5> glyph (juce::juce_wchar letter)
    {
        const auto c = static_cast<char> (juce::CharacterFunctions::toUpperCase (letter));
        switch (c)
        {
            case 'A': return {{0x7e,0x11,0x11,0x11,0x7e}};
            case 'B': return {{0x7f,0x49,0x49,0x49,0x36}};
            case 'C': return {{0x3e,0x41,0x41,0x41,0x22}};
            case 'D': return {{0x7f,0x41,0x41,0x22,0x1c}};
            case 'E': return {{0x7f,0x49,0x49,0x49,0x41}};
            case 'F': return {{0x7f,0x09,0x09,0x09,0x01}};
            case 'G': return {{0x3e,0x41,0x49,0x49,0x7a}};
            case 'H': return {{0x7f,0x08,0x08,0x08,0x7f}};
            case 'I': return {{0x41,0x41,0x7f,0x41,0x41}};
            case 'J': return {{0x20,0x40,0x41,0x3f,0x01}};
            case 'K': return {{0x7f,0x08,0x14,0x22,0x41}};
            case 'L': return {{0x7f,0x40,0x40,0x40,0x40}};
            case 'M': return {{0x7f,0x02,0x0c,0x02,0x7f}};
            case 'N': return {{0x7f,0x04,0x08,0x10,0x7f}};
            case 'O': return {{0x3e,0x41,0x41,0x41,0x3e}};
            case 'P': return {{0x7f,0x09,0x09,0x09,0x06}};
            case 'Q': return {{0x3e,0x41,0x51,0x21,0x5e}};
            case 'R': return {{0x7f,0x09,0x19,0x29,0x46}};
            case 'S': return {{0x46,0x49,0x49,0x49,0x31}};
            case 'T': return {{0x01,0x01,0x7f,0x01,0x01}};
            case 'U': return {{0x3f,0x40,0x40,0x40,0x3f}};
            case 'V': return {{0x1f,0x20,0x40,0x20,0x1f}};
            case 'W': return {{0x7f,0x20,0x18,0x20,0x7f}};
            case 'X': return {{0x63,0x14,0x08,0x14,0x63}};
            case 'Y': return {{0x03,0x04,0x78,0x04,0x03}};
            case 'Z': return {{0x61,0x51,0x49,0x45,0x43}};
            case '0': return {{0x3e,0x51,0x49,0x45,0x3e}};
            case '1': return {{0x00,0x42,0x7f,0x40,0x00}};
            case '2': return {{0x62,0x51,0x49,0x49,0x46}};
            case '3': return {{0x22,0x41,0x49,0x49,0x36}};
            case '4': return {{0x18,0x14,0x12,0x7f,0x10}};
            case '5': return {{0x27,0x45,0x45,0x45,0x39}};
            case '6': return {{0x3e,0x49,0x49,0x49,0x32}};
            case '7': return {{0x01,0x71,0x09,0x05,0x03}};
            case '8': return {{0x36,0x49,0x49,0x49,0x36}};
            case '9': return {{0x26,0x49,0x49,0x49,0x3e}};
            case '/': return {{0x40,0x20,0x10,0x08,0x04}};
            case '-': return {{0x08,0x08,0x08,0x08,0x08}};
            case '_': return {{0x40,0x40,0x40,0x40,0x40}};
            case '.': return {{0x00,0x60,0x60,0x00,0x00}};
            case ',': return {{0x00,0x80,0x60,0x00,0x00}};
            case ':': return {{0x00,0x36,0x36,0x00,0x00}};
            case '+': return {{0x08,0x08,0x3e,0x08,0x08}};
            case '%': return {{0x63,0x13,0x08,0x64,0x63}};
            case '=': return {{0x14,0x14,0x14,0x14,0x14}};
            case '#': return {{0x14,0x7f,0x14,0x7f,0x14}};
            case '>': return {{0x41,0x22,0x14,0x08,0x00}};
            case '<': return {{0x08,0x14,0x22,0x41,0x00}};
            case '?': return {{0x02,0x01,0x51,0x09,0x06}};
            case ' ': return {{0,0,0,0,0}};
            default: return {{0x41,0x41,0x41,0x41,0x41}};
        }
    }

    static void draw (juce::Graphics& g, const juce::String& value,
                      juce::Rectangle<int> bounds, juce::Colour ink,
                      juce::Justification alignment = juce::Justification::centred)
    {
        if (bounds.isEmpty() || value.isEmpty()) return;
        const int length = value.length();
        const int px = juce::jlimit (
            1, 3,
            juce::jmin (juce::jmax (1, bounds.getHeight() / 11),
                        bounds.getWidth() / juce::jmax (1, length * 6)));
        const int py = juce::jlimit (
            px, px * 2,
            juce::jmax (px, (bounds.getHeight() - 2) / 7));
        const int w = length * 6 * px - px;
        const int h = 7 * py;
        int x = bounds.getX() + (bounds.getWidth() - w) / 2;
        if (alignment.testFlags (juce::Justification::left)) x = bounds.getX() + 2;
        if (alignment.testFlags (juce::Justification::right)) x = bounds.getRight() - w - 2;
        const int y = bounds.getY() + (bounds.getHeight() - h) / 2;

        g.saveState();
        g.reduceClipRegion (bounds);
        g.setColour (ink);
        for (int i = 0; i < length; ++i)
        {
            const auto columns = glyph (value[i]);
            for (int column = 0; column < 5; ++column)
                for (int bit = 0; bit < 7; ++bit)
                    if ((columns[static_cast<size_t> (column)] >> bit) & 1)
                        g.fillRect (x + (i * 6 + column) * px,
                                    y + bit * py, px, py);
        }
        g.restoreState();
    }
};
