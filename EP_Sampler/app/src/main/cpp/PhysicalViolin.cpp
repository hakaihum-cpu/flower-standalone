#include "PhysicalViolin.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;

template <typename T>
T clampValue(T v, T lo, T hi) {
    return std::max(lo, std::min(hi, v));
}
}

double PhysicalViolin::midiToHz(double note) {
    return 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
}

void PhysicalViolin::prepare(double sampleRate) {
    sampleRate_ = sampleRate > 1000.0 ? sampleRate : 48000.0;

    const int openNotes[kStrings] = {55, 62, 69, 76}; // G3 D4 A4 E5
    for (int i = 0; i < kStrings; ++i) {
        strings_[i] = StringState{};
        strings_[i].openNote = openNotes[i];
        strings_[i].fundamental = midiToHz(openNotes[i]);
        strings_[i].targetFundamental = strings_[i].fundamental;
    }

    // Compact shared violin-body approximation. Frequencies are intentionally broad,
    // not a claim to reproduce a specific historical instrument.
    const float frequencies[kBodyModes] = {
        275.f, 390.f, 455.f, 525.f, 610.f, 720.f, 860.f,
        1040.f, 1250.f, 1510.f, 1850.f, 2250.f, 2750.f, 3350.f
    };
    const float gains[kBodyModes] = {
        0.085f, 0.075f, 0.105f, 0.120f, 0.110f, 0.095f, 0.078f,
        0.070f, 0.062f, 0.058f, 0.052f, 0.047f, 0.041f, 0.034f
    };

    for (int i = 0; i < kBodyModes; ++i) {
        const double f = frequencies[i];
        const double decaySeconds = 0.18 + 0.012 * i;
        const double r = std::exp(-1.0 / (decaySeconds * sampleRate_));
        const double w = 2.0 * kPi * f / sampleRate_;
        body_[i] = BodyMode{};
        body_[i].a1 = static_cast<float>(2.0 * r * std::cos(w));
        body_[i].a2 = static_cast<float>(-r * r);
        body_[i].gain = gains[i];
    }

    reset();
}

void PhysicalViolin::reset() {
    const int openNotes[kStrings] = {55, 62, 69, 76};
    for (int i = 0; i < kStrings; ++i) {
        const int open = strings_[i].openNote > 0 ? strings_[i].openNote : openNotes[i];
        strings_[i] = StringState{};
        strings_[i].openNote = open;
        strings_[i].fundamental = midiToHz(open);
        strings_[i].targetFundamental = strings_[i].fundamental;
    }
    for (auto& m : body_) {
        m.y1 = 0.0f;
        m.y2 = 0.0f;
    }
}

int PhysicalViolin::chooseString(int note) const {
    int best = -1;
    int bestScore = 100000;

    for (int i = 0; i < kStrings; ++i) {
        const auto& s = strings_[i];
        if (note < s.openNote || note > s.openNote + 24) continue;

        // Prefer a free string, then a currently matching string, then the highest
        // sensible open string (smallest finger distance).
        int score = (note - s.openNote) * 10;
        if (s.active) score += 500;
        if (s.active && s.note == note) score -= 700;
        if (!s.active) score -= 250;

        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }

    if (best >= 0) return best;

    // Outside the ideal two-octave-per-string range: use the closest physically
    // reachable string rather than creating a fifth synthetic voice.
    for (int i = kStrings - 1; i >= 0; --i) {
        if (note >= strings_[i].openNote) return i;
    }
    return -1;
}

void PhysicalViolin::noteOn(int note, int velocity) {
    if (note < 55 || note > 100 || velocity <= 0) return;
    const int idx = chooseString(note);
    if (idx < 0) return;

    auto& s = strings_[idx];
    const bool legato = s.active;
    s.active = true;
    s.keyDown = true;
    s.pendingRelease = false;
    s.note = note;
    s.velocity = clampValue(velocity, 1, 127);
    s.targetFundamental = midiToHz(note);
    if (!legato) {
        s.fundamental = s.targetFundamental;
        s.bowEnvelope = 0.0f;
        s.frictionState = 0.0f;
        s.energyFollower = 0.0f;
        s.age = 0;
        s.vibratoPhase = 0.0;
        for (auto& m : s.modes) {
            m.y1 = 0.0f;
            m.y2 = 0.0f;
        }
    }
    s.coeffCountdown = 0;
}

