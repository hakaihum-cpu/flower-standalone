#pragma once

#include <JuceHeader.h>
#include <atomic>

class TwilightPoseComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    TwilightPoseComponent();
    ~TwilightPoseComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct FrameState
    {
        int atlasIndex = 0;
        float actorX = 360.0f;
        float actorY = 590.0f;
        float actorWidth = 192.0f;
        float actorHeight = 384.0f;
        float opacity = 1.0f;
        const char* label = "WAIT";
    };

    static juce::MemoryBlock decodeCompressedChunks (
        const char* const* chunks,
        int chunkCount,
        int expectedSize);

    void loadEmbeddedVisuals();
    void timerCallback() override;
    FrameState getFrameState() const;
    void drawDesignSurface (juce::Graphics& g);

    juce::Image poseAtlas;
    juce::Image rooftopBackground;

    int tick = 0;
    std::atomic<float> measuredFps { 0.0f };
    double fpsWindowStartMs = 0.0;
    int fpsFrames = 0;
    bool visualsReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwilightPoseComponent)
};
