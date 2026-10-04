#pragma once

#include <array>
#include <cstdint>

class InstrumentModels {
public:
    enum Type : int {
        FLUTE = 1,
        SAX = 2,
        FELT_PIANO = 3,
        ACCORDION = 4,
        XYLOPHONE = 5,
        WOOD_BASS = 6,
        DRUMS = 7
    };

    void prepare(double sampleRate);
    void reset();
    void setType(int type);
    int type() const { return type_; }

    void noteOn(int note, int velocity);
    void noteOff(int note, bool sustainDown);
    void sustainChanged(bool down);
    void allNotesOff();
    void polyPressure(int note, int value);
    void channelPressure(int value);
    void pitchBend(int value14);

    void setControl(int cc, float normalized);
    void setAdsr(float attackMs, float decayMs, float sustain, float releaseMs);

    float process();
    int activeVoices() const;

private:
    static constexpr int kVoices = 8;
    static constexpr int kModes = 8;
    static constexpr int kDelay = 2048;

    struct Voice {
        bool active = false;
        bool keyDown = false;
        bool pendingRelease = false;
        int note = -1;
        int velocity = 0;
        int pressure = 0;
        uint64_t age = 0;

        float env = 0.0f;
        uint8_t envStage = 0; // 0 off, 1 attack, 2 decay, 3 sustain, 4 release

        double phase = 0.0;
        double phase2 = 0.0;
        double phase3 = 0.0;
        double frequency = 440.0;
        double targetFrequency = 440.0;

        std::array<double, kModes> modePhase{};
        std::array<float, kModes> modeAmp{};

        std::array<float, kDelay> delay{};
        int delayWrite = 0;
        int delayLength = 128;
        float delayFilter = 0.0f;

        float noiseState = 0.0f;
        float aux1 = 0.0f;
        float aux2 = 0.0f;
        uint32_t rng = 0x12345678u;
    };

    double sampleRate_ = 48000.0;
    int type_ = FLUTE;
    int pitchBend_ = 8192;
    int channelPressure_ = 0;
    bool sustainDown_ = false;

    float control1_ = 0.58f; // CC10: pressure / hardness
    float control2_ = 0.58f; // CC11: breath / force
    float control3_ = 0.33f; // CC74: color / position
    float vibrato_ = 0.10f;  // CC1

    float attackMs_ = 12.0f;
    float decayMs_ = 110.0f;
    float sustain_ = 0.88f;
    float releaseMs_ = 260.0f;

    std::array<Voice, kVoices> voices_{};

    static double midiToHz(double note);
    int allocateVoice(int note) const;
    float envelope(Voice& v);
    float noise(Voice& v);
    float processVoice(Voice& v);

    float processFlute(Voice& v, double freq);
    float processSax(Voice& v, double freq);
    float processFeltPiano(Voice& v, double freq);
    float processAccordion(Voice& v, double freq);
    float processXylophone(Voice& v, double freq);
    float processWoodBass(Voice& v, double freq);
    float processDrums(Voice& v, double freq);

    void initialiseModes(Voice& v);
    void initialiseBassDelay(Voice& v);
};
