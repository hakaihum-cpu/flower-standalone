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
            attackMs_ = 22.0f; decayMs_ = 90.0f; sustain_ = 0.96f; releaseMs_ = 160.0f;
            break;
        case SAX:
            attackMs_ = 16.0f; decayMs_ = 80.0f; sustain_ = 0.97f; releaseMs_ = 210.0f;
            break;
        case FELT_PIANO:
            attackMs_ = 1.0f; decayMs_ = 5000.0f; sustain_ = 1.0f; releaseMs_ = 900.0f;
            break;
        case ACCORDION:
            attackMs_ = 20.0f; decayMs_ = 80.0f; sustain_ = 0.98f; releaseMs_ = 220.0f;
            break;
        case XYLOPHONE:
            attackMs_ = 1.0f; decayMs_ = 5000.0f; sustain_ = 1.0f; releaseMs_ = 220.0f;
            break;
        case WOOD_BASS:
            // The string loop supplies the natural decay.  The amplitude envelope
            // only handles note articulation/damping when the player releases.
            attackMs_ = 1.0f; decayMs_ = 5000.0f; sustain_ = 1.0f; releaseMs_ = 180.0f;
            break;
        case DRUMS:
            attackMs_ = 1.0f; decayMs_ = 5000.0f; sustain_ = 1.0f; releaseMs_ = 120.0f;
            break;
    }
}

int InstrumentModels::allocateVoice(int note) const {
    for (int i=0; i<kVoices; ++i) {
        if (voices_[i].active && voices_[i].note == note) return i;
    }
    for (int i=0; i<kVoices; ++i) {
        if (!voices_[i].active) return i;
    }

    int candidate = -1;
    float quietest = 10.0f;
    uint64_t oldest = 0;
    for (int i=0; i<kVoices; ++i) {
        const auto& v = voices_[i];
        if (!v.keyDown && !v.pendingRelease) {
            if (v.env < quietest ||
                (std::fabs(v.env - quietest) < 0.0001f && v.age > oldest)) {
                quietest = v.env;
                oldest = v.age;
                candidate = i;
            }
        }
    }
    if (candidate >= 0) return candidate;

    candidate = 0;
    oldest = voices_[0].age;
    for (int i=1; i<kVoices; ++i) {
        if (voices_[i].age > oldest) {
            oldest = voices_[i].age;
            candidate = i;
        }
    }
    return candidate;
}

void InstrumentModels::noteOn(int note, int velocity) {
    if (note < 0 || note > 127 || velocity <= 0) return;
    if (type_ == DRUMS && note != 60 && note != 61 && note != 62) return;

    const int index = allocateVoice(note);
    Voice& v = voices_[index];
    v = Voice{};
    v.active = true;
    v.keyDown = true;
    v.note = note;
    v.velocity = std::max(1, std::min(127, velocity));
    v.frequency = midiToHz(note);
    v.envStage = 1;
    v.rng = 0x9E3779B9u ^ uint32_t(note * 2654435761u) ^ uint32_t(velocity * 2246822519u);

    if (type_ == FELT_PIANO) initialisePiano(v);
    else if (type_ == XYLOPHONE) initialiseXylophone(v);
    else if (type_ == WOOD_BASS) initialiseWoodBass(v);
    else if (type_ == DRUMS) initialiseDrums(v);
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
    if (type_ == DRUMS) {
        if (cc == 10) drumParams_[0] = normalized;          // Kick tune
        else if (cc == 11) drumParams_[4] = normalized;     // Hi-hat tune
        else if (cc == 74) drumParams_[8] = normalized;     // Snare tune
        else if (cc == 1) {                                 // Global decay convenience
            drumParams_[1] = normalized;
            drumParams_[5] = normalized;
            drumParams_[9] = normalized;
        }
        return;
    }
    if (cc == 1) vibrato_ = normalized;
    else if (cc == 10) control1_ = normalized;
    else if (cc == 11) control2_ = normalized;
    else if (cc == 74) control3_ = normalized;
}

void InstrumentModels::setDrumParameter(int parameter, float normalized) {
    if (parameter < 0 || parameter >= int(drumParams_.size())) return;
    drumParams_[parameter] = clampf(normalized, 0.0f, 1.0f);
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
            if (v.env <= 0.0f) {
                v.env = 0.0f;
                v.envStage = 0;
                v.active = false;
            }
            break;
        default:
            v.env = 0.0f;
            break;
    }
    return v.env;
}

