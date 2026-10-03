#include "DreamyEffect.h"
#include <cmath>
#include <algorithm>

void DreamyEffect::prepare(int sampleRate) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000;
    const size_t size = static_cast<size_t>(sampleRate_ * 2.5); // FLOWER Dreamy history ~2.5 s
    histL_.assign(size, 0.f);
    histR_.assign(size, 0.f);
    write_ = 0;
    historyFrames_ = 0;
    voices_ = {};
    x_ = targetX_ = 0.28f;
    y_ = targetY_ = 0.28f;
    wetLpL_ = wetLpR_ = 0.f;
}

void DreamyEffect::setXY(float x, float y) {
    targetX_ = std::clamp(x, 0.f, 1.f);
    targetY_ = std::clamp(y, 0.f, 1.f);
}

double DreamyEffect::wrap(double p) const {
    const double n = double(histL_.size());
    while (p < 0.0) p += n;
    while (p >= n) p -= n;
    return p;
}

float DreamyEffect::readInterp(const std::vector<float>& b, double p) const {
    p = wrap(p);
    size_t i0 = static_cast<size_t>(p);
    size_t i1 = (i0 + 1) % b.size();
    float f = float(p - std::floor(p));
    return b[i0] + (b[i1] - b[i0]) * f;
}

float DreamyEffect::grainSample(Grain& g, const std::vector<float>& b, double speed,
                                double loopFrames, double resetLagFrames,
                                int voiceIndex, int grainIndex) {
    if (!g.initialized) {
        g.phase = grainIndex == 0 ? 0.0 : 0.5;
        // Keep the first wet fragment safely behind the live write head.
        g.read = wrap(double(write_) - resetLagFrames - g.phase * loopFrames);
        g.initialized = true;
    }

    // Hann window gives overlap/fade at every micro-loop boundary and avoids the
    // hard fragment-switch clicks that produced the earlier "jirijiri" texture.
    const double window = 0.5 - 0.5 * std::cos(6.283185307179586 * g.phase);
    float v = readInterp(b, g.read) * float(window);
    g.read = wrap(g.read + speed);
    g.phase += 1.0 / loopFrames;
    if (g.phase >= 1.0) {
        g.phase -= 1.0;
        // Two voices/grains deliberately pick slightly different points in history.
        const double spread = (0.045 * grainIndex + 0.085 * voiceIndex) * sampleRate_;
        g.read = wrap(double(write_) - resetLagFrames - spread);
    }
    return v;
}

void DreamyEffect::process(float& l, float& r) {
    if (histL_.empty()) return;
    const float dryL = l, dryR = r;

    // Always accumulate history, even while Dreamy is bypassed.
    histL_[write_] = dryL;
    histR_[write_] = dryR;
    write_ = (write_ + 1) % histL_.size();
    historyFrames_++;

    // Smooth X/Y so MIDI CC changes cannot create discontinuities.
    const float smooth = 1.f - std::exp(-1.f / (0.025f * float(sampleRate_))); // ~25 ms
    x_ += (targetX_ - x_) * smooth;
    y_ += (targetY_ - y_) * smooth;

    const uint64_t minHistory = static_cast<uint64_t>(0.75 * sampleRate_);
    if (!enabled_ || historyFrames_ < minHistory) {
        l = dryL; r = dryR; return;
    }

    // FLOWER reference: X drift 0.90..1.00, Y controls loop length + wet.
    const double drift = 0.90 + 0.10 * double(x_);
    // Shorter fragments as Y rises. The exact range is kept conservative here;
    // the key reference behavior is the same: Y up => shorter/stronger texture.
    const double loopSec = 0.34 - 0.26 * double(y_); // 340 ms -> 80 ms
    const double loopFrames = std::max(64.0, loopSec * sampleRate_);
    const double baseLag = (0.75 + 0.28 * (1.0 - double(x_))) * sampleRate_;

    constexpr double speed5 = 1.3348398; // +5 semitones
    constexpr double speed12 = 2.0;      // +12 semitones

    float wetL = 0.f, wetR = 0.f;
    const double speeds[2] = { speed5 * drift, speed12 * drift };
    for (int vi=0; vi<2; ++vi) {
        float vl=0.f, vr=0.f;
        for (int gi=0; gi<2; ++gi) {
            const double lag = baseLag + vi * 0.11 * sampleRate_;
            vl += grainSample(voices_[vi].grains[gi], histL_, speeds[vi], loopFrames, lag, vi, gi);
            vr += grainSample(voices_[vi].grains[gi], histR_, speeds[vi], loopFrames, lag, vi, gi);
        }
        wetL += vl * 0.5f;
        wetR += vr * 0.5f;
    }
    wetL *= 0.5f; wetR *= 0.5f;

    // High-frequency damping added for the known Dreamy "jirijiri" failure mode.
    const float cutoff = 6000.f;
    const float lpA = 1.f - std::exp(-6.283185307179586f * cutoff / float(sampleRate_));
    wetLpL_ += lpA * (wetL - wetLpL_);
    wetLpR_ += lpA * (wetR - wetLpR_);

    const float wet = std::clamp(0.24f + y_ * 0.34f, 0.20f, 0.58f);
    l = dryL * (1.f - wet) + wetLpL_ * wet;
    r = dryR * (1.f - wet) + wetLpR_ * wet;
}
