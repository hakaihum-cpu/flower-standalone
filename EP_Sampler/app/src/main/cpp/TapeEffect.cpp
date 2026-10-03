#include "TapeEffect.h"
#include <algorithm>
#include <cmath>

void TapeEffect::prepare(int sampleRate) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000;
    histL_.assign(static_cast<size_t>(sampleRate_ * 0.05f) + 16, 0.f);
    histR_.assign(histL_.size(), 0.f);
    write_ = 0;
    wowPhase_ = flutterPhase_ = 0.0;
    lpL_ = lpR_ = 0.f;
}

float TapeEffect::readInterp(const std::vector<float>& b, double p) const {
    if (b.empty()) return 0.f;
    const double n = double(b.size());
    while (p < 0.0) p += n;
    while (p >= n) p -= n;
    const size_t i0 = static_cast<size_t>(p);
    const size_t i1 = (i0 + 1) % b.size();
    const float f = float(p - std::floor(p));
    return b[i0] + (b[i1] - b[i0]) * f;
}

void TapeEffect::process(float& l, float& r) {
    if (histL_.empty()) return;

    histL_[write_] = l;
    histR_[write_] = r;

    if (!enabled_) {
        write_ = (write_ + 1) % histL_.size();
        return;
    }

    const double twoPi = 6.283185307179586;
    const double wow = std::sin(wowPhase_);
    const double flutter = std::sin(flutterPhase_);
    const double delay = 12.0 + 7.5*wow + 2.2*flutter;

    const double readL = double(write_) - delay;
    const double readR = double(write_) - delay - 0.7;
    float xL = readInterp(histL_, readL);
    float xR = readInterp(histR_, readR);

    wowPhase_ += twoPi * 0.33 / double(sampleRate_);
    flutterPhase_ += twoPi * 6.4 / double(sampleRate_);
    if (wowPhase_ >= twoPi) wowPhase_ -= twoPi;
    if (flutterPhase_ >= twoPi) flutterPhase_ -= twoPi;

    const float cutoff = 9000.f;
    const float a = 1.f - std::exp(-twoPi * cutoff / float(sampleRate_));
    lpL_ += a * (xL - lpL_);
    lpR_ += a * (xR - lpR_);

    const float drive = 1.35f;
    const float norm = 1.f / std::tanh(drive);
    l = std::tanh(lpL_ * drive) * norm;
    r = std::tanh(lpR_ * drive) * norm;

    write_ = (write_ + 1) % histL_.size();
}