float InstrumentModels::noise(Voice& v) {
    v.rng = v.rng * 1664525u + 1013904223u;
    return (float(v.rng >> 8) / 8388608.0f) - 1.0f;
}

float InstrumentModels::readDelay(const std::array<float,kDelay>& buffer,
                                  int writeIndex,
                                  float delaySamples) {
    delaySamples = clampf(delaySamples, 1.0f, float(kDelay - 3));
    float read = float(writeIndex) - delaySamples;
    while (read < 0.0f) read += kDelay;

    const int i0 = int(read) % kDelay;
    const int i1 = (i0 + 1) % kDelay;
    const float frac = read - std::floor(read);
    return buffer[i0] + (buffer[i1] - buffer[i0]) * frac;
}

void InstrumentModels::writeDelay(std::array<float,kDelay>& buffer,
                                  int& writeIndex,
                                  float value) {
    buffer[writeIndex] = value;
    writeIndex++;
    if (writeIndex >= kDelay) writeIndex = 0;
}

void InstrumentModels::setupMode(Voice& v,
                                 int index,
                                 double frequency,
                                 float decaySeconds,
                                 float gain) {
    if (index < 0 || index >= kModes) return;
    if (frequency <= 0.0 || frequency >= sampleRate_ * 0.47 || gain == 0.0f) {
        v.modeA1[index] = 0.0f;
        v.modeA2[index] = 0.0f;
        v.modeGain[index] = 0.0f;
        v.modeY1[index] = 0.0f;
        v.modeY2[index] = 0.0f;
        return;
    }
    frequency = std::max(10.0, frequency);
    decaySeconds = std::max(0.015f, decaySeconds);

    const float r = std::exp(-1.0f / (decaySeconds * float(sampleRate_)));
    const float w = float(2.0 * kPi * frequency / sampleRate_);
    v.modeA1[index] = 2.0f * r * std::cos(w);
    v.modeA2[index] = -(r * r);
    v.modeGain[index] = gain;
    v.modeY1[index] = 0.0f;
    v.modeY2[index] = 0.0f;
}

float InstrumentModels::tickMode(Voice& v, int index, float excitation) {
    const float y = v.modeA1[index] * v.modeY1[index]
                  + v.modeA2[index] * v.modeY2[index]
                  + v.modeGain[index] * excitation;
    v.modeY2[index] = v.modeY1[index];
    v.modeY1[index] = y;
    return y;
}

void InstrumentModels::initialisePiano(Voice& v) {
    const float noteNorm = clampf((v.note - 21) / 87.0f, 0.0f, 1.0f);
    const float inharmonicity = 0.00012f + 0.00135f * noteNorm * noteNorm;
    const float softness = 1.0f - control1_;
    const float hammerPos = 0.11f + 0.16f * control3_;
    const float treble = clampf((v.note - 72) / 24.0f, 0.0f, 1.0f);
    const float modalScale = 0.0009f *
            clampf(float(v.frequency / 261.625565), 0.15f, 8.0f);

    for (int i=0; i<kModes; ++i) {
        const int n = i + 1;
        double partial = v.frequency * n *
                std::sqrt(1.0 + inharmonicity * n * n);

        // Three-string beating is approximated by tiny alternating detunes in
        // the upper scale while preserving the stiff-string modal law.
        if (v.note >= 48) {
            const float detune = ((i % 3) - 1) * (0.00025f + 0.00045f * noteNorm);
            partial *= (1.0 + detune);
        }

        const float positionCoupling = std::fabs(std::sin(float(kPi) * n * hammerPos));
        // The fixed felt low-pass used previously was too aggressive in the
        // upper register. Treble strings have shorter contact and need a
        // slightly more open modal excitation to retain a piano-like body.
        const float spectralSlope = 0.18f + softness * 0.34f - 0.06f * treble;
        const float spectral = std::exp(-i * spectralSlope);
        const float decay = std::max(0.22f, 5.5f / (1.0f + i * (0.55f + 0.28f * noteNorm)));
        setupMode(v, i, partial, decay, modalScale * positionCoupling * spectral);
    }
}

