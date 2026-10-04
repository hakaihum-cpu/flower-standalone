#include "InstrumentModels.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

inline double wrapPhase(double p) {
    while (p >= 2.0 * kPi) p -= 2.0 * kPi;
    while (p < 0.0) p += 2.0 * kPi;
    return p;
}
}

double InstrumentModels::midiToHz(double note) {
    return 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
}

void InstrumentModels::prepare(double sampleRate) {
    sampleRate_ = sampleRate > 1000.0 ? sampleRate : 48000.0;
    reset();
}

void InstrumentModels::reset() {
    for (auto& v : voices_) v = Voice{};
    pitchBend_ = 8192;
    channelPressure_ = 0;
    sustainDown_ = false;
}

void InstrumentModels::setType(int type) {
    type_ = std::max(int(FLUTE), std::min(int(DRUMS), type));
    reset();

    switch (type_) {
        case FLUTE:
            attackMs_ = 28.0f; decayMs_ = 120.0f; sustain_ = 0.92f; releaseMs_ = 180.0f;
            break;
        case SAX:
            attackMs_ = 18.0f; decayMs_ = 95.0f; sustain_ = 0.94f; releaseMs_ = 220.0f;
            break;
        case FELT_PIANO:
            attackMs_ = 3.0f; decayMs_ = 820.0f; sustain_ = 0.18f; releaseMs_ = 950.0f;
            break;
        case ACCORDION:
            attackMs_ = 24.0f; decayMs_ = 90.0f; sustain_ = 0.96f; releaseMs_ = 240.0f;
            break;
        case XYLOPHONE:
            attackMs_ = 1.0f; decayMs_ = 420.0f; sustain_ = 0.0f; releaseMs_ = 180.0f;
            break;
        case WOOD_BASS:
            attackMs_ = 2.0f; decayMs_ = 520.0f; sustain_ = 0.38f; releaseMs_ = 520.0f;
            break;
        case DRUMS:
            attackMs_ = 1.0f; decayMs_ = 260.0f; sustain_ = 0.0f; releaseMs_ = 120.0f;
            break;
    }
}

int InstrumentModels::allocateVoice(int note) const {
    for (int i = 0; i < kVoices; ++i) {
        if (voices_[i].active && voices_[i].note == note) return i;
    }
    for (int i = 0; i < kVoices; ++i) {
        if (!voices_[i].active) return i;
    }

    int candidate = -1;
    float quietest = 10.0f;
    uint64_t oldest = 0;
    for (int i = 0; i < kVoices; ++i) {
        const auto& v = voices_[i];
        if (!v.keyDown && !v.pendingRelease) {
            if (v.env < quietest || (std::fabs(v.env - quietest) < 0.0001f && v.age > oldest)) {
                quietest = v.env;
                oldest = v.age;
                candidate = i;
            }
        }
    }
    if (candidate >= 0) return candidate;

    candidate = 0;
    oldest = voices_[0].age;
    for (int i = 1; i < kVoices; ++i) {
        if (voices_[i].age > oldest) {
            oldest = voices_[i].age;
            candidate = i;
        }
    }
    return candidate;
}

void InstrumentModels::noteOn(int note, int velocity) {
    if (note < 0 || note > 127 || velocity <= 0) return;

    const int index = allocateVoice(note);
    Voice& v = voices_[index];
    v = Voice{};
    v.active = true;
    v.keyDown = true;
    v.note = note;
    v.velocity = std::max(1, std::min(127, velocity));
    v.frequency = midiToHz(note);
    v.targetFrequency = v.frequency;
    v.envStage = 1;
    v.rng = 0x9E3779B9u ^ uint32_t(note * 2654435761u) ^ uint32_t(velocity * 2246822519u);

    initialiseModes(v);
    if (type_ == WOOD_BASS) initialiseBassDelay(v);
}

void InstrumentModels::noteOff(int note, bool sustainDown) {
    for (auto& v : voices_) {
        if (!v.active || v.note != note) continue;
        v.keyDown = false;
        v.pendingRelease = sustainDown;
        if (!sustainDown) v.envStage = 4;
    }
}

void InstrumentModels::sustainChanged(bool down) {
    sustainDown_ = down;
    if (!down) {
        for (auto& v : voices_) {
            if (v.active && v.pendingRelease && !v.keyDown) {
                v.pendingRelease = false;
                v.envStage = 4;
            }
        }
    }
}

void InstrumentModels::allNotesOff() {
    for (auto& v : voices_) v = Voice{};
}

void InstrumentModels::polyPressure(int note, int value) {
    for (auto& v : voices_) {
        if (v.active && v.note == note) v.pressure = std::max(0, std::min(127, value));
    }
}