void PhysicalViolin::noteOff(int note, bool sustainDown) {
    sustainDown_ = sustainDown;
    for (auto& s : strings_) {
        if (s.active && s.note == note && s.keyDown) {
            s.keyDown = false;
            if (sustainDown_) s.pendingRelease = true;
            else s.pendingRelease = false;
        }
    }
}

void PhysicalViolin::sustainChanged(bool down) {
    const bool wasDown = sustainDown_;
    sustainDown_ = down;
    if (wasDown && !down) {
        for (auto& s : strings_) {
            if (s.active && s.pendingRelease && !s.keyDown) {
                s.pendingRelease = false;
            }
        }
    }
}

void PhysicalViolin::allNotesOff() {
    sustainDown_ = false;
    for (auto& s : strings_) {
        s.keyDown = false;
        s.pendingRelease = false;
    }
}

void PhysicalViolin::polyPressure(int note, int value) {
    const int p = clampValue(value, 0, 127);
    for (auto& s : strings_) {
        if (s.active && s.note == note) s.pressure = p;
    }
}

void PhysicalViolin::channelPressure(int value) {
    channelPressure_ = clampValue(value, 0, 127);
}

void PhysicalViolin::pitchBend(int value14) {
    pitchBend_ = clampValue(value14, 0, 16383);
    for (auto& s : strings_) s.coeffCountdown = 0;
}

void PhysicalViolin::setBowPressure(float normalized) {
    bowPressure_ = clampValue(normalized, 0.0f, 1.0f);
}

void PhysicalViolin::setBowSpeed(float normalized) {
    bowSpeed_ = clampValue(normalized, 0.0f, 1.0f);
}

void PhysicalViolin::setBowPosition(float normalized) {
    bowPosition_ = clampValue(normalized, 0.0f, 1.0f);
    for (auto& s : strings_) s.coeffCountdown = 0;
}

void PhysicalViolin::setVibratoDepth(float normalized) {
    vibratoDepth_ = clampValue(normalized, 0.0f, 1.0f);
}

void PhysicalViolin::updateStringCoefficients(StringState& s) {
    const double bendSemis = (static_cast<double>(pitchBend_) - 8192.0) / 8192.0 * 2.0;
    const double vibCents = std::sin(s.vibratoPhase) * (22.0 * vibratoDepth_);
    const double pitchRatio = std::pow(2.0, bendSemis / 12.0 + vibCents / 1200.0);
    const double base = s.fundamental * pitchRatio;

    // bowPosition_ is defined as distance from bridge: 0 = almost at bridge,
    // 1 = toward the finger. Keep it inside a numerically useful physical range.
    const double fromBridge = 0.035 + 0.265 * bowPosition_;
    const double xBow = 1.0 - fromBridge;

    for (int i = 0; i < kMaxModes; ++i) {
        const int n = i + 1;
        // Tiny stiffness term gives bowed-string brightness without pushing the
        // upper partials far into inharmonic territory.
        const double stiffness = 1.0 + 0.000018 * n * n;
        const double f = base * n * stiffness;
        auto& m = s.modes[i];

        if (f >= std::min(15500.0, sampleRate_ * 0.43)) {
            m.enabled = false;
            continue;
        }

        const double damping = 1.8 + 0.20 * n + 0.0015 * f;
        const double r = std::exp(-damping / sampleRate_);
        const double w = 2.0 * kPi * f / sampleRate_;

        m.a1 = static_cast<float>(2.0 * r * std::cos(w));
        m.a2 = static_cast<float>(-r * r);
        m.phiBow = static_cast<float>(std::sin(n * kPi * xBow));
        m.excite = m.phiBow * (0.0038f / static_cast<float>(n));
        m.bridgeWeight = ((n & 1) ? -1.0f : 1.0f) *
                         std::min(1.0f, 0.075f * static_cast<float>(n));
        m.enabled = true;
    }
    s.coeffCountdown = 24;
}

