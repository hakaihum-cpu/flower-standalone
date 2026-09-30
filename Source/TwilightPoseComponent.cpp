#include "TwilightPoseComponent.h"
#include "TwilightPoseData.h"

#include <cmath>

namespace
{
constexpr int designSize = 720;
constexpr int atlasCellWidth = 64;
constexpr int atlasCellHeight = 128;
constexpr int atlasColumns = 8;

constexpr int walkFrames[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
constexpr int startStopFrames[8] = { 8, 9, 10, 11, 12, 13, 14, 15 };
constexpr int turnFrames[8] = { 16, 17, 18, 19, 20, 21, 22, 23 };
constexpr int poseFrames[6] = { 24, 25, 26, 27, 28, 29 };

float easeInOut (float x)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    return x * x * (3.0f - 2.0f * x);
}
}

TwilightPoseComponent::TwilightPoseComponent()
{
    setOpaque (true);
    setWantsKeyboardFocus (false);

    loadEmbeddedVisuals();

    fpsWindowStartMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (16);
}

TwilightPoseComponent::~TwilightPoseComponent()
{
    stopTimer();
}

juce::MemoryBlock TwilightPoseComponent::decodeCompressedChunks (
    const char* const* chunks,
    int chunkCount,
    int expectedSize)
{
    juce::String encoded;

    for (int i = 0; i < chunkCount; ++i)
        encoded += chunks[i];

    juce::MemoryOutputStream compressedOut;
    if (! juce::Base64::convertFromBase64 (compressedOut, encoded))
        return {};

    const auto compressed = compressedOut.getMemoryBlock();
    juce::MemoryInputStream source (
        compressed.getData(), compressed.getSize(), false);

    juce::GZIPDecompressorInputStream decompressor (
        &source,
        false,
        juce::GZIPDecompressorInputStream::gzipFormat,
        expectedSize);

    juce::MemoryBlock decoded;
    decompressor.readIntoMemoryBlock (decoded, expectedSize);

    return decoded;
}

void TwilightPoseComponent::loadEmbeddedVisuals()
{
    const auto atlasBytes = decodeCompressedChunks (
        TwilightPoseData::atlasChunks,
        TwilightPoseData::atlasChunkCount,
        TwilightPoseData::atlasDecodedSize);

    const auto backgroundBytes = decodeCompressedChunks (
        TwilightPoseData::backgroundChunks,
        TwilightPoseData::backgroundChunkCount,
        TwilightPoseData::backgroundDecodedSize);

    if (atlasBytes.getSize() != static_cast<size_t> (TwilightPoseData::atlasDecodedSize)
        || backgroundBytes.getSize() != static_cast<size_t> (TwilightPoseData::backgroundDecodedSize))
    {
        visualsReady = false;
        return;
    }

    poseAtlas = juce::Image (
        juce::Image::ARGB,
        TwilightPoseData::atlasWidth,
        TwilightPoseData::atlasHeight,
        true);

    {
        juce::Image::BitmapData pixels (
            poseAtlas,
            juce::Image::BitmapData::writeOnly);

        const auto* packed =
            static_cast<const unsigned char*> (atlasBytes.getData());

        for (int y = 0; y < TwilightPoseData::atlasHeight; ++y)
        {
            for (int x = 0; x < TwilightPoseData::atlasWidth; ++x)
            {
                const auto value =
                    packed[y * TwilightPoseData::atlasWidth + x];

                const auto grey =
                    static_cast<juce::uint8> ((value >> 4) * 17);
                const auto alpha =
                    static_cast<juce::uint8> ((value & 0x0f) * 17);

                pixels.setPixelColour (
                    x, y,
                    juce::Colour (grey, grey, grey, alpha));
            }
        }
    }

    rooftopBackground = juce::Image (
        juce::Image::RGB,
        TwilightPoseData::backgroundWidth,
        TwilightPoseData::backgroundHeight,
        true);

    {
        juce::Image::BitmapData pixels (
            rooftopBackground,
            juce::Image::BitmapData::writeOnly);

        const auto* values =
            static_cast<const unsigned char*> (backgroundBytes.getData());

        for (int y = 0; y < TwilightPoseData::backgroundHeight; ++y)
        {
            for (int x = 0; x < TwilightPoseData::backgroundWidth; ++x)
            {
                const auto grey = static_cast<juce::uint8> (
                    values[y * TwilightPoseData::backgroundWidth + x] * 17);

                pixels.setPixelColour (
                    x, y,
                    juce::Colour (grey, grey, grey));
            }
        }
    }

    visualsReady = poseAtlas.isValid() && rooftopBackground.isValid();
}

