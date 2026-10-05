#include "DreamyEffect.h"
#include <cmath>
#include <algorithm>

namespace {
constexpr double kTwoPi = 6.28318530717958647692;
inline float clamp1(float v) { return std::clamp(v, 0.0f, 1.0f); }
}

void DreamyEffect::prepare(int sampleRate) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000;

    // Four seconds comfortably covers the longer MOOD/Chroma-style loops while
    // keeping MODE 0's original ~0.7-1.1 s read positions unchanged.
    const size_t histSize = static_cast<size_t>(sampleRate_ * 4.0);
    histL_.assign(histSize, 0.f);
    histR_.assign(histSize, 0.f);

    // Feedback/diffusion ring is kept separate from input history so no mode
    // can recursively corrupt another mode's dry capture.
    const size_t fxSize = static_cast<size_t>(sampleRate_ * 3.0);
    fxL_.assign(fxSize, 0.f);
    fxR_.assign(fxSize, 0.f);

    write_ = 0;
    fxWrite_ = 0;
    historyFrames_ = 0;
    voices_ = {};
    grains_ = {};

    targetP1_ = p1_ = 0.28f;
    targetP2_ = p2_ = 0.28f;
    targetP3_ = p3_ = 0.50f;
    targetP4_ = p4_ = 0.50f;
    targetMix_ = mix_ = 0.34f;

    currentMode_ = targetMode_ = DREAMY;
    switchingMode_ = false;
    fadingOutForMode_ = false;
    modeFade_ = 1.0f;
    enableFade_ = enabled_ ? 1.0f : 0.0f;

    wetLpL_ = wetLpR_ = 0.f;
    diffuseLpL_ = diffuseLpR_ = 0.f;
    toneLpL_ = toneLpR_ = 0.f;
    rng_ = 0x41C64E6Du;
}

void DreamyEffect::setMode(int mode) {
    targetMode_ = std::clamp(mode, 0, MODE_COUNT - 1);
}

void DreamyEffect::setXY(float x, float y) {
    targetP1_ = clamp1(x);
    targetP2_ = clamp1(y);
}

void DreamyEffect::setParameters(float x, float y, float mix) {
    targetP1_ = clamp1(x);
    targetP2_ = clamp1(y);
    targetMix_ = clamp1(mix);
}

void DreamyEffect::setExtraParameters(float p3, float p4) {
    targetP3_ = clamp1(p3);
    targetP4_ = clamp1(p4);
}

double DreamyEffect::wrap(double p, size_t size) const {
    if (size == 0) return 0.0;
    const double n = double(size);
    while (p < 0.0) p += n;
    while (p >= n) p -= n;
    return p;
}

double DreamyEffect::wrapHistory(double p) const {
    return wrap(p, histL_.size());
}

float DreamyEffect::readInterp(const std::vector<float>& b, double p) const {
    if (b.empty()) return 0.f;
    p = wrap(p, b.size());
    const size_t i0 = static_cast<size_t>(p);
    const size_t i1 = (i0 + 1) % b.size();
    const float f = float(p - std::floor(p));
    return b[i0] + (b[i1] - b[i0]) * f;
}

float DreamyEffect::readFx(const std::vector<float>& b, double delayFrames) const {
    if (b.empty()) return 0.f;
    delayFrames = std::clamp(delayFrames, 1.0, double(b.size() - 3));
    return readInterp(b, double(fxWrite_) - delayFrames);
}

float DreamyEffect::randomSigned() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (float(rng_ & 0x00FFFFFFu) / 8388607.5f) - 1.0f;
}