void InstrumentModels::initialiseXylophone(Voice& v) {
    // Free-free bar modes, nudged toward the deliberately tuned first modes
    // of a xylophone bar. Higher modes remain strongly inharmonic.
    static constexpr float ratios[kModes] = {
        1.000f, 3.000f, 5.88f, 8.93f, 13.34f, 18.65f,
        24.84f, 31.91f, 39.87f, 48.73f, 58.48f, 69.13f
    };

    const float strikePos = 0.12f + 0.30f * control3_;
    const float hardness = control1_;
    const float modalScale = 0.0026f *
            clampf(float(v.frequency / 261.625565), 0.20f, 5.0f);

    for (int i=0; i<kModes; ++i) {
        const double f = v.frequency * ratios[i];
        const float nodeWeight =
                std::fabs(std::sin(float(kPi) * (i + 1) * strikePos));
        const float hardBoost = std::pow(0.54f + 0.75f * hardness, float(i) * 0.34f);
        const float decay = std::max(0.035f, 1.75f / (1.0f + i * 0.52f));
        setupMode(v, i, f, decay,
                  modalScale * nodeWeight * hardBoost / (1.0f + 0.18f * i));
    }
}

void InstrumentModels::initialiseWoodBass(Voice& v) {
    const double freq = std::max(24.0, v.frequency);

    // Seed one period of the displacement wave immediately behind the write
    // head.  processWoodBass() reads this ring with a fractional delay, so
    // pitch bend and vibrato can continuously alter the effective string length.
    const float nominalDelay = clampf(float(sampleRate_ / freq - 0.75),
                                      8.0f, float(kDelay - 4));
    const int seedLength = std::max(8, std::min(kDelay - 4,
            int(std::ceil(nominalDelay)) + 2));
    v.delayLengthA = seedLength;
    v.writeA = 0;

    // Finger plucks are closer to a triangular displacement than a noise burst.
    // PLUCK POSITION controls the spectral nulls, while PLUCK FORCE and
    // velocity determine displacement amplitude.
    const float pluckPos = 0.08f + 0.34f * control3_;
    const float velocity = v.velocity / 127.0f;
    const float force = (0.18f + 0.58f * control2_) *
                        (0.32f + 0.68f * velocity);

    float mean = 0.0f;
    const int start = kDelay - seedLength;
    for (int i=0; i<seedLength; ++i) {
        const float x = i / float(std::max(1, seedLength - 1));
        float displacement;
        if (x <= pluckPos) displacement = x / std::max(0.01f, pluckPos);
        else displacement = (1.0f - x) / std::max(0.01f, 1.0f - pluckPos);
        displacement = displacement * 2.0f - 1.0f;

        // A tiny deterministic roughness prevents every note from having the
        // same mathematically perfect attack without turning the string noisy.
        const float roughness = noise(v) * (0.006f + 0.012f * velocity);
        v.delayA[start + i] = (displacement + roughness) * force;
        mean += v.delayA[start + i];
    }
    mean /= float(seedLength);
    for (int i=0; i<seedLength; ++i) v.delayA[start + i] -= mean;

    // Double-bass body/air and bridge-admittance landmarks.  The first pair
    // approximates A0/T1 around 60/100 Hz; the upper modes broaden the wooden
    // corpus response and the characteristic bridge/body regions toward 1 kHz.
    setupMode(v, 0,   62.0, 0.62f, 0.000018f);
    setupMode(v, 1,  100.0, 0.52f, 0.000020f);
    setupMode(v, 2,  145.0, 0.38f, 0.000013f);
    setupMode(v, 3,  190.0, 0.31f, 0.000010f);
    setupMode(v, 4,  275.0, 0.24f, 0.000007f);
    setupMode(v, 5,  400.0, 0.18f, 0.000005f);
    setupMode(v, 6,  700.0, 0.11f, 0.0000028f);
    setupMode(v, 7, 1000.0, 0.085f, 0.0000018f);
}