TwilightPoseComponent::FrameState TwilightPoseComponent::getFrameState() const
{
    // Timeline is intentionally authored around the supplied pose board.
    // The actor uses the source poses rather than interpolated 3D geometry.
    constexpr int waitTicks = 18;
    constexpr int startTicks = 8;
    constexpr int walkTicks = 64;
    constexpr int stopTicks = 12;
    constexpr int turnTicks = 24;
    constexpr int backHoldTicks = 18;
    constexpr int fadeTicks = 8;
    constexpr int poseHoldTicks = 20;
    constexpr int lieHoldTicks = 34;

    constexpr int poseSectionTicks =
        poseHoldTicks * 5 + lieHoldTicks;

    constexpr int totalTicks =
        waitTicks
        + startTicks
        + walkTicks
        + stopTicks
        + backHoldTicks
        + turnTicks
        + fadeTicks
        + poseSectionTicks
        + fadeTicks;

    int t = tick % totalTicks;

    FrameState s;
    s.actorY = 612.0f;

    if (t < waitTicks)
    {
        s.atlasIndex = startStopFrames[0];
        s.actorX = 105.0f;
        s.label = "WAIT";
        return s;
    }
    t -= waitTicks;

    if (t < startTicks)
    {
        const int frame = juce::jlimit (0, 3, t / 2);
        s.atlasIndex = startStopFrames[frame];
        s.actorX = 105.0f + static_cast<float> (t) * 3.0f;
        s.label = "WALK START";
        return s;
    }
    t -= startTicks;

    if (t < walkTicks)
    {
        const int frame = (t / 1) % 8;
        const float p = static_cast<float> (t) / static_cast<float> (walkTicks - 1);

        s.atlasIndex = walkFrames[frame];
        s.actorX = juce::jmap (easeInOut (p), 128.0f, 526.0f);
        s.label = "WALK CYCLE";
        return s;
    }
    t -= walkTicks;

    if (t < stopTicks)
    {
        const int frame = juce::jlimit (4, 7, 4 + t / 3);
        s.atlasIndex = startStopFrames[frame];
        s.actorX = 526.0f + static_cast<float> (t) * 1.0f;
        s.label = "WALK STOP";
        return s;
    }
    t -= stopTicks;

    if (t < backHoldTicks)
    {
        s.atlasIndex = startStopFrames[7];
        s.actorX = 538.0f;
        s.label = "STOP";
        return s;
    }
    t -= backHoldTicks;

    if (t < turnTicks)
    {
        const int frame = juce::jlimit (0, 7, t / 3);
        s.atlasIndex = turnFrames[frame];
        s.actorX = 538.0f;
        s.label = "DIRECTION / TURN";
        return s;
    }
    t -= turnTicks;

    if (t < fadeTicks)
    {
        s.atlasIndex = turnFrames[7];
        s.actorX = 538.0f;
        s.opacity = 1.0f - static_cast<float> (t) / static_cast<float> (fadeTicks);
        s.label = "CUT";
        return s;
    }
    t -= fadeTicks;

    // Other-pose section: use the board's six photographic pose samples.
    s.actorX = 360.0f;

    if (t < poseHoldTicks)
    {
        s.atlasIndex = poseFrames[0];
        s.actorWidth = 230.0f;
        s.actorHeight = 460.0f;
        s.actorY = 626.0f;
        s.opacity = juce::jlimit (0.0f, 1.0f, static_cast<float> (t) / 5.0f);
        s.label = "SIT 1";
        return s;
    }
    t -= poseHoldTicks;

    if (t < poseHoldTicks)
    {
        s.atlasIndex = poseFrames[1];
        s.actorWidth = 230.0f;
        s.actorHeight = 460.0f;
        s.actorY = 626.0f;
        s.label = "SIT 2";
        return s;
    }
    t -= poseHoldTicks;

    if (t < poseHoldTicks)
    {
        s.atlasIndex = poseFrames[2];
        s.actorWidth = 230.0f;
        s.actorHeight = 460.0f;
        s.actorY = 626.0f;
        s.label = "CROUCH 1";
        return s;
    }
    t -= poseHoldTicks;

    if (t < poseHoldTicks)
    {
        s.atlasIndex = poseFrames[3];
        s.actorWidth = 230.0f;
        s.actorHeight = 460.0f;
        s.actorY = 626.0f;
        s.label = "CROUCH 2";
        return s;
    }
    t -= poseHoldTicks;

    if (t < poseHoldTicks)
    {
        s.atlasIndex = poseFrames[4];
        s.actorWidth = 230.0f;
        s.actorHeight = 460.0f;
        s.actorY = 626.0f;
        s.label = "KNEES UP";
        return s;
    }
    t -= poseHoldTicks;

    if (t < lieHoldTicks)
    {
        s.atlasIndex = poseFrames[5];
        s.actorWidth = 360.0f;
        s.actorHeight = 360.0f;
        s.actorY = 600.0f;
        s.label = "LIE DOWN";
        return s;
    }
    t -= lieHoldTicks;

    s.atlasIndex = poseFrames[5];
    s.actorWidth = 360.0f;
    s.actorHeight = 360.0f;
    s.actorY = 600.0f;
    s.opacity = 1.0f - static_cast<float> (t) / static_cast<float> (fadeTicks);
    s.label = "LOOP";
    return s;
}