// Original MODE 0 sampler intentionally retained byte-for-byte in behavior:
// one mono call per channel advances the grain twice, matching the established
// Dreamy sound the user already approved.
float DreamyEffect::grainSample(Grain& g, const std::vector<float>& b, double speed,
                                double loopFrames, double resetLagFrames,
                                int voiceIndex, int grainIndex) {
    if (!g.initialized) {
        g.phase = grainIndex == 0 ? 0.0 : 0.5;
        g.read = wrapHistory(double(write_) - resetLagFrames - g.phase * loopFrames);
        g.initialized = true;
    }

    const double window = 0.5 - 0.5 * std::cos(kTwoPi * g.phase);
    const float v = readInterp(b, g.read) * float(window);
    g.read = wrapHistory(g.read + speed);
    g.phase += 1.0 / loopFrames;
    if (g.phase >= 1.0) {
        g.phase -= 1.0;
        const double spread = (0.045 * grainIndex + 0.085 * voiceIndex) * sampleRate_;
        g.read = wrapHistory(double(write_) - resetLagFrames - spread);
    }
    return v;
}

void DreamyEffect::grainStereo(Grain& g,
                               double speed,
                               double loopFrames,
                               double resetLagFrames,
                               double jitterFrames,
                               float& outL,
                               float& outR) {
    loopFrames = std::max(48.0, loopFrames);
    if (!g.initialized) {
        const size_t index = static_cast<size_t>(&g - grains_.data());
        g.phase = (index & 1u) ? 0.5 : 0.0;
        g.read = wrapHistory(double(write_) - resetLagFrames - g.phase * loopFrames);
        g.initialized = true;
    }

    const double w = 0.5 - 0.5 * std::cos(kTwoPi * g.phase);
    const double pos = g.read;
    outL += readInterp(histL_, pos) * float(w);
    outR += readInterp(histR_, pos) * float(w);

    g.read = wrapHistory(g.read + speed);
    g.phase += 1.0 / loopFrames;
    if (g.phase >= 1.0) {
        g.phase -= 1.0;
        const double jitter = double(randomSigned()) * jitterFrames;
        g.read = wrapHistory(double(write_) - resetLagFrames - jitter);
    }
}

void DreamyEffect::resetNewModeState() {
    voices_ = {};
    grains_ = {};
    std::fill(fxL_.begin(), fxL_.end(), 0.f);
    std::fill(fxR_.begin(), fxR_.end(), 0.f);
    fxWrite_ = 0;
    diffuseLpL_ = diffuseLpR_ = 0.f;
    toneLpL_ = toneLpR_ = 0.f;
}

void DreamyEffect::simpleDiffusion(float dryL, float dryR,
                                   float delayMs, float feedback, float damping,
                                   float& wetL, float& wetR) {
    if (fxL_.empty()) return;

    const double delayFrames = std::clamp(
            double(delayMs) * 0.001 * double(sampleRate_),
            8.0, double(fxL_.size() - 4));

    const float tapL = readFx(fxL_, delayFrames);
    const float tapR = readFx(fxR_, delayFrames * 1.037 + 17.0);

    damping = std::clamp(damping, 0.015f, 0.55f);
    diffuseLpL_ += (tapL - diffuseLpL_) * damping;
    diffuseLpR_ += (tapR - diffuseLpR_) * damping;

    feedback = std::clamp(feedback, 0.0f, 0.88f);

    // Cross-channel feedback creates width without hard ping-pong switching.
    const float inL = std::tanh(dryL + diffuseLpR_ * feedback);
    const float inR = std::tanh(dryR + diffuseLpL_ * feedback);
    fxL_[fxWrite_] = std::clamp(inL, -1.6f, 1.6f);
    fxR_[fxWrite_] = std::clamp(inR, -1.6f, 1.6f);

    fxWrite_++;
    if (fxWrite_ >= fxL_.size()) fxWrite_ = 0;

    wetL += diffuseLpL_;
    wetR += diffuseLpR_;
}

void DreamyEffect::renderDreamy(float& wetL, float& wetR) {
    const double loopSec = 0.34 - 0.26 * double(p2_);
    const double loopFrames = std::max(64.0, loopSec * sampleRate_);
    const double baseLag = (0.70 + 0.38 * (1.0 - double(p1_))) * sampleRate_;

    constexpr double speed5 = 1.3348398;
    constexpr double speed12 = 2.0;

    const double speeds[2] = {speed5, speed12};
    for (int vi=0; vi<2; ++vi) {
        float vl=0.f, vr=0.f;
        for (int gi=0; gi<2; ++gi) {
            const double lag = baseLag + vi * 0.11 * sampleRate_;
            vl += grainSample(voices_[vi].grains[gi], histL_, speeds[vi],
                              loopFrames, lag, vi, gi);
            vr += grainSample(voices_[vi].grains[gi], histR_, speeds[vi],
                              loopFrames, lag, vi, gi);
        }
        wetL += vl * 0.5f;
        wetR += vr * 0.5f;
    }
    wetL *= 0.5f;
    wetR *= 0.5f;
}