void InstrumentModels::initialiseDrums(Voice& v) {
    static constexpr float membrane[kModes] = {
        1.000f, 1.593f, 2.136f, 2.296f, 2.653f, 2.918f,
        3.156f, 3.500f, 3.600f, 3.652f, 4.060f, 4.153f
    };
    static constexpr float plate[kModes] = {
        1.000f, 1.480f, 2.090f, 2.660f, 3.420f, 4.170f,
        5.010f, 5.940f, 6.920f, 8.030f, 9.180f, 10.400f
    };

    const int n = v.note;
    const bool kick = (n == 60);
    const bool hat = (n == 61);
    const bool snare = (n == 62);

    auto tuneRatio = [](float value) {
        const float semitones = (clampf(value, 0.0f, 1.0f) - 0.5f) * 24.0f;
        return std::pow(2.0f, semitones / 12.0f);
    };

    const float tune = kick ? drumParams_[0] : (hat ? drumParams_[4] : drumParams_[8]);
    const float decayParam = kick ? drumParams_[1] : (hat ? drumParams_[5] : drumParams_[9]);
    const float color = hat ? drumParams_[6] : 0.5f;

    double base = 180.0;
    if (kick) base = 52.0 * tuneRatio(tune);
    else if (hat) base = (620.0 + 620.0 * color) * tuneRatio(tune);
    else if (snare) base = 190.0 * tuneRatio(tune);

    const float decayScale = 0.38f + 2.55f * decayParam;
    const float membraneScale = clampf(float(base / 3500.0), 0.012f, 0.12f);

    for (int i=0; i<kModes; ++i) {
        float ratio = hat ? plate[i] : membrane[i];
        if (hat) ratio *= 0.82f + color * (0.18f + 0.035f * i);

        const double f = base * ratio;
        const float decay = kick
                ? std::max(0.045f, (0.92f * decayScale) / (1.0f + i * 0.48f))
                : snare
                    ? std::max(0.028f, (0.52f * decayScale) / (1.0f + i * 0.34f))
                    : std::max(0.018f, (0.32f * decayScale) / (1.0f + i * 0.16f));
        const float gain = (hat ? (0.0025f + 0.0035f * color)
                                : 0.0085f * membraneScale) /
                           (1.0f + i * (hat ? 0.09f : 0.18f));
        setupMode(v, i, f, decay, gain);
    }
}
float InstrumentModels::processFlute(Voice& v, double freq, float env) {
    // Stable jet-drive / open-bore waveguide.
    //
    // The previous version tuned the bore from freq*0.66666 and returned only
    // a weak bore tap. On RG Rotate this could settle into mostly turbulent
    // breath instead of a clear pitched oscillation. The bore is now tuned to
    // the played fundamental and receives a stronger, filtered open-end return.
    const float vel = v.velocity / 127.0f;
    const float pressure = (v.pressure > 0 ? v.pressure : channelPressure_) / 127.0f;

    const float breathGain = (0.46f + 0.46f * vel) *
                             (0.56f + 0.58f * control2_ + 0.12f * pressure);
    float breath = env * breathGain;

    // Breath noise remains audible but subordinate to the resonating bore.
    const float n = noise(v);
    const float noiseGain = 0.010f + 0.055f * control1_;
    const float vibGain = 0.015f + 0.10f * vibrato_;
    breath *= 1.0f + noiseGain * n
            + vibGain * std::sin(v.vibratoPhase);

    // One round trip per played period, with a small phase allowance for the
    // reflection filter. Fractional readDelay keeps bend/vibrato continuous.
    const float boreDelay = clampf(
            float(sampleRate_ / std::max(35.0, freq) - 1.35),
            3.0f, float(kDelay - 4));

    // Embouchure/jet geometry changes the delay from mouth to edge, not the
    // acoustic bore pitch itself.
    const float jetRatio = 0.16f + 0.24f * control3_;
    const float jetDelay = clampf(boreDelay * jetRatio, 1.0f, float(kDelay - 4));

    const float boreOut = readDelay(v.delayA, v.writeA, boreDelay);

    // Open-end reflection: low-pass losses suppress unstable upper partials
    // while retaining enough loop gain to sustain a flute tone.
    const float reflectionRate = 0.22f + 0.18f * (1.0f - control1_);
    v.filter1 += (boreOut - v.filter1) * reflectionRate;
    const float reflected = -v.filter1 * (0.86f + 0.07f * control2_);

    // The delayed pressure difference hits the edge non-linearity.
    const float pressureDiff = clampf(breath + 0.42f * reflected, -1.35f, 1.35f);
    const float jetOld = readDelay(v.delayB, v.writeB, jetDelay);
    writeDelay(v.delayB, v.writeB, pressureDiff);

    // Smooth odd jet transfer. This sign reinforces the intended bore mode
    // instead of cancelling it at the operating point.
    const float x = clampf(jetOld, -1.20f, 1.20f);
    float jet = x * (1.0f - x * x);
    jet = clampf(jet, -0.85f, 0.85f);

    // A short tonal seed starts the self-oscillation reliably, then vanishes.
    v.phase = wrapPhase(v.phase + 2.0 * kPi * freq / sampleRate_);
    const float ageSec = float(v.age) / float(sampleRate_);
    const float startSeed = std::sin(v.phase) * std::exp(-ageSec * 30.0f)
                          * (0.018f + 0.018f * vel);

    const float boreIn =
            0.10f * breath
            + 0.92f * jet
            + reflected
            + startSeed;
    writeDelay(v.delayA, v.writeA, clampf(boreIn, -1.25f, 1.25f));

    // Radiated pressure is primarily the resonant bore; keep only a very small
    // air component so BREATH still changes character without becoming the sound.
    v.filter2 += (boreOut - v.filter2) * 0.035f;
    const float tone = boreOut - 0.08f * v.filter2;
    const float air = n * breath * (0.006f + 0.010f * control1_);
    return clampf(0.68f * tone + air, -1.0f, 1.0f);
}

