#pragma once
#include <vector>
#include <array>
#include <cstddef>
#include <cstdint>

// Global texture/micro-loop processor.
// MODE 0 preserves the existing Dreamy behavior.
// Additional modes are inspired by documented behaviors of Microcosm,
// Chroma Console and MOOD MKII; they are not circuit-identical clones.
class DreamyEffect {
public:
    enum Mode : int {
        DREAMY = 0,
        MICROCOSM_MOSAIC = 1,
        MICROCOSM_GLIDE = 2,
        HAZE = 3,
        CHROMA_COLLAGE = 4,
        CHROMA_SPACE = 5,
        MOOD_REVERB = 6,
        MOOD_DELAY = 7,
        MOOD_SLIP = 8,
        MOOD_TAPE = 9,
        MOOD_STRETCH = 10,
        MODE_COUNT = 11
    };

    void prepare(int sampleRate);
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }

    void setMode(int mode);
    int mode() const { return targetMode_; }

    // Backward-compatible mapping:
    // X -> P1, Y -> P2, MIX stays MIX.
    void setXY(float x, float y);
    void setParameters(float x, float y, float mix);

    // Extra per-mode controls P3 / P4.
    void setExtraParameters(float p3, float p4);

    void process(float& left, float& right);

private:
    struct Grain {
        double read = 0.0;
        double phase = 0.0;
        bool initialized = false;
    };
    struct Voice {
        std::array<Grain,2> grains{};
    };

    bool enabled_ = true;
    int sampleRate_ = 48000;

    std::vector<float> histL_, histR_;
    size_t write_ = 0;
    uint64_t historyFrames_ = 0;

    // MODE 0 legacy Dreamy state.
    std::array<Voice,2> voices_{};

    // Additional modes use independent stereo grains so one channel never
    // advances the shared read head for the other channel.
    std::array<Grain,12> grains_{};

    // Feedback/diffusion ring shared by Chroma/MOOD-style modes.
    std::vector<float> fxL_, fxR_;
    size_t fxWrite_ = 0;
    float diffuseLpL_ = 0.f, diffuseLpR_ = 0.f;
    float toneLpL_ = 0.f, toneLpR_ = 0.f;

    float targetP1_ = 0.28f, targetP2_ = 0.28f;
    float targetP3_ = 0.50f, targetP4_ = 0.50f;
    float targetMix_ = 0.34f;

    float p1_ = 0.28f, p2_ = 0.28f;
    float p3_ = 0.50f, p4_ = 0.50f;
    float mix_ = 0.34f;

    int currentMode_ = DREAMY;
    int targetMode_ = DREAMY;
    bool switchingMode_ = false;
    bool fadingOutForMode_ = false;
    float modeFade_ = 1.0f;

    float wetLpL_ = 0.f, wetLpR_ = 0.f;
    uint32_t rng_ = 0x41C64E6Du;

    double wrap(double p, size_t size) const;
    double wrapHistory(double p) const;
    float readInterp(const std::vector<float>& b, double p) const;
    float readFx(const std::vector<float>& b, double delayFrames) const;

    // Legacy mono grain sampler used only by original Dreamy mode.
    float grainSample(Grain& g, const std::vector<float>& b, double speed,
                      double loopFrames, double resetLagFrames,
                      int voiceIndex, int grainIndex);

    // Safe stereo grain renderer for new modes.
    void grainStereo(Grain& g,
                     double speed,
                     double loopFrames,
                     double resetLagFrames,
                     double jitterFrames,
                     float& outL,
                     float& outR);

    float randomSigned();
    void resetNewModeState();

    void renderDreamy(float& wetL, float& wetR);
    void renderMicrocosmMosaic(float& wetL, float& wetR);
    void renderMicrocosmGlide(float& wetL, float& wetR);
    void renderHaze(float& wetL, float& wetR);
    void renderChromaCollage(float dryL, float dryR, float& wetL, float& wetR);
    void renderChromaSpace(float dryL, float dryR, float& wetL, float& wetR);
    void renderMoodReverb(float dryL, float dryR, float& wetL, float& wetR);
    void renderMoodDelay(float dryL, float dryR, float& wetL, float& wetR);
    void renderMoodSlip(float& wetL, float& wetR);
    void renderMoodTape(float dryL, float dryR, float& wetL, float& wetR);
    void renderMoodStretch(float& wetL, float& wetR);

    void simpleDiffusion(float dryL, float dryR,
                         float delayMs, float feedback, float damping,
                         float& wetL, float& wetR);
};