void InstrumentModels::channelPressure(int value) {
    channelPressure_ = std::max(0, std::min(127, value));
}

void InstrumentModels::pitchBend(int value14) {
    pitchBend_ = std::max(0, std::min(16383, value14));
}

void InstrumentModels::setControl(int cc, float normalized) {
    normalized = clampf(normalized, 0.0f, 1.0f);
    if (cc == 1) vibrato_ = normalized;
    else if (cc == 10) control1_ = normalized;
    else if (cc == 11) control2_ = normalized;
    else if (cc == 74) control3_ = normalized;
}

void InstrumentModels::setAdsr(float a, float d, float s, float r) {
    attackMs_ = clampf(a, 0.0f, 5000.0f);
    decayMs_ = clampf(d, 0.0f, 5000.0f);
    sustain_ = clampf(s, 0.0f, 1.0f);
    releaseMs_ = clampf(r, 0.0f, 5000.0f);
}

float InstrumentModels::envelope(Voice& v) {
    auto rateForMs = [this](float ms) {
        if (ms <= 0.0f) return 1.0f;
        return 1.0f / std::max(1.0f, ms * 0.001f * float(sampleRate_));
    };

    switch (v.envStage) {
        case 1:
            v.env += rateForMs(attackMs_);
            if (v.env >= 1.0f) { v.env = 1.0f; v.envStage = 2; }
            break;
        case 2:
            v.env -= rateForMs(decayMs_) * (1.0f - sustain_);
            if (v.env <= sustain_) { v.env = sustain_; v.envStage = 3; }
            break;
        case 3:
            v.env = sustain_;
            if (!v.keyDown && !v.pendingRelease) v.envStage = 4;
            break;
        case 4:
            v.env -= rateForMs(releaseMs_);
            if (v.env <= 0.0f) { v.env = 0.0f; v.envStage = 0; v.active = false; }
            break;
        default:
            v.env = 0.0f;
            break;
    }
    return v.env;
}

float InstrumentModels::noise(Voice& v) {
    v.rng = v.rng * 1664525u + 1013904223u;
    return (float(int32_t(v.rng >> 8)) / 8388608.0f) - 1.0f;
}

void InstrumentModels::initialiseModes(Voice& v) {
    static constexpr float pianoRatios[kModes] = {1.0f, 2.002f, 3.008f, 4.018f, 5.032f, 6.052f, 7.075f, 8.105f};
    static constexpr float xyloRatios[kModes] = {1.0f, 3.99f, 9.02f, 16.1f, 25.2f, 36.4f, 49.0f, 64.0f};

    for (int i = 0; i < kModes; ++i) {
        v.modePhase[i] = 0.0;
        if (type_ == XYLOPHONE) {
            v.modeAmp[i] = (i == 0 ? 1.0f : (0.42f / (1.0f + i * 0.48f))) *
                           (1.0f + control1_ * 0.22f);
            v.aux1 += xyloRatios[i] * 0.0f;
        } else {
            v.modeAmp[i] = (i == 0 ? 1.0f : 0.46f / (1.0f + i * 0.60f));
            v.aux1 += pianoRatios[i] * 0.0f;
        }
    }
}

void InstrumentModels::initialiseBassDelay(Voice& v) {
    const double freq = std::max(28.0, v.frequency);
    v.delayLength = std::max(8, std::min(kDelay - 2, int(sampleRate_ / freq)));
    for (int i = 0; i < v.delayLength; ++i) {
        float n = noise(v);
        float shaped = n * (0.55f + 0.45f * control2_);
        v.delay[i] = shaped * (0.20f + 0.30f * (v.velocity / 127.0f));
    }
    v.delayWrite = 0;
    v.delayFilter = 0.0f;
}

float InstrumentModels::processFlute(Voice& v, double freq) {
    const float pressure = (v.pressure > 0 ? v.pressure : channelPressure_) / 127.0f;
    const float breath = clampf(0.18f + 0.72f * control2_ + 0.22f * pressure, 0.0f, 1.2f);
    const float edge = 0.25f + 0.70f * control1_;
    const float brightness = 0.12f + 0.62f * control3_;

    v.phase = wrapPhase(v.phase + 2.0 * kPi * freq / sampleRate_);
    v.phase2 = wrapPhase(v.phase2 + 2.0 * kPi * freq * 2.0 / sampleRate_);
    v.phase3 = wrapPhase(v.phase3 + 2.0 * kPi * freq * 3.0 / sampleRate_);

    float n = noise(v);
    v.noiseState += (n - v.noiseState) * (0.025f + brightness * 0.08f);
    float jet = std::sin(v.phase) + brightness * 0.22f * std::sin(v.phase2)
              + brightness * 0.07f * std::sin(v.phase3);
    jet = std::tanh(jet * (0.85f + edge * 0.75f));
    return 0.48f * breath * jet + 0.055f * breath * v.noiseState;
}