float InstrumentModels::processSax(Voice& v, double freq, float env) {
    // Two-section bore + memoryless nonlinear single-reed reflection.
    const float vel = v.velocity / 127.0f;
    const float pressure = (v.pressure > 0 ? v.pressure : channelPressure_) / 127.0f;
    float breath = env * (0.50f + 0.34f * vel) *
                   (0.55f + 0.72f * control2_ + 0.18f * pressure);
    breath += breath * (0.035f + 0.11f * control3_) * noise(v);
    breath += breath * (0.02f + 0.18f * vibrato_) * std::sin(v.vibratoPhase);

    const float totalDelay = clampf(float(sampleRate_ / std::max(35.0, freq) - 1.0),
                                    3.0f, float(kDelay - 4));
    const float position = 0.08f + 0.30f * control3_;
    const float delay0 = clampf((1.0f - position) * totalDelay, 1.0f, float(kDelay - 4));
    const float delay1 = clampf(position * totalDelay, 1.0f, float(kDelay - 4));

    const float out0 = readDelay(v.delayA, v.writeA, delay0);
    const float out1 = readDelay(v.delayB, v.writeB, delay1);

    const float oneZero = 0.5f * (out0 + v.filter1);
    v.filter1 = out0;
    const float temp = -0.95f * oneZero;
    const float borePressure = temp - out1;
    const float pressureDiff = breath - borePressure;

    const float reedSlope = 0.10f + 0.42f * control1_;
    const float reedOffset = 0.56f + 0.28f * (1.0f - control1_);
    const float reedReflection = clampf(reedOffset + reedSlope * pressureDiff, -1.0f, 1.0f);

    writeDelay(v.delayB, v.writeB, temp);
    const float boreIn = breath - pressureDiff * reedReflection - temp;
    writeDelay(v.delayA, v.writeA, clampf(boreIn, -1.5f, 1.5f));

    return borePressure * 0.52f;
}