void DreamyEffect::renderMicrocosmMosaic(float& wetL, float& wetR) {
    // Microcosm MOSAIC documentation describes overlapping loops at multiple
    // speeds. P2 moves continuously across the A-D style speed families.
    const int variation = std::clamp(int(p2_ * 4.0f), 0, 3);
    static const double speeds[4][4] = {
        {1.0, 2.0, 1.0, 2.0},
        {1.0, 0.5, 1.0, 0.5},
        {2.0, 2.0, 2.0, 2.0},
        {0.5, 1.0, 2.0, 4.0}
    };

    const int active = std::clamp(1 + int(std::round(p1_ * 3.0f)), 1, 4);
    const double loopFrames = (0.20 - 0.12 * p1_) * sampleRate_;
    const double lagBase = (0.36 + 0.60 * p3_) * sampleRate_;
    for (int i=0; i<active; ++i) {
        const double lag = lagBase + i * (0.055 + 0.035 * p3_) * sampleRate_;
        grainStereo(grains_[i], speeds[variation][i], loopFrames,
                    lag, 0.012 * sampleRate_, wetL, wetR);
    }
    const float g = 1.0f / std::sqrt(float(active));
    wetL *= g;
    wetR *= g;
}

void DreamyEffect::renderMicrocosmGlide(float& wetL, float& wetR) {
    // GLIDE: overlapping short loops shift in pitch; Activity changes rate and
    // Shape selects the glide family.
    const int variation = std::clamp(int(p2_ * 4.0f), 0, 3);
    const double t = double(historyFrames_) / double(sampleRate_);
    const double rate = 0.08 + 1.45 * double(p1_);
    const double s = 0.5 + 0.5 * std::sin(kTwoPi * rate * t);

    double lo=0.5, hi=1.0;
    if (variation == 1) { lo = 0.5; hi = 2.0; }
    else if (variation == 2) { lo = 1.0; hi = 2.0; }

    const int active = 4;
    for (int i=0; i<active; ++i) {
        double phase = s;
        if (variation == 3 && (i & 1)) phase = 1.0 - s;
        const double speed = lo + (hi - lo) * phase;
        const double lag = (0.30 + 0.48 * p3_ + i * 0.045) * sampleRate_;
        grainStereo(grains_[i], speed, 0.095 * sampleRate_,
                    lag, 0.008 * sampleRate_, wetL, wetR);
    }
    wetL *= 0.5f;
    wetR *= 0.5f;
}

void DreamyEffect::renderHaze(float& wetL, float& wetR) {
    // HAZE documentation: grain density/spread plus A-D variants:
    // diffuse stretch, randomized grains, 1x+2x, 1x+0.5x.
    const int variation = std::clamp(int(p3_ * 4.0f), 0, 3);
    const int active = std::clamp(2 + int(std::round(p1_ * 10.0f)), 2, 12);
    const double loopFrames = (0.135 - 0.095 * p1_) * sampleRate_;
    const double spreadFrames = (0.012 + 0.38 * p2_) * sampleRate_;

    for (int i=0; i<active; ++i) {
        double speed = 1.0;
        if (variation == 0) {
            speed = 0.82 + 0.30 * (0.5 + 0.5 * std::sin(0.17 * i + historyFrames_ * 0.000017));
        } else if (variation == 1) {
            const double slowRandom = 0.5 + 0.5 * std::sin(
                    0.73 * i + double(historyFrames_) * (0.000006 + 0.000004 * i));
            speed = 0.72 + 0.58 * slowRandom;
        } else if (variation == 2) {
            speed = (i & 1) ? 2.0 : 1.0;
        } else {
            speed = (i & 1) ? 0.5 : 1.0;
        }

        const double lag = (0.22 + 0.028 * i) * sampleRate_;
        grainStereo(grains_[i], speed, loopFrames,
                    lag, spreadFrames, wetL, wetR);
    }

    const float g = 0.82f / std::sqrt(float(active));
    wetL *= g;
    wetR *= g;
}

