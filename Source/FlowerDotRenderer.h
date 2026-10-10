#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include "FlowerTechnicalFont.h"

// Optional FLOWER DOT presentation layer.
// It consumes the already-selected still/video frame and never owns asset
// selection, random timing, MIDI, audio processing, or transport state.
class FlowerDotRenderer
{
public:
    static constexpr int sourceWidth = 512;
    static constexpr int sourceHeight = 384;

    void reset()
    {
        lastSourceKey = -1;
        sourceValid = false;
        sourceRevision = 0;
        renderedRevision = -1;
        renderedTick = -1;
        displayCache = {};
    }

    void paint (juce::Graphics& g,
                juce::Rectangle<int> bounds,
                const juce::Image& source,
                int sourceKey,
                float phaseSeconds,
                bool arpOn,
                bool delayOn,
                bool yEffectOn,
                bool dreamyMode,
                float bpm,
                bool bpmVisible)
    {
        if (! source.isValid() || bounds.isEmpty())
            return;

        if (sourceKey != lastSourceKey || ! sourceValid)
            setSourceImage (source, sourceKey);

        const int stateMask =
            (arpOn ? 1 : 0)
            | (delayOn ? 2 : 0)
            | (yEffectOn ? 4 : 0)
            | (dreamyMode ? 8 : 0);

        if (stateMask != lastStateMask)
        {
            lastStateMask = stateMask;
            ++sourceRevision;
        }

        const auto dark = juce::Colour (0xff100e08);
        const auto mid = juce::Colour (0xff80671e);
        const auto amber = juce::Colour (0xffffdb46);
        const auto bright = juce::Colour (0xffffe57a);

        g.fillAll (dark);

        const auto inner = bounds.reduced (8);
        auto available = inner.withTrimmedTop (46).withTrimmedBottom (46);
        const int previewW = juce::jmax (1, available.getWidth());
        const int previewH = juce::jmax (
            1, juce::jmin (available.getHeight(), previewW * 3 / 4));
        const int previewY =
            available.getY() + (available.getHeight() - previewH) / 2;
        const juce::Rectangle<int> preview (
            available.getX(), previewY, previewW, previewH);

        const int w = juce::jmin (sourceWidth, juce::jmax (1, preview.getWidth()));
        const int h = juce::jmin (sourceHeight, juce::jmax (1, preview.getHeight()));
        const bool newSize =
            ! displayCache.isValid()
            || displayCache.getWidth() != w
            || displayCache.getHeight() != h;

        if (newSize)
        {
            displayCache = juce::Image (juce::Image::RGB, w, h, true);
            renderedRevision = -1;
        }

        const int tick = juce::roundToInt (phaseSeconds * 24.0f);
        const float t = static_cast<float> (tick);

        // These are visual-only mappings from FLOWER's existing effect state.
        // No new user parameter or audio behaviour is introduced.
        const float jitter = yEffectOn && ! dreamyMode ? 0.18f : 0.0f;
        const float slice = yEffectOn && ! dreamyMode ? 0.14f : 0.0f;
        const float roll = yEffectOn && ! dreamyMode ? 0.05f : 0.0f;
        const float zoomAmount = yEffectOn && dreamyMode ? 0.14f : 0.0f;
        const float flicker = yEffectOn && dreamyMode ? 0.06f : 0.0f;
        const float ghost = juce::jlimit (
            0.0f, 1.0f,
            (delayOn ? 0.26f : 0.0f)
            + (yEffectOn && dreamyMode ? 0.10f : 0.0f));

        const bool animated =
            jitter > 0.01f || slice > 0.01f || roll > 0.01f
            || zoomAmount > 0.01f || flicker > 0.01f;

        if (renderedRevision != sourceRevision
            || (animated && renderedTick != tick))
        {
            const float zoom = 1.0f + zoomAmount * 0.2f
                * (0.5f + 0.5f * std::sin (t * 0.08f));
            const float rollOffset =
                roll * 0.08f * std::sin (t * 0.025f);
            const float light = flicker > 0.0f
                ? 1.0f
                    - flicker * (0.12f + 0.19f * std::sin (t * 0.2f))
                : 1.0f;

            // MIYAKO Android #204 ordered-dither matrix and four amber levels.
            static constexpr int bayer4[16] {
                0, 8, 2, 10,
                12, 4, 14, 6,
                3, 11, 1, 9,
                15, 7, 13, 5
            };

            juce::Image::BitmapData target (
                displayCache, juce::Image::BitmapData::readWrite);

            for (int y = 0; y < h; ++y)
            {
                const bool sliced =
                    slice > 0.01f
                    && ((y * 11 + tick / 7) % 29)
                        < static_cast<int> (slice * 6.0f);
                const float fy0 =
                    static_cast<float> (y) / juce::jmax (1, h - 1);
                const int rowJitter = static_cast<int> (
                    jitter * 4.0f * std::sin (y * 7.0f + t * 0.39f));

                for (int x = 0; x < w; ++x)
                {
                    float fx =
                        (static_cast<float> (x) / juce::jmax (1, w - 1) - 0.5f)
                        / zoom + 0.5f;
                    float fy = (fy0 - 0.5f) / zoom + 0.5f;

                    fx += (rowJitter
                           + (sliced ? static_cast<int> (slice * 9.0f) : 0))
                        / static_cast<float> (juce::jmax (1, w));
                    fy += rollOffset;

                    const int sx = juce::jlimit (
                        0, sourceWidth - 1,
                        static_cast<int> (fx * static_cast<float> (sourceWidth)));
                    const int sy = juce::jlimit (
                        0, sourceHeight - 1,
                        static_cast<int> (fy * static_cast<float> (sourceHeight)));

                    float luminance = sourceValid
                        ? sourceLuma[static_cast<size_t> (
                            sy * sourceWidth + sx)]
                        : 0.0f;

                    if (ghost > 0.01f)
                    {
                        const int ghostX = juce::jlimit (
                            0, sourceWidth - 1,
                            sx - 1 - static_cast<int> (ghost * 7.0f));
                        luminance = juce::jmax (
                            luminance,
                            ghost * 0.6f
                                * sourceLuma[static_cast<size_t> (
                                    sy * sourceWidth + ghostX)]);
                    }

                    luminance *= light;

                    const float threshold =
                        0.13f
                        + 0.63f
                            * static_cast<float> (
                                bayer4[(x & 3) + ((y & 3) << 2)])
                            / 16.0f;

                    target.setPixelColour (
                        x, y,
                        luminance < threshold ? dark
                        : luminance > 0.88f ? bright
                        : luminance > 0.63f ? amber
                        : mid);
                }
            }

            renderedRevision = sourceRevision;
            renderedTick = tick;
        }

        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (
            displayCache,
            static_cast<float> (preview.getX()),
            static_cast<float> (preview.getY()),
            static_cast<float> (preview.getWidth()),
            static_cast<float> (preview.getHeight()),
            0, 0, displayCache.getWidth(), displayCache.getHeight());

        g.setColour (mid);
        g.drawRect (inner, 1);
        g.setColour (amber);
        g.drawRect (preview.expanded (2), 1);

        FlowerTechnicalFont::draw (
            g, "FLOWER / DOT",
            { inner.getX() + 4, inner.getY() + 2,
              inner.getWidth() * 2 / 5, 34 },
            amber, juce::Justification::centredLeft);

        juce::StringArray effects;
        if (arpOn) effects.add ("ARP");
        if (delayOn) effects.add ("DLY");
        if (yEffectOn) effects.add (dreamyMode ? "DRM" : "GRN");
        if (effects.isEmpty()) effects.add ("DRY");

        FlowerTechnicalFont::draw (
            g, effects.joinIntoString (" "),
            { inner.getX() + inner.getWidth() * 2 / 5,
              inner.getY() + 2,
              inner.getWidth() * 3 / 5 - 4, 34 },
            bright, juce::Justification::centredRight);

        FlowerTechnicalFont::draw (
            g, "SELECT CONFIG",
            { inner.getX() + 4, inner.getBottom() - 36,
              inner.getWidth() / 2 - 6, 30 },
            mid, juce::Justification::centredLeft);

        const juce::String rightText = bpmVisible
            ? "BPM " + juce::String (juce::roundToInt (bpm))
            : "XY PERFORMANCE";

        FlowerTechnicalFont::draw (
            g, rightText,
            { inner.getCentreX(),
              inner.getBottom() - 36,
              inner.getWidth() / 2 - 4, 30 },
            bpmVisible ? bright : mid,
            juce::Justification::centredRight);
    }

private:
    void setSourceImage (const juce::Image& source, int sourceKey)
    {
        juce::Image small (
            juce::Image::RGB, sourceWidth, sourceHeight, true);
        juce::Graphics downsampler (small);
        downsampler.fillAll (juce::Colours::black);
        downsampler.setImageResamplingQuality (
            juce::Graphics::mediumResamplingQuality);
        downsampler.drawImage (
            source,
            0, 0, sourceWidth, sourceHeight,
            0, 0, source.getWidth(), source.getHeight());

        juce::Image::BitmapData data (
            small, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < sourceHeight; ++y)
            for (int x = 0; x < sourceWidth; ++x)
            {
                const auto colour = data.getPixelColour (x, y);
                sourceLuma[static_cast<size_t> (
                    y * sourceWidth + x)] =
                    0.2126f * colour.getFloatRed()
                    + 0.7152f * colour.getFloatGreen()
                    + 0.0722f * colour.getFloatBlue();
            }

        lastSourceKey = sourceKey;
        sourceValid = true;
        ++sourceRevision;
    }

    std::array<float, sourceWidth * sourceHeight> sourceLuma {};
    juce::Image displayCache;
    int lastSourceKey = -1;
    int lastStateMask = -1;
    int sourceRevision = 0;
    int renderedRevision = -1;
    int renderedTick = -1;
    bool sourceValid = false;
};