float InstrumentModels::processAccordion(Voice& v, double freq, float env) {
    // Reduced-order free-reed model. The reed oscillation itself is the main
    // radiating source; cavity pressure is retained as the acoustic load.
    // This avoids the previous steady-state cancellation where flow-bodyState
    // converged almost to zero and the instrument became effectively silent.
    const float pressure = (v.pressure > 0 ? v.pressure : channelPressure_) / 127.0f;
    const float vel = v.velocity / 127.0f;
    const float bellows = env * (0.36f + 0.82f * control2_ + 0.16f * pressure) *
                          (0.72f + 0.28f * vel);

    const float dt = 1.0f / float(sampleRate_);
    const float musette = 0.0010f + 0.010f * control3_;
    const float reedDamping = 0.012f + 0.025f * control1_;
    const float amplitudeLimit = 0.22f + 0.12f * (1.0f - control1_);
    const float drive = clampf(bellows - 0.12f * v.bodyState, 0.0f, 1.5f);
    const float turbulence = noise(v) * drive * 0.0015f;

    auto stepReed = [&](float& x, float& vv, double reedFreq) {
        const float omega = float(2.0 * kPi * reedFreq);
        const float threshold = 0.10f + 0.10f * control1_;
        const float mu = std::max(0.0f, drive - threshold) *
                         (0.16f + 0.20f * (1.0f - control1_));
        const float normalizedX = x / std::max(0.08f, amplitudeLimit);
        const float nonlinear = mu * (1.0f - normalizedX * normalizedX);

        const float acceleration =
                -omega * omega * x
                + 2.0f * omega * (nonlinear - reedDamping) * vv
                + omega * omega * (drive * 0.0035f + turbulence);

        vv += acceleration * dt;
        x += vv * dt;
        x = clampf(x, -0.50f, 0.50f);
        vv = clampf(vv, -1800.0f, 1800.0f);
    };

    stepReed(v.reedX1, v.reedV1, freq * (1.0 - musette));
    stepReed(v.reedX2, v.reedV2, freq * (1.0 + musette));

    const float aperture1 = clampf(0.10f + 0.18f * (1.0f - control1_) + 0.33f * v.reedX1,
                                   0.005f, 0.45f);
    const float aperture2 = clampf(0.10f + 0.18f * (1.0f - control1_) + 0.33f * v.reedX2,
                                   0.005f, 0.45f);
    const float flow = (aperture1 + aperture2) * std::sqrt(std::max(0.0f, drive));

    const float bodyRate = clampf(float(2.0 * kPi * std::min(freq * 0.28, 350.0) / sampleRate_),
                                  0.0015f, 0.10f);
    v.bodyState += (flow - v.bodyState) * bodyRate;

    const float reedVelocity =
            (v.reedV1 + v.reedV2) / std::max(1.0f, float(2.0 * kPi * freq));
    const float radiated =
            (v.reedX1 + v.reedX2) * 1.30f
            + reedVelocity * 0.16f
            + (flow - v.bodyState) * 0.20f;

    // Remove the blowing-pressure DC component before the shared body/FX bus.
    const float hp = radiated - v.dcX + 0.995f * v.dcY;
    v.dcX = radiated;
    v.dcY = hp;

    return clampf(hp * 0.20f, -1.0f, 1.0f);
}

float InstrumentModels::processFeltPiano(Voice& v, double) {
    const float t = float(v.age) / float(sampleRate_);
    const float softness = 1.0f - control1_;

    // Hammer/string contact time must shorten as string frequency rises.
    // A fixed ~4 ms contact spans multiple cycles above C5 and causes
    // destructive cancellation, which was the source of the thin/silent
    // upper register.
    const float baseContact = 0.0010f + 0.0035f * softness;
    const float registerScale = std::pow(
            261.625565f / std::max(65.0f, float(v.frequency)), 0.75f);
    const float contactDuration = clampf(
            baseContact * registerScale, 0.00035f, 0.0065f);
    float hammerForce = 0.0f;

    if (t < contactDuration) {
        const float q = clampf(t / contactDuration, 0.0f, 1.0f);
        const float compression = std::sin(float(kPi) * q);
        const float feltExponent = 1.9f + 2.1f * control1_;
        const float velocity = 0.35f + 0.65f * (v.velocity / 127.0f);
        hammerForce = std::pow(std::max(0.0f, compression), feltExponent)
                    * velocity * (0.68f + 0.62f * control2_);
    }

    float sum = 0.0f;
    for (int i=0; i<kModes; ++i) sum += tickMode(v, i, hammerForce);

    // Felt absorbs high-frequency contact noise; harder settings expose more.
    const float n = noise(v);
    const float contactNoise = t < contactDuration
            ? n * hammerForce * (0.006f + 0.024f * control1_)
            : 0.0f;

    return sum + contactNoise;
}

float InstrumentModels::processXylophone(Voice& v, double) {
    const float strikeSamples = 2.0f + 16.0f * (1.0f - control1_);
    float excitation = 0.0f;
    if (float(v.age) < strikeSamples) {
        const float q = (float(v.age) + 1.0f) / (strikeSamples + 1.0f);
        excitation = std::sin(float(kPi) * q)
                   * (0.45f + 0.75f * (v.velocity / 127.0f));
    }

    float sum = 0.0f;
    for (int i=0; i<kModes; ++i) sum += tickMode(v, i, excitation);
    return sum;
}

