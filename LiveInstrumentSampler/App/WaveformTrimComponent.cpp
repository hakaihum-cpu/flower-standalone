#include "WaveformTrimComponent.h"

WaveformTrimComponent::WaveformTrimComponent() {
    formatManager_.registerBasicFormats();
    thumbnail_.addChangeListener(this);
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

WaveformTrimComponent::~WaveformTrimComponent() {
    thumbnail_.removeChangeListener(this);
}

bool WaveformTrimComponent::loadFile(const juce::File& file) {
    clear();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return false;
    durationSec_ = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    startSec_ = 0.0;
    endSec_ = durationSec_;
    thumbnail_.setSource(new juce::FileInputSource(file));
    repaint();
    return true;
}

void WaveformTrimComponent::clear() {
    thumbnail_.clear();
    durationSec_ = 0.0;
    startSec_ = 0.0;
    endSec_ = 0.0;
    repaint();
}

void WaveformTrimComponent::setTrimSeconds(double start, double end) {
    if (durationSec_ <= 0.0) return;
    start = juce::jlimit(0.0, durationSec_, start);
    end = juce::jlimit(0.0, durationSec_, end);
    if (end < start) std::swap(start, end);
    if (end - start < 0.001)
        end = juce::jmin(durationSec_, start + 0.001);
    startSec_ = start;
    endSec_ = end;
    repaint();
    if (onTrimChanged) onTrimChanged(startSec_, endSec_);
}

void WaveformTrimComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff151515));
    auto r = getLocalBounds().reduced(8);
    g.setColour(juce::Colour(0xff303030));
    g.drawRect(r);

    if (durationSec_ <= 0.0) {
        g.setColour(juce::Colours::grey);
        g.drawFittedText("The waveform will appear here after STOP", r, juce::Justification::centred, 1);
        return;
    }

    g.setColour(juce::Colour(0xffbdbdbd));
    thumbnail_.drawChannels(g, r, 0.0, durationSec_, 1.0f);

    const float sx = secondsToX(startSec_);
    const float ex = secondsToX(endSec_);
    g.setColour(juce::Colours::black.withAlpha(0.58f));
    g.fillRect(r.withRight(static_cast<int>(sx)));
    g.fillRect(r.withLeft(static_cast<int>(ex)));

    g.setColour(juce::Colour(0xffeeeeee));
    g.drawLine(sx, static_cast<float>(r.getY()), sx, static_cast<float>(r.getBottom()), 2.0f);
    g.drawLine(ex, static_cast<float>(r.getY()), ex, static_cast<float>(r.getBottom()), 2.0f);
    g.drawText("START", static_cast<int>(sx) + 5, r.getY() + 4, 70, 20, juce::Justification::left);
    g.drawText("END", static_cast<int>(ex) - 75, r.getY() + 4, 70, 20, juce::Justification::right);
}

double WaveformTrimComponent::xToSeconds(float x) const {
    const auto r = getLocalBounds().reduced(8);
    const float nx = juce::jlimit(0.0f, 1.0f, (x - static_cast<float>(r.getX())) / static_cast<float>(juce::jmax(1, r.getWidth())));
    return static_cast<double>(nx) * durationSec_;
}

float WaveformTrimComponent::secondsToX(double sec) const {
    const auto r = getLocalBounds().reduced(8);
    if (durationSec_ <= 0.0) return static_cast<float>(r.getX());
    return static_cast<float>(r.getX()) + static_cast<float>(sec / durationSec_) * static_cast<float>(r.getWidth());
}

void WaveformTrimComponent::mouseDown(const juce::MouseEvent& e) {
    if (durationSec_ <= 0.0) return;
    const float sx = secondsToX(startSec_);
    const float ex = secondsToX(endSec_);
    drag_ = std::abs(e.position.x - sx) <= std::abs(e.position.x - ex) ? DragHandle::start : DragHandle::end;
    mouseDrag(e);
}

void WaveformTrimComponent::mouseDrag(const juce::MouseEvent& e) {
    if (durationSec_ <= 0.0 || drag_ == DragHandle::none) return;
    const double sec = xToSeconds(e.position.x);
    if (drag_ == DragHandle::start)
        setTrimSeconds(juce::jmin(sec, endSec_ - 0.001), endSec_);
    else
        setTrimSeconds(startSec_, juce::jmax(sec, startSec_ + 0.001));
}
