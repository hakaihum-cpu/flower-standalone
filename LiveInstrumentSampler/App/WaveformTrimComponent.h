#pragma once

#include <JuceHeader.h>

class WaveformTrimComponent final : public juce::Component,
                                    private juce::ChangeListener {
public:
    WaveformTrimComponent();
    ~WaveformTrimComponent() override;

    bool loadFile(const juce::File& file);
    void clear();
    double startSeconds() const noexcept { return startSec_; }
    double endSeconds() const noexcept { return endSec_; }
    double durationSeconds() const noexcept { return durationSec_; }
    void setTrimSeconds(double start, double end);

    std::function<void(double, double)> onTrimChanged;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    enum class DragHandle { none, start, end };

    double xToSeconds(float x) const;
    float secondsToX(double sec) const;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    juce::AudioFormatManager formatManager_;
    juce::AudioThumbnailCache thumbnailCache_{8};
    juce::AudioThumbnail thumbnail_{512, formatManager_, thumbnailCache_};
    double durationSec_ = 0.0;
    double startSec_ = 0.0;
    double endSec_ = 0.0;
    DragHandle drag_ = DragHandle::none;
};