float InstrumentModels::processWoodBass(Voice& v, double freq, float env) {
    // Fractional-delay string length: unlike the previous fixed integer loop,
    // this follows pitch bend and vibrato in real time.
    const float delaySamples = clampf(
            float(sampleRate_ / std::max(24.0, freq) - 0.75),
            8.0f, float(kDelay - 4));
    const float stringOut = readDelay(v.delayA, v.writeA, delaySamples);

    // Approximate which physical string is being used (E1/A1/D2/G2) from the
    // played pitch.  Higher stopped positions lose energy slightly faster.
    int openNote = 28; // E1
    if (v.note >= 43) openNote = 43;      // G2
    else if (v.note >= 38) openNote = 38; // D2
    else if (v.note >= 33) openNote = 33; // A1
    const float stopped = clampf((v.note - openNote) / 12.0f, 0.0f, 2.0f);

    // A one-pole loop loss gives frequency-dependent damping: high partials
    // disappear faster than the fundamental. STRING DAMP closes this filter.
    const float tracking = 0.16f + 0.68f * (1.0f - control1_);
    v.filter1 += (stringOut - v.filter1) * tracking;

    // Per-round-trip energy loss dominates natural pizzicato decay.  The
    // player's damping control and stopped-string length both shorten sustain.
    const float loopGain = clampf(
            0.952f - 0.030f * control1_ - 0.0030f * stopped,
            0.900f, 0.957f);
    const float loop = v.filter1 * loopGain;
    writeDelay(v.delayA, v.writeA, loop);

    // Bridge velocity contains both string displacement and its brighter
    // difference component.  Driving the fixed body modes from this signal
    // makes the box participate instead of acting like a post-EQ.
    const float bridgeHigh = stringOut - v.filter1;
    const float bridgeDrive = 0.34f * stringOut + 0.66f * bridgeHigh;

    float body = 0.0f;
    for (int i=0; i<8; ++i) body += tickMode(v, i, bridgeDrive);

    // Large wooden bodies radiate the lowest string components more smoothly
    // than a direct pickup.  This low-pass state is mixed with some direct
    // bridge signal so the result keeps articulation without becoming boomy.
    v.bodyState += (stringOut - v.bodyState) * 0.032f;

    // Finger/fingerboard contact is short, velocity-sensitive and mostly
    // mid/high frequency.  It is deliberately subtle.
    const float t = float(v.age) / float(sampleRate_);
    const float n = noise(v);
    v.noiseState += (n - v.noiseState) * 0.055f;
    const float fingerHigh = n - v.noiseState;
    const float velocity = v.velocity / 127.0f;
    const float fingerEnv = std::exp(-t * (90.0f + 95.0f * control2_));
    float finger = fingerHigh * fingerEnv *
                   (0.004f + 0.020f * velocity * velocity);

    // A hard pluck can lightly touch the fingerboard, but only at the top of
    // the velocity range; this avoids a permanent synthetic click.
    if (velocity > 0.78f) {
        const float slapEnv = std::exp(-t * 58.0f);
        finger += bridgeHigh * slapEnv * (velocity - 0.78f) * 0.22f;
    }

    const float direct = 0.38f * stringOut + 0.32f * v.bodyState;

    // Body modes colour the bridge signal but must not become an independent
    // pitched oscillator.  Keep them well below the direct string so A0/T1
    // reinforce timbre without replacing the played fundamental.
    return (direct + 0.025f * body + finger) * env;
}