void DreamyEffect::renderChromaCollage(float dryL, float dryR, float& wetL, float& wetR) {
    // COLLAGE: time ranges from granular snippets to loop-like phrases,
    // Amount increases repeats/feedback, Drift adds double-speed loops and
    // pitch modulation. P4 is a Cassette-style degradation macro.
    const double t = double(historyFrames_) / double(sampleRate_);
    const double loopSec = 0.035 + 0.72 * p1_;
    const double drift = p3_;
    const double wobble = std::sin(kTwoPi * (0.11 + 0.42 * p4_) * t)
                        * (0.002 + 0.026 * drift);
    const double pulse = 0.5 + 0.5 * std::sin(kTwoPi * (0.37 + 0.8 * drift) * t);
    const double doubleSpeedBlend = drift * std::pow(std::max(0.0, pulse), 6.0);
    const double speed = 1.0 + doubleSpeedBlend + wobble;

    for (int i=0; i<3; ++i) {
        grainStereo(grains_[i], speed + i * 0.004,
                    loopSec * sampleRate_,
                    (0.12 + 0.22 * p1_ + 0.045 * i) * sampleRate_,
                    (0.004 + 0.035 * drift) * sampleRate_, wetL, wetR);
    }
    wetL *= 0.45f;
    wetR *= 0.45f;

    float dl=0.f, dr=0.f;
    simpleDiffusion(dryL, dryR,
                    45.0f + 590.0f * p1_,
                    0.18f + 0.67f * p2_,
                    0.20f - 0.13f * p4_,
                    dl, dr);
    wetL += dl * (0.18f + 0.52f * p2_);
    wetR += dr * (0.18f + 0.52f * p2_);

    // Cassette macro: darker and slightly softer as degradation rises.
    const float toneA = 0.18f - 0.13f * p4_;
    toneLpL_ += (wetL - toneLpL_) * std::clamp(toneA, 0.025f, 0.18f);
    toneLpR_ += (wetR - toneLpR_) * std::clamp(toneA, 0.025f, 0.18f);
    wetL = wetL * (1.f - 0.55f*p4_) + toneLpL_ * (0.55f*p4_);
    wetR = wetR * (1.f - 0.55f*p4_) + toneLpR_ * (0.55f*p4_);
}

void DreamyEffect::renderChromaSpace(float dryL, float dryR, float& wetL, float& wetR) {
    // SPACE: size/tonality, wet amount, pitch-modulating Drift. P4 adds a mild
    // cassette-like softening to keep the "recording gear" character.
    const double t = double(historyFrames_) / double(sampleRate_);
    const float driftMs = float(std::sin(kTwoPi * (0.07 + 0.23*p3_) * t)
                              * (0.2 + 6.5*p3_));

    float aL=0.f,aR=0.f;
    simpleDiffusion(dryL, dryR,
                    58.0f + 430.0f*p1_ + driftMs,
                    0.42f + 0.43f*p2_,
                    0.10f - 0.065f*p4_,
                    aL,aR);

    const float tap2L = readFx(fxL_, (0.071 + 0.29*p1_) * sampleRate_);
    const float tap2R = readFx(fxR_, (0.083 + 0.33*p1_) * sampleRate_);
    wetL = aL * 0.72f + tap2L * 0.38f;
    wetR = aR * 0.72f + tap2R * 0.38f;

    const float toneA = std::clamp(0.16f - 0.10f*p4_, 0.035f, 0.16f);
    toneLpL_ += (wetL - toneLpL_) * toneA;
    toneLpR_ += (wetR - toneLpR_) * toneA;
    wetL = wetL * (1.f - 0.48f*p4_) + toneLpL_ * (0.48f*p4_);
    wetR = wetR * (1.f - 0.48f*p4_) + toneLpR_ * (0.48f*p4_);
}

