#include "SpaceEffect.h"
#include <algorithm>
#include <cmath>

void SpaceEffect::prepare(int sampleRate) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000;
    const size_t n = static_cast<size_t>(sampleRate_ * 0.40f) + 8;
    histL_.assign(n, 0.f);
    histR_.assign(n, 0.f);
    write_ = 0;
    dampL_ = dampR_ = 0.f;
}

void SpaceEffect::setMode(int mode) {
    mode_ = std::clamp(mode, int(NONE), int(SPACE));
}

float SpaceEffect::readDelay(const std::vector<float>& b, float seconds) const {
    if (b.empty()) return 0.f;
    const float d = std::max(1.f, seconds * float(sampleRate_));
    float p = float(write_) - d;
    const float n = float(b.size());
    while (p < 0.f) p += n;
    while (p >= n) p -= n;
    const size_t i0 = static_cast<size_t>(p);
    const size_t i1 = (i0 + 1) % b.size();
    const float f = p - std::floor(p);
    return b[i0] + (b[i1] - b[i0]) * f;
}

void SpaceEffect::process(float& l, float& r) {
    if (histL_.empty()) return;
    const float dryL = l, dryR = r;

    if (mode_ == NONE) {
        histL_[write_] = dryL;
        histR_[write_] = dryR;
        write_ = (write_ + 1) % histL_.size();
        return;
    }

    std::array<float,4> taps{};
    float feedback=0.42f, wet=0.18f, cutoff=7000.f, cross=0.10f;
    if (mode_ == ROOM) {
        taps = {0.023f,0.031f,0.041f,0.053f};
        feedback=0.42f; wet=0.18f; cutoff=7000.f; cross=0.10f;
    } else if (mode_ == HALL) {
        taps = {0.047f,0.061f,0.079f,0.101f};
        feedback=0.62f; wet=0.28f; cutoff=5600.f; cross=0.16f;
    } else {
        taps = {0.083f,0.127f,0.173f,0.239f};
        feedback=0.76f; wet=0.38f; cutoff=4300.f; cross=0.24f;
    }

    float aL=0.f, aR=0.f;
    for (int i=0;i<4;i++) {
        const float dl = readDelay(histL_, taps[i]);
        const float dr = readDelay(histR_, taps[(i+1)&3] * 1.017f);
        const float signL = (i==2) ? -1.f : 1.f;
        const float signR = (i==1) ? -1.f : 1.f;
        aL += dl * signL;
        aR += dr * signR;
    }
    aL *= 0.25f;
    aR *= 0.25f;

    const float lpA = 1.f - std::exp(-6.283185307179586f * cutoff / float(sampleRate_));
    dampL_ += lpA * (aL - dampL_);
    dampR_ += lpA * (aR - dampR_);

    histL_[write_] = dryL + (dampL_ + dampR_*cross) * feedback;
    histR_[write_] = dryR + (dampR_ + dampL_*cross) * feedback;
    write_ = (write_ + 1) % histL_.size();

    l = dryL * (1.f - wet) + dampL_ * wet;
    r = dryR * (1.f - wet) + dampR_ * wet;
}