void TwilightPoseComponent::drawDesignSurface (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0e));

    if (! visualsReady)
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
        g.drawFittedText (
            "TWILIGHT VISUAL DATA ERROR",
            40, 40, 640, 640,
            juce::Justification::centred,
            2);
        return;
    }

    // A 236x168 rooftop photograph from the supplied reference board is
    // deliberately enlarged with low-quality sampling, preserving the
    // digitised late-90s game texture instead of smoothing it away.
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (
        rooftopBackground,
        0, 104, 720, 512,
        0, 0,
        rooftopBackground.getWidth(),
        rooftopBackground.getHeight(),
        false);

    // Dark lower/upper letterbox regions are part of the intended fixed-camera
    // composition and keep the square RG Rotate screen from looking like a
    // stretched phone layout.
    g.setColour (juce::Colour (0xff101011));
    g.fillRect (0, 0, 720, 104);
    g.fillRect (0, 616, 720, 104);

    const auto state = getFrameState();

    const int cellX = (state.atlasIndex % atlasColumns) * atlasCellWidth;
    const int cellY = (state.atlasIndex / atlasColumns) * atlasCellHeight;

    const int destW = juce::roundToInt (state.actorWidth);
    const int destH = juce::roundToInt (state.actorHeight);
    const int destX = juce::roundToInt (state.actorX - 0.5f * state.actorWidth);
    const int destY = juce::roundToInt (state.actorY - state.actorHeight);

    g.setOpacity (state.opacity);
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (
        poseAtlas,
        destX, destY, destW, destH,
        cellX, cellY, atlasCellWidth, atlasCellHeight,
        false);
    g.setOpacity (1.0f);

    // Sparse scan structure and vignette.  These are intentionally subtle:
    // the photographic actor remains the focus.
    g.setColour (juce::Colours::black.withAlpha (0.055f));
    for (int y = 104; y < 616; y += 4)
        g.fillRect (0, y, 720, 1);

    juce::ColourGradient edge (
        juce::Colours::transparentBlack,
        360.0f, 360.0f,
        juce::Colours::black.withAlpha (0.25f),
        690.0f, 690.0f,
        true);
    g.setGradientFill (edge);
    g.fillRect (0, 104, 720, 512);

    g.setColour (juce::Colours::white.withAlpha (0.70f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (
        juce::String ("FLOWER / TWILIGHT POSE STUDY    ")
            + state.label
            + "    " + juce::String (measuredFps.load(), 1) + " FPS",
        15, 72, 690, 24,
        juce::Justification::centredLeft,
        false);
}

void TwilightPoseComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    const float scale = juce::jmin (
        static_cast<float> (getWidth()) / static_cast<float> (designSize),
        static_cast<float> (getHeight()) / static_cast<float> (designSize));

    const float contentW = designSize * scale;
    const float contentH = designSize * scale;
    const float offsetX = (static_cast<float> (getWidth()) - contentW) * 0.5f;
    const float offsetY = (static_cast<float> (getHeight()) - contentH) * 0.5f;

    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::translation (offsetX, offsetY));
    g.addTransform (juce::AffineTransform::scale (scale));

    drawDesignSurface (g);
}

void TwilightPoseComponent::resized()
{
}

void TwilightPoseComponent::timerCallback()
{
    ++tick;

    ++fpsFrames;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto elapsed = now - fpsWindowStartMs;

    if (elapsed >= 1000.0)
    {
        measuredFps.store (
            static_cast<float> (
                static_cast<double> (fpsFrames) * 1000.0 / elapsed),
            std::memory_order_relaxed);

        fpsWindowStartMs = now;
        fpsFrames = 0;
    }

    repaint();
}