void DreamyEffect::renderMoodReverb(float dryL, float dryR, float& wetL, float& wetR) {
    // MOOD Wet/Reverb: CLOCK changes quality/time, TIME controls decay/size,
    // MODIFY controls smear. P4 introduces a captured micro-loop layer.
    const float clock = 0.55f + 0.90f*p1_;
    const float size = p2_;
    const float smear = p3_;

    float rvL=0.f,rvR=0.f;
    simpleDiffusion(dryL, dryR,
                    (65.0f + 510.0f*size) / clock,
                    0.40f + 0.45f*size,
                    0.18f - 0.14f*smear,
                    rvL,rvR);

    float loopL=0.f,loopR=0.f;
    if (p4_ > 0.01f) {
        for (int i=0;i<2;++i) {
            grainStereo(grains_[i], clock,
                        (0.12 + 0.42*p4_) * sampleRate_,
                        (0.28 + 0.32*i) * sampleRate_,
                        0.010*sampleRate_, loopL, loopR);
        }
        loopL *= 0.5f;
        loopR *= 0.5f;
    }

    wetL = rvL * (0.52f + 0.35f*smear) + loopL * p4_ * 0.55f;
    wetR = rvR * (0.52f + 0.35f*smear) + loopR * p4_ * 0.55f;

    const float toneA = std::clamp(0.045f + 0.20f*p1_, 0.04f, 0.24f);
    toneLpL_ += (wetL-toneLpL_)*toneA;
    toneLpR_ += (wetR-toneLpR_)*toneA;
    wetL = toneLpL_;
    wetR = toneLpR_;
}

void DreamyEffect::renderMoodDelay(float dryL, float dryR, float& wetL, float& wetR) {
    // MOOD Wet/Delay: TIME and feedback are direct documented controls.
    const float clock = 0.55f + 0.90f*p1_;
    float dl=0.f,dr=0.f;
    simpleDiffusion(dryL, dryR,
                    (25.0f + 820.0f*p2_) / clock,
                    0.05f + 0.82f*p3_,
                    0.16f - 0.10f*(1.0f-p1_),
                    dl,dr);
    wetL = dl;
    wetR = dr;

    if (p4_ > 0.01f) {
        float loopL=0.f,loopR=0.f;
        grainStereo(grains_[0], clock,
                    (0.08 + 0.55*p4_) * sampleRate_,
                    (0.24 + 0.45*p2_) * sampleRate_,
                    0.006*sampleRate_, loopL, loopR);
        wetL += loopL * p4_ * 0.42f;
        wetR += loopR * p4_ * 0.42f;
    }
}

void DreamyEffect::renderMoodSlip(float& wetL, float& wetR) {
    // MOOD Wet/Slip: P2 refresh rate, P3 playback speed/direction.
    const float clock = 0.55f + 0.90f*p1_;
    double speed = (double(p3_) - 0.5) * 4.0;
    if (std::fabs(speed) < 0.08) speed = 0.0;

    const double loopFrames = (0.045 + 0.48*(1.0-p2_)) * sampleRate_;
    const double lag = (0.18 + 0.52*p2_) * sampleRate_;
    for (int i=0;i<3;++i) {
        grainStereo(grains_[i], speed * clock,
                    loopFrames,
                    lag + i*0.035*sampleRate_,
                    0.008*sampleRate_, wetL, wetR);
    }
    wetL *= 0.46f;
    wetR *= 0.46f;

    if (p4_ > 0.01f) {
        float holdL=0.f, holdR=0.f;
        grainStereo(grains_[3], 1.0,
                    (0.12 + 0.60*p4_) * sampleRate_,
                    0.48*sampleRate_, 0.0, holdL, holdR);
        wetL += holdL * p4_ * 0.45f;
        wetR += holdR * p4_ * 0.45f;
    }
}

