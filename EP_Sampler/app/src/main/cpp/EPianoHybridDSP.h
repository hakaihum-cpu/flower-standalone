#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// E.PIANO prototype DSP. Audio-thread only: no memory allocations, locks, or I/O.
// Applied exclusively to EPBANK slot 0, after the unmodified multisample engine.
// Resonance is intentionally quiet: sampled VST tails must remain the main sound.
class EPianoHybridDSP {
public:
    void prepare(int sampleRate) {
        sampleRate_ = std::max(8000, sampleRate);
        phase_ = 0.0f;
        tailFrames_ = 0;
        activeWet_ = 0.0f;
        nextMode_ = 0;
        for (auto& m : modes_) m = Mode{};
    }

    void control(int cc, int value) {
        const float v = std::clamp(value, 0, 127) / 127.0f;
        switch (cc) {
            case 80: enabled_ = value >= 64; break;
            case 81: resonance_ = v; break;
            case 82: drive_ = v; break;
            case 83: tremolo_ = v; break;
            default: break;
        }
    }

    void noteOn(int note, int velocity) {
        if (note < 21 || note > 108 || velocity < 1) return;
        const float base = 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
        const float vel = std::clamp(velocity, 1, 127) / 127.0f;
        // Tuned, damped modes are excited by actual multisample playback.
        // A limited rotating bank prevents unbounded resonator accumulation.
        constexpr float ratios[3] = {1.0f, 2.01f, 3.93f};
        for (float ratio : ratios) {
            const float hz = base * ratio;
            if (hz >= 0.40f * sampleRate_) continue;
            Mode& m = modes_[nextMode_++ % modes_.size()];
            m = Mode{};
            const float decay = 0.14f + 0.42f / ratio;
            const float pole = std::exp(-1.0f / (decay * sampleRate_));
            m.coeff = 2.0f * pole * std::cos(6.28318530718f * hz / sampleRate_);
            m.poleSquared = pole * pole;
            m.injection = (1.0f - pole) * (0.4f + 0.6f * vel);
            m.valid = true;
        }
        tailFrames_ = sampleRate_ * 3;
    }

    bool hasTail() const { return tailFrames_ > 0 && activeWet_ > 0.00001f; }

    void process(float& left, float& right) {
        // Keep transitions smooth when a UI control or MIDI CC toggles the DSP.
        const float targetWet = enabled_ ? resonance_ * 0.24f : 0.0f;
        activeWet_ += 0.0018f * (targetWet - activeWet_);
        if (!enabled_ && activeWet_ < 0.00001f) return;

        const float dryL = std::isfinite(left) ? left : 0.0f;
        const float dryR = std::isfinite(right) ? right : 0.0f;
        float modalL = 0.0f;
        float modalR = 0.0f;
        if (tailFrames_ > 0) {
            --tailFrames_;
            // Each mode is a stable second-order damped oscillator.
            for (auto& m : modes_) {
                if (!m.valid) continue;
                float yl = dryL * m.injection + m.coeff * m.l1 - m.poleSquared * m.l2;
                float yr = dryR * m.injection + m.coeff * m.r1 - m.poleSquared * m.r2;
                if (std::fabs(yl) < 1.0e-18f) yl = 0.0f; // avoid denormals
                if (std::fabs(yr) < 1.0e-18f) yr = 0.0f;
                m.l2 = m.l1; m.l1 = yl;
                m.r2 = m.r1; m.r1 = yr;
                modalL += yl;
                modalR += yr;
            }
        }

        float l = dryL + activeWet_ * std::clamp(modalL, -1.5f, 1.5f);
        float r = dryR + activeWet_ * std::clamp(modalR, -1.5f, 1.5f);

        // Low-level preamp colouring: parallel blend preserves the VST sample.
        const float driveBlend = enabled_ ? 0.20f * drive_ : 0.0f;
        const float amount = 1.5f + 3.0f * drive_;
        auto soft = [amount](float x) {
            return x * (1.0f + amount) / (1.0f + amount * std::fabs(x));
        };
        l += driveBlend * (soft(l) - l);
        r += driveBlend * (soft(r) - r);

        // Optional stereo tremolo; defaults off so samples remain uncoloured.
        phase_ += 6.28318530718f * 4.8f / sampleRate_;
        if (phase_ >= 6.28318530718f) phase_ -= 6.28318530718f;
        const float depth = enabled_ ? tremolo_ * 0.70f : 0.0f;
        if (depth > 0.00001f) {
            const float sinPhase = std::sin(phase_);
            l *= 1.0f - depth * 0.5f * (1.0f - sinPhase);
            r *= 1.0f - depth * 0.5f * (1.0f + sinPhase);
        }

        left = std::isfinite(l) ? l : 0.0f;
        right = std::isfinite(r) ? r : 0.0f;
    }

private:
    struct Mode {
        float coeff = 0.0f, poleSquared = 0.0f, injection = 0.0f;
        float l1 = 0.0f, l2 = 0.0f, r1 = 0.0f, r2 = 0.0f;
        bool valid = false;
    };
    std::array<Mode, 18> modes_{};
    size_t nextMode_ = 0;
    int sampleRate_ = 48000;
    int tailFrames_ = 0;
    bool enabled_ = true;
    float resonance_ = 0.15f;
    float drive_ = 0.12f;
    float tremolo_ = 0.0f;
    float activeWet_ = 0.0f;
    float phase_ = 0.0f;
};
