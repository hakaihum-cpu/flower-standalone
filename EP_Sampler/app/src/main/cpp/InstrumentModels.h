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
    static constexpr int kModes = 12;
    static constexpr int kDelay = 4096;

    struct Voice {
        bool active = false;
        bool keyDown = false;
        bool pendingRelease = false;
        int note = -1;
        int velocity = 0;
        int pressure = 0;
        uint64_t age = 0;

        float env = 0.0f;
        uint8_t envStage = 0;

        double frequency = 440.0;
        double phase = 0.0;
        double phase2 = 0.0;
        double vibratoPhase = 0.0;

        std::array<float, kDelay> delayA{};
        std::array<float, kDelay> delayB{};
        int writeA = 0;
        int writeB = 0;
        float filter1 = 0.0f;
        float filter2 = 0.0f;
        float dcX = 0.0f;
        float dcY = 0.0f;

        // Free-reed states (two reeds allow musette/accordion behavior).
        float reedX1 = 0.0f;
        float reedV1 = 0.0f;
        float reedX2 = 0.0f;
        float reedV2 = 0.0f;

        // Stable second-order modal resonators.
        std::array<float, kModes> modeY1{};
        std::array<float, kModes> modeY2{};
        std::array<float, kModes> modeA1{};
        std::array<float, kModes> modeA2{};
        std::array<float, kModes> modeGain{};

        float noiseState = 0.0f;
        float bodyState = 0.0f;
        uint32_t rng = 0x12345678u;
    };

    double sampleRate_ = 48000.0;
    int type_ = FLUTE;
    int pitchBend_ = 8192;
    int channelPressure_ = 0;
    bool sustainDown_ = false;

    float control1_ = 0.58f;
    float control2_ = 0.58f;
    float control3_ = 0.33f;
    float vibrato_ = 0.10f;

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

    static float readDelay(const std::array<float,kDelay>& buffer, int writeIndex, float delaySamples);
    static void writeDelay(std::array<float,kDelay>& buffer, int& writeIndex, float value);
    void setupMode(Voice& v, int index, double frequency, float decaySeconds, float gain);
    float tickMode(Voice& v, int index, float excitation);

    float processFlute(Voice& v, double freq, float env);
    float processSax(Voice& v, double freq, float env);
    float processFeltPiano(Voice& v, double freq);
    float processAccordion(Voice& v, double freq, float env);
    float processXylophone(Voice& v, double freq);
    float processWoodBass(Voice& v, double freq, float env);
    float processDrums(Voice& v, double freq);

    void initialisePiano(Voice& v);
    void initialiseXylophone(Voice& v);
    void initialiseWoodBass(Voice& v);
    void initialiseDrums(Voice& v);
};