void DreamyEffect::renderMoodTape(float dryL, float dryR, float& wetL, float& wetR) {
    // MOOD Micro-Looper/Tape: LENGTH + playback speed/direction. P4 mimics
    // loop fade/decay; CLOCK alters resolution/character.
    const float clock = 0.55f + 0.90f*p1_;
    double speed = (double(p3_) - 0.5) * 4.0;
    if (std::fabs(speed) < 0.06) speed = 0.0;

    const double loopFrames = (0.06 + 1.25*p2_) * sampleRate_;
    for (int i=0;i<2;++i) {
        grainStereo(grains_[i], speed * clock,
                    loopFrames,
                    (0.22 + i*0.09) * sampleRate_,
                    0.004*sampleRate_, wetL, wetR);
    }
    wetL *= 0.52f;
    wetR *= 0.52f;

    float dl=0.f,dr=0.f;
    simpleDiffusion(dryL,dryR,
                    90.0f + 410.0f*p2_,
                    0.12f + 0.64f*p4_,
                    0.10f,
                    dl,dr);
    wetL += dl * p4_ * 0.28f;
    wetR += dr * p4_ * 0.28f;
}

void DreamyEffect::renderMoodStretch(float& wetL, float& wetR) {
    // MOOD Micro-Looper/Stretch: slice size + stretch amount/direction.
    const float clock = 0.55f + 0.90f*p1_;
    const double amount = (double(p3_) - 0.5) * 2.0;
    const double speed = amount >= 0.0
            ? (0.18 + 0.82*(1.0-amount))
            : -(0.18 + 0.82*(1.0+amount));

    const double loopFrames = (0.035 + 0.62*p2_) * sampleRate_;
    for (int i=0;i<4;++i) {
        grainStereo(grains_[i], speed * clock,
                    loopFrames,
                    (0.20 + 0.055*i) * sampleRate_,
                    (0.006 + 0.018*std::fabs(amount))*sampleRate_,
                    wetL,wetR);
    }
    wetL *= 0.43f;
    wetR *= 0.43f;

    // P4 = tone, matching MOOD's documented hidden hi-cut concept.
    const float toneA = std::clamp(0.025f + 0.28f*p4_, 0.025f, 0.305f);
    toneLpL_ += (wetL-toneLpL_)*toneA;
    toneLpR_ += (wetR-toneLpR_)*toneA;
    wetL = toneLpL_;
    wetR = toneLpR_;
}