float PhysicalViolin::processString(StringState& s) {
    if (!s.active) return 0.0f;

    // Finger movement changes effective string length. Smooth it just enough to
    // avoid zippering while retaining a genuine legato/glissando response.
    s.fundamental += (s.targetFundamental - s.fundamental) * 0.0035;

    const float pressureFromVelocity = 0.30f + 0.70f * (s.velocity / 127.0f);
    const int effectivePressure = s.pressure > 0 ? s.pressure : channelPressure_;
    const float pressureExpression = effectivePressure > 0
        ? (0.35f + 0.65f * (effectivePressure / 127.0f))
        : 1.0f;

    const bool bowed = s.keyDown || (s.pendingRelease && sustainDown_);
    const float targetBow = bowed ? pressureFromVelocity * pressureExpression : 0.0f;
    const float bowAttack = bowed ? 0.0038f : 0.0018f;
    s.bowEnvelope += (targetBow - s.bowEnvelope) * bowAttack;

    s.vibratoPhase += 2.0 * kPi * 5.35 / sampleRate_;
    if (s.vibratoPhase > 2.0 * kPi) s.vibratoPhase -= 2.0 * kPi;

    if (--s.coeffCountdown <= 0) updateStringCoefficients(s);

    float bowPointVelocity = 0.0f;
    for (const auto& m : s.modes) {
        if (!m.enabled) continue;
        const float modalVelocity = (m.y1 - m.y2) * static_cast<float>(sampleRate_);
        bowPointVelocity += modalVelocity * m.phiBow * 0.018f;
    }

    // Stateful stick/slip approximation. The velocity-dependent friction curve
    // deliberately has a high static region and a lower dynamic region.
    const float physicalBowSpeed = 0.035f + 0.46f * bowSpeed_;
    const float relativeVelocity = physicalBowSpeed - bowPointVelocity;
    const float ar = std::fabs(relativeVelocity);
    const float transition = 0.090f;
    const float muStatic = 0.92f;
    const float muDynamic = 0.24f;
    const float mu = muDynamic + (muStatic - muDynamic) *
        std::exp(-(ar * ar) / (transition * transition));
    const float direction = std::tanh(relativeVelocity * 28.0f);

    const float forceTarget =
        bowPressure_ * s.bowEnvelope * mu * direction;
    s.frictionState += (forceTarget - s.frictionState) * 0.16f;
    const float force = clampValue(s.frictionState, -1.2f, 1.2f);

    float bridge = 0.0f;
    float energy = 0.0f;

    for (auto& m : s.modes) {
        if (!m.enabled) continue;
        float y = m.a1 * m.y1 + m.a2 * m.y2 + force * m.excite;
        if (!std::isfinite(y)) y = 0.0f;
        y = clampValue(y, -2.0f, 2.0f);

        m.y2 = m.y1;
        m.y1 = y;

        bridge += y * m.bridgeWeight;
        energy += y * y;
    }

    bridge *= 0.30f;
    s.energyFollower += (energy - s.energyFollower) * 0.0025f;
    s.age++;

    // After bow release, let the string/body ring naturally before freeing it.
    if (!s.keyDown && !s.pendingRelease && s.bowEnvelope < 0.0005f &&
        s.energyFollower < 1.0e-7f && s.age > static_cast<uint64_t>(sampleRate_ * 0.10)) {
        s.active = false;
        s.note = -1;
        s.pressure = 0;
    }

    return clampValue(bridge, -1.5f, 1.5f);
}

float PhysicalViolin::processBody(float bridgeInput) {
    float sum = 0.0f;
    for (auto& m : body_) {
        float y = m.a1 * m.y1 + m.a2 * m.y2 + bridgeInput * m.gain;
        if (!std::isfinite(y)) y = 0.0f;
        y = clampValue(y, -3.0f, 3.0f);
        m.y2 = m.y1;
        m.y1 = y;
        sum += y;
    }

    // A little direct bridge component preserves articulation; the resonant term
    // supplies the shared wooden-body character for all four strings.
    const float out = bridgeInput * 0.42f + sum * 0.18f;
    return std::tanh(out * 1.8f);
}

float PhysicalViolin::process() {
    float bridge = 0.0f;
    for (auto& s : strings_) bridge += processString(s);

    if (!std::isfinite(bridge)) {
        reset();
        return 0.0f;
    }
    return processBody(clampValue(bridge, -2.0f, 2.0f));
}

int PhysicalViolin::activeVoices() const {
    int count = 0;
    for (const auto& s : strings_) if (s.active) ++count;
    return count;
}
