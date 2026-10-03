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

float PhysicalViolin::readDelay(const DelayLine& delay, float delaySamples) {
    delaySamples = clampValue(delaySamples, 1.0f, static_cast<float>(kDelaySize - 3));
    float readPos = static_cast<float>(delay.writeIndex) - delaySamples;
    while (readPos < 0.0f) readPos += static_cast<float>(kDelaySize);

    const int i0 = static_cast<int>(readPos) % kDelaySize;
    const int i1 = (i0 + 1) % kDelaySize;
    const float frac = readPos - std::floor(readPos);
    return delay.data[i0] + (delay.data[i1] - delay.data[i0]) * frac;
}

void PhysicalViolin::writeDelay(DelayLine& delay, float sample) {
    delay.data[delay.writeIndex] = sample;
    delay.writeIndex++;
    if (delay.writeIndex >= kDelaySize) delay.writeIndex = 0;
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

    // Shared body is deliberately subtle in v0.2. The previous version let the
    // body resonators dominate the string and sounded like resonance/noise.
    const float frequencies[kBodyModes] = {
        275.f, 390.f, 455.f, 525.f, 610.f, 720.f, 860.f,
        1040.f, 1250.f, 1510.f, 1850.f, 2250.f, 2750.f, 3350.f
    };
    const float gains[kBodyModes] = {
        0.055f, 0.050f, 0.064f, 0.070f, 0.066f, 0.058f, 0.052f,
        0.047f, 0.042f, 0.038f, 0.034f, 0.030f, 0.026f, 0.022f
    };

    for (int i = 0; i < kBodyModes; ++i) {
        const double f = frequencies[i];
        const double decaySeconds = 0.11 + 0.009 * i;
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
    // Retrigger the same pitch on the same voice before consuming a new voice.
    for (int i = 0; i < kStrings; ++i) {
        if (strings_[i].active && strings_[i].note == note) return i;
    }

    int best = -1;
    int bestScore = 1000000;

    // Prefer a truly free voice, loosely preserving G/D/A/E character.
    for (int i = 0; i < kStrings; ++i) {
        const auto& voice = strings_[i];
        if (voice.active) continue;
        const int distance = std::abs(note - voice.openNote);
        if (distance < bestScore) {
            bestScore = distance;
            best = i;
        }
    }
    if (best >= 0) return best;

    // Next steal a voice already in release / no longer held.
    best = -1;
    for (int i = 0; i < kStrings; ++i) {
        const auto& voice = strings_[i];
        if (voice.keyDown || voice.pendingRelease) continue;
        if (best < 0 || voice.ampEnv < strings_[best].ampEnv ||
            (voice.ampEnv == strings_[best].ampEnv && voice.age > strings_[best].age)) {
            best = i;
        }
    }
    if (best >= 0) return best;

    // Only when all four are genuinely held: steal the oldest.
    best = 0;
    for (int i = 1; i < kStrings; ++i) {
        if (strings_[i].age > strings_[best].age) best = i;
    }
    return best;
}

void PhysicalViolin::noteOn(int note, int velocity) {
    if (note < 55 || note > 108 || velocity <= 0) return;
    const int idx = chooseString(note);
    if (idx < 0) return;

    auto& s = strings_[idx];
    const bool legato = s.active && s.note == note;

    s.active = true;
    s.keyDown = true;
    s.pendingRelease = false;
    s.note = note;
    s.velocity = clampValue(velocity, 1, 127);
    s.targetFundamental = midiToHz(note);
    s.ampStage = 1;
    s.startupAssist = 1.0f;

    if (!legato) {
        const int open = s.openNote;
        s = StringState{};
        s.openNote = open;
        s.active = true;
        s.keyDown = true;
        s.note = note;
        s.velocity = clampValue(velocity, 1, 127);
        s.fundamental = s.targetFundamental = midiToHz(note);
        s.ampEnv = 0.0f;
        s.ampStage = 1;
        s.startupAssist = 1.0f;

        // A deterministic displacement seeds the travelling wave. It is not a
        // sample layer; sustained energy still comes only from bow interaction.
        const float seed = 0.012f * (0.35f + 0.65f * (s.velocity / 127.0f));
        writeDelay(s.bridgeDelay, seed);
        writeDelay(s.neckDelay, -seed);
    }
}

void PhysicalViolin::noteOff(int note, bool sustainDown) {
    sustainDown_ = sustainDown;
    for (auto& s : strings_) {
        if (s.active && s.note == note && s.keyDown) {
            s.keyDown = false;
            s.pendingRelease = sustainDown_;
            if (!sustainDown_) s.ampStage = 4;
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
                s.ampStage = 4;
            }
        }
    }
}