float InstrumentModels::processSax(Voice& v, double freq) {
    const float pressure = (v.pressure > 0 ? v.pressure : channelPressure_) / 127.0f;
    const float breath = clampf(0.18f + 0.78f * control2_ + 0.20f * pressure, 0.0f, 1.25f);
    const float reed = 0.60f + 2.8f * control1_;
    const float brightness = 0.20f + 0.80f * control3_;

    v.phase = wrapPhase(v.phase + 2.0 * kPi * freq / sampleRate_);
    v.phase2 = wrapPhase(v.phase2 + 2.0 * kPi * freq * 2.0 / sampleRate_);
    v.phase3 = wrapPhase(v.phase3 + 2.0 * kPi * freq * 3.0 / sampleRate_);

    float bore = std::sin(v.phase)
               + brightness * 0.42f * std::sin(v.phase2)
               + brightness * 0.20f * std::sin(v.phase3);
    float reedWave = std::tanh(bore * reed);
    float n = noise(v);
    v.noiseState += (n - v.noiseState) * 0.09f;
    return 0.42f * breath * reedWave + 0.035f * breath * v.noiseState;
}

float InstrumentModels::processFeltPiano(Voice& v, double freq) {
    static constexpr double ratios[kModes] = {1.0, 2.002, 3.008, 4.018, 5.032, 6.052, 7.075, 8.105};
    const float softness = 1.0f - control1_;
    const float tone = 0.20f + 0.80f * control3_;
    float sum = 0.0f;

    for (int i = 0; i < kModes; ++i) {
        double f = freq * ratios[i];
        v.modePhase[i] = wrapPhase(v.modePhase[i] + 2.0 * kPi * f / sampleRate_);
        const float highDamp = std::exp(-float(i) * (0.22f + softness * 0.55f));
        const float ageSec = float(v.age) / float(sampleRate_);
        const float decay = std::exp(-ageSec * (0.24f + i * (0.18f + softness * 0.16f)));
        sum += std::sin(v.modePhase[i]) * v.modeAmp[i] * highDamp * decay;
    }

    float n = noise(v);
    v.noiseState += (n - v.noiseState) * 0.18f;
    const float hammer = std::exp(-float(v.age) / float(sampleRate_) * 42.0f) *
                         v.noiseState * (0.035f + 0.10f * tone);
    return sum * 0.24f + hammer;
}

float InstrumentModels::processAccordion(Voice& v, double freq) {
    const float pressure = (v.pressure > 0 ? v.pressure : channelPressure_) / 127.0f;
    const float bellows = clampf(0.20f + 0.76f * control2_ + 0.18f * pressure, 0.0f, 1.2f);
    const float musette = 0.0015f + 0.009f * control3_;

    v.phase = wrapPhase(v.phase + 2.0 * kPi * freq * (1.0 - musette) / sampleRate_);
    v.phase2 = wrapPhase(v.phase2 + 2.0 * kPi * freq * (1.0 + musette) / sampleRate_);
    v.phase3 = wrapPhase(v.phase3 + 2.0 * kPi * freq / sampleRate_);

    auto reed = [](double p) {
        return float(std::sin(p) + 0.33 * std::sin(2.0 * p) + 0.15 * std::sin(3.0 * p));
    };
    float body = reed(v.phase) + reed(v.phase2) + 0.55f * reed(v.phase3);
    return body * bellows * 0.18f;
}

float InstrumentModels::processXylophone(Voice& v, double freq) {
    static constexpr double ratios[kModes] = {1.0, 3.99, 9.02, 16.1, 25.2, 36.4, 49.0, 64.0};
    float sum = 0.0f;
    const float hardness = control1_;
    const float ageSec = float(v.age) / float(sampleRate_);

    for (int i = 0; i < kModes; ++i) {
        const double f = freq * ratios[i];
        if (f > sampleRate_ * 0.45) continue;
        v.modePhase[i] = wrapPhase(v.modePhase[i] + 2.0 * kPi * f / sampleRate_);
        const float decayRate = 2.4f + i * (0.72f + 0.85f * (1.0f - control3_));
        const float decay = std::exp(-ageSec * decayRate);
        const float hardBoost = 1.0f + hardness * i * 0.12f;
        sum += std::sin(v.modePhase[i]) * v.modeAmp[i] * decay * hardBoost;
    }
    return sum * 0.34f;
}