float InstrumentModels::processDrums(Voice& v, double) {
    const int n = v.note;
    const bool kick = (n == 60);
    const bool hat = (n == 61);
    const bool snare = (n == 62);
    const float velocity = v.velocity / 127.0f;
    const float t = float(v.age) / float(sampleRate_);

    const float strikeSamples = kick
            ? (7.0f + 22.0f * (1.0f - drumParams_[3]))
            : hat ? 4.0f
            : (5.0f + 16.0f * (1.0f - drumParams_[11]));

    float excitation = 0.0f;
    if (float(v.age) < strikeSamples) {
        const float q = (float(v.age) + 1.0f) / (strikeSamples + 1.0f);
        const float impact = kick ? (0.75f + 0.70f * drumParams_[3])
                                  : snare ? (0.72f + 0.72f * drumParams_[11])
                                          : (0.68f + 0.38f * drumParams_[7]);
        excitation = std::sin(float(kPi) * q)
                   * impact
                   * (0.35f + 0.65f * velocity);
    }

    float membrane = 0.0f;
    for (int i=0; i<kModes; ++i) membrane += tickMode(v, i, excitation);

    if (kick) {
        const float bend = drumParams_[2];
        const float click = drumParams_[3];
        const float base = 52.0f * std::pow(2.0f,
                ((drumParams_[0] - 0.5f) * 24.0f) / 12.0f);
        const float bendSemis = 28.0f * bend * std::exp(-t * 42.0f);
        const float f = base * std::pow(2.0f, bendSemis / 12.0f);
        v.phase = wrapPhase(v.phase + 2.0 * kPi * f / sampleRate_);
        const float punch = std::sin(v.phase) * std::exp(-t * 22.0f) * bend * 0.22f;

        const float nse = noise(v);
        const float hp = nse - v.noiseState;
        v.noiseState += (nse - v.noiseState) * 0.10f;
        const float clickEnv = std::exp(-t * (150.0f + 280.0f * click));
        return membrane + punch + hp * clickEnv * click * 0.20f;
    }

    if (snare) {
        const float snappy = drumParams_[10];
        const float impact = drumParams_[11];
        const float nse = noise(v);
        const float drive = std::min(1.0f, std::fabs(membrane) * (8.0f + 26.0f * snappy));
        const float noiseDecay = std::exp(-t * (5.0f + 18.0f * (1.0f - drumParams_[9])));
        v.noiseState += (nse * drive - v.noiseState) * (0.18f + 0.34f * snappy);

        const float transient = nse - v.filter2;
        v.filter2 += (nse - v.filter2) * 0.22f;
        const float impactEnv = std::exp(-t * (85.0f + 190.0f * impact));

        return membrane * (0.82f - 0.24f * snappy)
             + v.noiseState * noiseDecay * (0.10f + 0.52f * snappy)
             + transient * impactEnv * impact * 0.15f;
    }

    const float noiseLevel = drumParams_[7];
    const float nse = noise(v);
    const float hp = nse - v.noiseState;
    v.noiseState += (nse - v.noiseState) * (0.025f + 0.10f * drumParams_[6]);
    const float hatDecay = std::exp(-t * (7.0f + 24.0f * (1.0f - drumParams_[5])));
    return membrane * (0.72f - 0.22f * noiseLevel)
         + hp * hatDecay * noiseLevel * (0.18f + 0.28f * velocity);
}
float InstrumentModels::processVoice(Voice& v) {
    if (!v.active) return 0.0f;

    const float env = envelope(v);
    if (!v.active || env <= 0.0f) return 0.0f;

    const double bendSemis = (double(pitchBend_) - 8192.0) / 8192.0 * 2.0;
    const float pressure = (v.pressure > 0 ? v.pressure : channelPressure_) / 127.0f;
    const double vibratoCents = std::sin(v.vibratoPhase) *
                                (18.0 * vibrato_) * (0.35 + 0.65 * pressure);
    v.vibratoPhase = wrapPhase(v.vibratoPhase + 2.0 * kPi * 5.2 / sampleRate_);
    const double freq = v.frequency *
            std::pow(2.0, bendSemis / 12.0 + vibratoCents / 1200.0);

    float raw = 0.0f;
    switch (type_) {
        case FLUTE: raw = processFlute(v, freq, env); break;
        case SAX: raw = processSax(v, freq, env); break;
        case FELT_PIANO: raw = processFeltPiano(v, freq) * env; break;
        case ACCORDION: raw = processAccordion(v, freq, env); break;
        case XYLOPHONE: raw = processXylophone(v, freq) * env; break;
        case WOOD_BASS: raw = processWoodBass(v, freq, env); break;
        case DRUMS: raw = processDrums(v, freq) * env; break;
        default: break;
    }

    v.age++;

    // Natural one-shot retirement keeps silent percussive voices from
    // occupying the allocator even if a controller omits NoteOff.
    if ((type_ == DRUMS && v.age > uint64_t(sampleRate_ * 4.0)) ||
        (type_ == XYLOPHONE && v.age > uint64_t(sampleRate_ * 5.0)) ||
        (type_ == FELT_PIANO && v.age > uint64_t(sampleRate_ * 10.0))) {
        v.active = false;
    }

    const float velocityGain = 0.30f + 0.70f * (v.velocity / 127.0f);
    return std::isfinite(raw) ? raw * velocityGain : 0.0f;
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
    return std::tanh(sum * 0.95f);
}

int InstrumentModels::activeVoices() const {
    int n = 0;
    for (const auto& v : voices_) if (v.active) ++n;
    return n;
}