void PhysicalViolin::allNotesOff() {
    sustainDown_ = false;
    for (auto& s : strings_) {
        const int open = s.openNote;
        s = StringState{};
        s.openNote = open;
        s.fundamental = midiToHz(open);
        s.targetFundamental = s.fundamental;
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
}

void PhysicalViolin::setBowPressure(float normalized) {
    bowPressure_ = clampValue(normalized, 0.0f, 1.0f);
}

void PhysicalViolin::setBowSpeed(float normalized) {
    bowSpeed_ = clampValue(normalized, 0.0f, 1.0f);
}

void PhysicalViolin::setBowPosition(float normalized) {
    bowPosition_ = clampValue(normalized, 0.0f, 1.0f);
}

void PhysicalViolin::setVibratoDepth(float normalized) {
    vibratoDepth_ = clampValue(normalized, 0.0f, 1.0f);
}

void PhysicalViolin::setAdsr(float attackMs, float decayMs, float sustain, float releaseMs) {
    attackMs_ = clampValue(attackMs, 0.0f, 5000.0f);
    decayMs_ = clampValue(decayMs, 0.0f, 5000.0f);
    sustain_ = clampValue(sustain, 0.0f, 1.0f);
    releaseMs_ = clampValue(releaseMs, 0.0f, 5000.0f);
}

float PhysicalViolin::processAmpEnvelope(StringState& s) {
    constexpr uint8_t OFF = 0, ATTACK = 1, DECAY = 2, SUSTAIN = 3, RELEASE = 4;

    switch (s.ampStage) {
        case ATTACK: {
            if (attackMs_ <= 0.1f) {
                s.ampEnv = 1.0f;
                s.ampStage = DECAY;
            } else {
                s.ampEnv += 1000.0f / (attackMs_ * static_cast<float>(sampleRate_));
                if (s.ampEnv >= 1.0f) {
                    s.ampEnv = 1.0f;
                    s.ampStage = DECAY;
                }
            }
            break;
        }
        case DECAY: {
            if (decayMs_ <= 0.1f || sustain_ >= 0.9999f) {
                s.ampEnv = sustain_;
                s.ampStage = SUSTAIN;
            } else {
                const float step = (1.0f - sustain_) * 1000.0f /
                    (decayMs_ * static_cast<float>(sampleRate_));
                s.ampEnv -= step;
                if (s.ampEnv <= sustain_) {
                    s.ampEnv = sustain_;
                    s.ampStage = SUSTAIN;
                }
            }
            break;
        }
        case SUSTAIN:
            s.ampEnv = sustain_;
            break;
        case RELEASE: {
            if (releaseMs_ <= 0.1f) {
                s.ampEnv = 0.0f;
                s.ampStage = OFF;
            } else {
                s.ampEnv -= 1000.0f / (releaseMs_ * static_cast<float>(sampleRate_));
                if (s.ampEnv <= 0.0f) {
                    s.ampEnv = 0.0f;
                    s.ampStage = OFF;
                }
            }
            break;
        }
        default:
            s.ampEnv = 0.0f;
            break;
    }
    return clampValue(s.ampEnv, 0.0f, 1.0f);
}

float PhysicalViolin::processString(StringState& s) {
    if (!s.active) return 0.0f;

    const float amp = processAmpEnvelope(s);
    if (!s.keyDown && !s.pendingRelease && s.ampStage == 0 && amp <= 0.0f) {
        const int open = s.openNote;
        s = StringState{};
        s.openNote = open;
        s.fundamental = midiToHz(open);
        s.targetFundamental = s.fundamental;
        return 0.0f;
    }

    // Finger movement changes the physical round-trip delay rather than shifting
    // an already-generated tone.
    s.fundamental += (s.targetFundamental - s.fundamental) * 0.0028;

    s.vibratoPhase += 2.0 * kPi * 5.35 / sampleRate_;
    if (s.vibratoPhase >= 2.0 * kPi) s.vibratoPhase -= 2.0 * kPi;

    const double bendSemis =
        (static_cast<double>(pitchBend_) - 8192.0) / 8192.0 * 2.0;
    const double vibCents =
        std::sin(s.vibratoPhase) * (18.0 * vibratoDepth_);
    const double frequency =
        s.fundamental * std::pow(2.0, bendSemis / 12.0 + vibCents / 1200.0);

    // One full waveguide round trip is approximately Fs/f0. A small correction
    // accounts for the reflection filter phase delay.
    float totalDelay = static_cast<float>(sampleRate_ / std::max(40.0, frequency) - 1.6);
    totalDelay = clampValue(totalDelay, 6.0f, static_cast<float>(kDelaySize - 8));

    // Distance from bridge. Keep the bow away from either exact end.
    const float bridgeFraction = 0.045f + 0.255f * bowPosition_;
    float bridgeSamples = totalDelay * bridgeFraction;
    bridgeSamples = clampValue(bridgeSamples, 2.0f, totalDelay - 2.0f);
    const float neckSamples = std::max(2.0f, totalDelay - bridgeSamples);

    const float bridgeArrival = readDelay(s.bridgeDelay, bridgeSamples);
    const float nutArrival = readDelay(s.neckDelay, neckSamples);

    // Lossy bridge reflection removes the metallic infinite-ring behaviour of
    // the first modal prototype. Nut reflection remains nearly rigid.
    s.bridgeFilter += (bridgeArrival - s.bridgeFilter) * 0.20f;
    const float bridgeReflected = -0.982f * s.bridgeFilter;
    const float nutReflected = -0.996f * nutArrival;

    const float stringVelocity = bridgeReflected + nutReflected;

    const int effectivePressure = s.pressure > 0 ? s.pressure : channelPressure_;
    const float pressureExpression = effectivePressure > 0
        ? (0.40f + 0.60f * (effectivePressure / 127.0f))
        : 1.0f;
    const float velocityExpression = 0.35f + 0.65f * (s.velocity / 127.0f);

    const bool bowed = s.keyDown || (s.pendingRelease && sustainDown_);
    const float targetBow = bowed ? velocityExpression * pressureExpression : 0.0f;
    const float envelopeRate = bowed ? 0.0045f : 0.0016f;
    s.bowEnvelope += (targetBow - s.bowEnvelope) * envelopeRate;

    // The bow velocity is a physical control, not output gain.
    const float bowVelocity = 0.025f + 0.34f * bowSpeed_;
    const float relativeVelocity = bowVelocity - stringVelocity;

    // Stable nonlinear friction curve. Near zero relative velocity the bow
    // sticks strongly; as slip velocity rises coupling falls. This gives the
    // delay loop a periodic bowed-string source instead of broadband excitation.
    const float pressure = clampValue(bowPressure_ * s.bowEnvelope, 0.0f, 1.2f);
    const float absRel = std::fabs(relativeVelocity);
    const float slope = 3.2f + (1.0f - pressure) * 3.0f;
    const float tableArg = std::max(0.75f, 0.75f + absRel * slope);
    float frictionGain = std::pow(tableArg, -4.0f);
    frictionGain = clampValue(frictionGain, 0.0f, 1.0f);

    // No bow pressure means no energy injection. The previous prototype kept
    // a constant 0.28 coupling term here, so even after Note Off the virtual
    // bow could continue feeding the delay loop indefinitely.
    // For the first few tens of milliseconds, guarantee that every newly
    // allocated physical voice enters the stick/slip regime. Without this,
    // some pitches could receive a voice but remain effectively non-speaking.
    const float startupCoupling = 0.20f * s.startupAssist;
    const float coupling = 1.12f * pressure + startupCoupling;
    float junctionVelocity =
        relativeVelocity * frictionGain * coupling;

    const float startupDecay = std::exp(-1.0f / (0.025f * static_cast<float>(sampleRate_)));
    s.startupAssist *= startupDecay;
    if (s.startupAssist < 0.0001f) s.startupAssist = 0.0f;

    const float frictionSlew = bowed ? 0.24f : 0.42f;
    s.frictionState += (junctionVelocity - s.frictionState) * frictionSlew;
    if (!bowed && s.bowEnvelope < 0.0010f && std::fabs(s.frictionState) < 0.00005f) {
        s.frictionState = 0.0f;
    }
    junctionVelocity = clampValue(s.frictionState, -0.55f, 0.55f);

    float towardBridge = nutReflected + junctionVelocity;
    float towardNut = bridgeReflected + junctionVelocity;

    towardBridge = clampValue(towardBridge, -0.95f, 0.95f);
    towardNut = clampValue(towardNut, -0.95f, 0.95f);

    writeDelay(s.bridgeDelay, towardBridge);
    writeDelay(s.neckDelay, towardNut);

    // The bridge-arriving travelling wave is the actual string signal.
    // Keep a small differentiated component for bow articulation, but the
    // periodic string component remains dominant.
    const float bridgeDelta = bridgeArrival - s.lastBridge;
    s.lastBridge = bridgeArrival;
    float bridgeSignal = bridgeArrival * 0.92f + bridgeDelta * 0.20f;

    // A shorter waveguide stores less displacement energy per cycle, so the
    // unnormalised model gets progressively quieter as pitch rises. Compensate
    // at the string/bridge boundary rather than with a global EQ so each voice
    // reaches the shared body at a comparable playing level.
    const float referenceHz = static_cast<float>(midiToHz(60.0)); // C4
    const float pitchLevelComp = clampValue(
        std::sqrt(static_cast<float>(frequency) / referenceHz),
        0.82f, 2.35f);
    bridgeSignal *= pitchLevelComp;
    bridgeSignal = clampValue(bridgeSignal, -1.0f, 1.0f);

    const float energy = bridgeSignal * bridgeSignal;
    s.energyFollower += (energy - s.energyFollower) * 0.0015f;
    s.age++;

    if (!std::isfinite(bridgeSignal)) {
        const int open = s.openNote;
        s = StringState{};
        s.openNote = open;
        s.fundamental = midiToHz(open);
        s.targetFundamental = s.fundamental;
        return 0.0f;
    }

    return bridgeSignal * amp;
}

float PhysicalViolin::processBody(float bridgeInput) {
    float bodySum = 0.0f;

    for (auto& m : body_) {
        float y = m.a1 * m.y1 + m.a2 * m.y2 + bridgeInput * m.gain;
        if (!std::isfinite(y)) y = 0.0f;
        y = clampValue(y, -1.5f, 1.5f);
        m.y2 = m.y1;
        m.y1 = y;
        bodySum += y;
    }

    // Dry string is intentionally dominant. Body is coloration, not the source.
    const float out = bridgeInput * 0.90f + bodySum * 0.045f;
    return std::tanh(out * 1.10f);
}

float PhysicalViolin::process() {
    float bridge = 0.0f;
    int sounding = 0;
    for (auto& s : strings_) {
        const float v = processString(s);
        bridge += v;
        if (s.active && s.ampEnv > 0.0001f) sounding++;
    }

    if (!std::isfinite(bridge)) {
        reset();
        return 0.0f;
    }

    const float polyNorm = sounding > 1 ? 1.0f / std::sqrt(static_cast<float>(sounding)) : 1.0f;
    bridge = clampValue(bridge * 0.72f * polyNorm, -1.35f, 1.35f);
    return processBody(bridge);
}

int PhysicalViolin::activeVoices() const {
    int count = 0;
    for (const auto& s : strings_) if (s.active) ++count;
    return count;
}