float InstrumentModels::processWoodBass(Voice& v, double) {
    if (v.delayLength < 2) return 0.0f;
    const int next = (v.delayWrite + 1) % v.delayLength;
    const float a = v.delay[v.delayWrite];
    const float b = v.delay[next];

    const float damping = 0.985f - 0.035f * control1_;
    const float filtered = (a + b) * 0.5f * damping;
    v.delayFilter += (filtered - v.delayFilter) * (0.35f + control3_ * 0.45f);
    v.delay[v.delayWrite] = v.delayFilter;
    v.delayWrite = next;

    const float body = a + 0.18f * std::sin(v.phase);
    v.phase = wrapPhase(v.phase + 2.0 * kPi * v.frequency / sampleRate_);
    return body * 0.72f;
}

float InstrumentModels::processDrums(Voice& v, double freq) {
    const int n = v.note;
    const float ageSec = float(v.age) / float(sampleRate_);
    const int cls = ((n % 12) + 12) % 12;
    float out = 0.0f;

    if (n == 35 || n == 36 || cls == 0) {
        const double f = 46.0 + 92.0 * std::exp(-ageSec * 24.0);
        v.phase = wrapPhase(v.phase + 2.0 * kPi * f / sampleRate_);
        out = std::sin(v.phase) * std::exp(-ageSec * (5.0f + 4.0f * control1_)) * 0.95f;
    } else if (n == 38 || n == 40 || cls == 2 || cls == 7) {
        float nse = noise(v);
        v.noiseState += (nse - v.noiseState) * 0.42f;
        v.phase = wrapPhase(v.phase + 2.0 * kPi * (170.0 + 70.0 * control3_) / sampleRate_);
        out = (0.72f * v.noiseState + 0.28f * std::sin(v.phase)) *
              std::exp(-ageSec * (8.0f + 6.0f * control1_));
    } else if (n == 42 || n == 44 || n == 46 || cls == 6 || cls == 10) {
        float nse = noise(v);
        const float hp = nse - v.noiseState;
        v.noiseState += (nse - v.noiseState) * 0.08f;
        out = hp * std::exp(-ageSec * (18.0f + 18.0f * control1_)) * 0.62f;
    } else {
        const double base = std::max(70.0, std::min(320.0, freq * 0.38));
        v.phase = wrapPhase(v.phase + 2.0 * kPi * base / sampleRate_);
        v.phase2 = wrapPhase(v.phase2 + 2.0 * kPi * base * 1.47 / sampleRate_);
        out = (std::sin(v.phase) + 0.35f * std::sin(v.phase2)) *
              std::exp(-ageSec * (6.0f + 5.0f * control1_)) * 0.68f;
    }
    return out;
}

float InstrumentModels::processVoice(Voice& v) {
    if (!v.active) return 0.0f;

    const float env = envelope(v);
    if (!v.active || env <= 0.0f) return 0.0f;

    const double bendSemis = (double(pitchBend_) - 8192.0) / 8192.0 * 2.0;
    const float pressure = (v.pressure > 0 ? v.pressure : channelPressure_) / 127.0f;
    const double vibratoCents = std::sin(v.aux2) * (18.0 * vibrato_) * (0.35 + 0.65 * pressure);
    v.aux2 = float(wrapPhase(v.aux2 + 2.0 * kPi * 5.2 / sampleRate_));
    const double freq = v.frequency * std::pow(2.0, bendSemis / 12.0 + vibratoCents / 1200.0);

    float raw = 0.0f;
    switch (type_) {
        case FLUTE: raw = processFlute(v, freq); break;
        case SAX: raw = processSax(v, freq); break;
        case FELT_PIANO: raw = processFeltPiano(v, freq); break;
        case ACCORDION: raw = processAccordion(v, freq); break;
        case XYLOPHONE: raw = processXylophone(v, freq); break;
        case WOOD_BASS: raw = processWoodBass(v, freq); break;
        case DRUMS: raw = processDrums(v, freq); break;
        default: break;
    }

    v.age++;
    const float velocityGain = 0.30f + 0.70f * (v.velocity / 127.0f);
    return std::isfinite(raw) ? raw * env * velocityGain : 0.0f;
}

float InstrumentModels::process() {
    float sum = 0.0f;
    int active = 0;
    for (auto& v : voices_) {
        if (!v.active) continue;
        sum += processVoice(v);
        if (v.active) active++;
    }

    if (active > 1) sum *= 1.0f / std::sqrt(float(active));
    return std::tanh(sum * 0.92f);
}