void DreamyEffect::process(float& l, float& r) {
    if (histL_.empty()) return;
    const float dryL = l;
    const float dryR = r;

    // Input history is always captured. This keeps mode changes immediate and
    // prevents the first wet fragment after bypass from reading silence.
    histL_[write_] = std::isfinite(dryL) ? dryL : 0.f;
    histR_[write_] = std::isfinite(dryR) ? dryR : 0.f;
    write_ = (write_ + 1) % histL_.size();
    historyFrames_++;

    // Smooth every user-adjustable parameter. ~30 ms is long enough to remove
    // zipper noise yet short enough to still feel responsive under MIDI/XY.
    const float smoothSeconds = currentMode_ == DREAMY ? 0.025f : 0.030f;
    const float smooth = 1.f - std::exp(-1.f / (smoothSeconds * float(sampleRate_)));
    p1_ += (targetP1_ - p1_) * smooth;
    p2_ += (targetP2_ - p2_) * smooth;
    p3_ += (targetP3_ - p3_) * smooth;
    p4_ += (targetP4_ - p4_) * smooth;
    mix_ += (targetMix_ - mix_) * smooth;

    // Bypass is crossfaded rather than switched to avoid a discontinuity.
    const float bypassStep = 1.0f / std::max(1.0f, 0.012f * float(sampleRate_));
    const float enableTarget = enabled_ ? 1.0f : 0.0f;
    if (enableFade_ < enableTarget) enableFade_ = std::min(enableTarget, enableFade_ + bypassStep);
    else if (enableFade_ > enableTarget) enableFade_ = std::max(enableTarget, enableFade_ - bypassStep);

    // Mode changes pass through dry for ~25 ms instead of hard-resetting grains
    // while audible. This removes the classic grain-boundary "jirijiri" click.
    if (targetMode_ != currentMode_ && !switchingMode_) {
        switchingMode_ = true;
        fadingOutForMode_ = true;
    }
    const float modeStep = 1.0f / std::max(1.0f, 0.025f * float(sampleRate_));
    if (switchingMode_) {
        if (fadingOutForMode_) {
            modeFade_ = std::max(0.0f, modeFade_ - modeStep);
            if (modeFade_ <= 0.0f) {
                currentMode_ = targetMode_;
                resetNewModeState();
                fadingOutForMode_ = false;
            }
        } else {
            modeFade_ = std::min(1.0f, modeFade_ + modeStep);
            if (modeFade_ >= 1.0f) switchingMode_ = false;
        }
    }

    const uint64_t minHistory = static_cast<uint64_t>(
            (currentMode_ == DREAMY ? 0.75 : 0.35) * sampleRate_);

    if (historyFrames_ < minHistory || enableFade_ <= 0.0001f) {
        l = dryL;
        r = dryR;
        return;
    }

    float wetL=0.f, wetR=0.f;
    switch (currentMode_) {
        case DREAMY:
            renderDreamy(wetL,wetR);
            break;
        case MICROCOSM_MOSAIC:
            renderMicrocosmMosaic(wetL,wetR);
            break;
        case MICROCOSM_GLIDE:
            renderMicrocosmGlide(wetL,wetR);
            break;
        case HAZE:
            renderHaze(wetL,wetR);
            break;
        case CHROMA_COLLAGE:
            renderChromaCollage(dryL,dryR,wetL,wetR);
            break;
        case CHROMA_SPACE:
            renderChromaSpace(dryL,dryR,wetL,wetR);
            break;
        case MOOD_REVERB:
            renderMoodReverb(dryL,dryR,wetL,wetR);
            break;
        case MOOD_DELAY:
            renderMoodDelay(dryL,dryR,wetL,wetR);
            break;
        case MOOD_SLIP:
            renderMoodSlip(wetL,wetR);
            break;
        case MOOD_TAPE:
            renderMoodTape(dryL,dryR,wetL,wetR);
            break;
        case MOOD_STRETCH:
            renderMoodStretch(wetL,wetR);
            break;
        default:
            renderDreamy(wetL,wetR);
            break;
    }

    // Microcosm-family SPACE parameter is P4. Use a restrained diffusion layer
    // after granular processing, never enough feedback to self-oscillate.
    if (currentMode_ == MICROCOSM_MOSAIC ||
        currentMode_ == MICROCOSM_GLIDE ||
        currentMode_ == HAZE) {
        float rvL=0.f,rvR=0.f;
        simpleDiffusion(dryL,dryR,
                        72.0f + 360.0f*p4_,
                        0.18f + 0.56f*p4_,
                        0.13f - 0.075f*p4_,
                        rvL,rvR);
        wetL += rvL * p4_ * 0.42f;
        wetR += rvR * p4_ * 0.42f;
    }

    if (!std::isfinite(wetL)) wetL = 0.f;
    if (!std::isfinite(wetR)) wetR = 0.f;
    if (currentMode_ != DREAMY) {
        wetL = std::tanh(std::clamp(wetL, -3.0f, 3.0f));
        wetR = std::tanh(std::clamp(wetR, -3.0f, 3.0f));
    }

    // MODE 0 keeps the existing 6 kHz damping exactly. New modes use a slightly
    // more open 8 kHz safety filter after all granular/feedback stages.
    const float cutoff = currentMode_ == DREAMY ? 6000.f : 8000.f;
    const float lpA = 1.f - std::exp(-float(kTwoPi) * cutoff / float(sampleRate_));
    wetLpL_ += lpA * (wetL - wetLpL_);
    wetLpR_ += lpA * (wetR - wetLpR_);

    const float maxWet = currentMode_ == DREAMY ? 0.72f : 0.78f;
    const float wet = std::clamp(mix_ * maxWet * modeFade_ * enableFade_, 0.f, maxWet);
    l = dryL * (1.f - wet) + wetLpL_ * wet;
    r = dryR * (1.f - wet) + wetLpR_ * wet;
}
