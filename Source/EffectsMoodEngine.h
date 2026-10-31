#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// MOOD MKII-inspired EFFECTS mode.
// CC numbers and MIDI-note behaviour follow the published Chase Bliss MIDI map.
// This is not a circuit-accurate or algorithm-identical emulation.
class EffectsMoodEngine final
{
public:
    void prepare (double sampleRate)
    {
        rate = juce::jmax (8000.0, sampleRate);
        const int max = (int) std::ceil (rate * 8.0);
        const int delay = (int) std::ceil (rate * 2.0);
        for (auto& c : history) c.assign ((size_t) max, 0.0f);
        for (auto& c : captured) c.assign ((size_t) max, 0.0f);
        for (auto& c : spatial) c.assign ((size_t) delay, 0.0f);
        historyPos = spatialPos = 0;
        capturedLength = 0;
        capturedStart = 0;
        capturedReady.store (false, std::memory_order_relaxed);
        captureFlag.store (false, std::memory_order_relaxed);
        playHead = 0.0; playSpeed = 1.0; rampPhase = 0.0; env = 0.0f;
        lastNote = -1;
        envStage = 0;
        midiNote = -1;
        uiNote.store (-1, std::memory_order_relaxed);
        noteVelocity = 1.0f;
        filtered = { 0.0f, 0.0f };
        for (auto& c : cc) c.store (64, std::memory_order_relaxed);
        setCc (14, 64); // TIME
        setCc (15, 90); // MIX
        setCc (16, 64); // LENGTH
        setCc (17, 50); // WET MODIFY
        setCc (18, 100); // CLOCK
        setCc (19, 92); // MICRO MODIFY: 1x forward, TAPE mode
        setCc (20, 0);  // RAMP disabled by default
        setCc (84, 0);  // no portamento by default
        setCc (21, 1);  // WET CHANNEL: REVERB
        setCc (22, 2);  // ROUTING: LOOP+IN
        setCc (23, 2);  // MICRO LOOPER: TAPE
        setCc (58, 2);  // SYNTH: ADSR
        setCc (80, 8); setCc (81, 32); setCc (82, 108); setCc (83, 48);
        setCc (102, 127); setCc (103, 127);
        setCc (105, 0); setCc (106, 0); // no freeze / overdub by default
        axisX.store (14, std::memory_order_relaxed);
        axisY.store (19, std::memory_order_relaxed);
        x.store (0.5f, std::memory_order_relaxed);
        y.store (0.78f, std::memory_order_relaxed);
    }
    void reset() noexcept
    {
        captureFlag.store (false, std::memory_order_relaxed);
        capturedReady.store (false, std::memory_order_relaxed);
        capturedLength = 0;
        capturedStart = 0;
        midiNote = lastNote = -1;
        envStage = 0;
        uiNote.store (-1, std::memory_order_relaxed);
        env = 0.0f;
        playHead = 0.0;
        playSpeed = 1.0;
        rampPhase = 0.0;
    }
    void requestCapture() noexcept { captureFlag.store (true, std::memory_order_release); }
    bool hasCapture() const noexcept { return capturedReady.load (std::memory_order_acquire); }
    void setUiNote (int note, bool down) noexcept
    {
        uiNote.store (down ? juce::jlimit (0, 127, note) : -1,
                      std::memory_order_release);
    }
    int activeNote() const noexcept { return playingNote.load (std::memory_order_relaxed); }
    void setAxis (bool xAxis, int number) noexcept
    {
        number = juce::jlimit (0, 127, number);
        if (xAxis) axisX.store (number, std::memory_order_release);
        else axisY.store (number, std::memory_order_release);
    }
    int getAxis (bool xAxis) const noexcept
    {
        return (xAxis ? axisX : axisY).load (std::memory_order_relaxed);
    }
    void setXY (float xx, float yy) noexcept
    {
        xx = juce::jlimit (0.0f, 1.0f, xx);
        yy = juce::jlimit (0.0f, 1.0f, yy);
        x.store (xx, std::memory_order_relaxed);
        y.store (yy, std::memory_order_relaxed);
        setCc (getAxis (true), juce::roundToInt (xx * 127.0f));
        setCc (getAxis (false), juce::roundToInt (yy * 127.0f));
    }
    float getXY (bool xAxis) const noexcept
    {
        return (xAxis ? x : y).load (std::memory_order_relaxed);
    }
    void setCc (int number, int value) noexcept
    {
        if (juce::isPositiveAndBelow (number, 128))
            cc[(size_t) number].store (juce::jlimit (0, 127, value),
                                       std::memory_order_release);
    }
    int getCc (int number) const noexcept
    {
        return juce::isPositiveAndBelow (number, 128)
            ? cc[(size_t) number].load (std::memory_order_relaxed) : 0;
    }
    void handleMidi (const juce::MidiMessage& m, int channel) noexcept
    {
        if (! m.isForChannel (channel))
            return;
        if (m.isController())
        {
            setCc (m.getControllerNumber(), m.getControllerValue());
            if (m.getControllerNumber() == getAxis (true))
                x.store ((float) m.getControllerValue() / 127.0f, std::memory_order_relaxed);
            if (m.getControllerNumber() == getAxis (false))
                y.store ((float) m.getControllerValue() / 127.0f, std::memory_order_relaxed);
            // MOOD MKII uses CC102 for looper bypass; CAPTURE is a dedicated
            // application action, not a fake MIDI CC.
        }
        else if (m.isNoteOn())
        {
            midiNote = m.getNoteNumber();
            noteVelocity = m.getFloatVelocity();
        }
        else if (m.isNoteOff() && m.getNoteNumber() == midiNote)
            midiNote = -1;
    }
    void process (juce::AudioBuffer<float>& audio) noexcept
    {
        const int n = audio.getNumSamples();
        if (n <= 0 || audio.getNumChannels() <= 0 || history[0].empty()) return;
        const int histSize = (int) history[0].size();
        const int delaySize = (int) spatial[0].size();
        const float time = (float) getCc (14) / 127.0f;
        const float length = (float) getCc (16) / 127.0f;
        const float mix = (float) getCc (15) / 127.0f;
        const float modify = (float) getCc (19) / 127.0f;
        const float wetModify = (float) getCc (17) / 127.0f;
        const float rampDepth = (float) getCc (20) / 127.0f;
        const float clock = (float) getCc (18) / 127.0f;
        const int wetMode = getCc (21) <= 1 ? 0 : (getCc (21) == 2 ? 1 : 2);
        const int loopMode = getCc (23) <= 1 ? 0 : (getCc (23) == 2 ? 1 : 2);
        const int routing = getCc (22) <= 1 ? 0 : (getCc (22) == 2 ? 1 : 2);
        const bool loopOn = getCc (102) > 0;
        const bool wetOn = getCc (103) > 0;
        const int requested = uiNote.load (std::memory_order_acquire);
        const int note = requested >= 0 ? requested : midiNote;
        playingNote.store (note, std::memory_order_relaxed);
        const bool noteGate = note >= 0;
        const float attack = 0.003f + 1.5f * std::pow ((float) getCc (80) / 127.0f, 2.0f);
        const float decay = 0.005f + 2.5f * std::pow ((float) getCc (81) / 127.0f, 2.0f);
        const float sustain = (float) getCc (82) / 127.0f;
        const float release = 0.008f + 3.5f * std::pow ((float) getCc (83) / 127.0f, 2.0f);
        const int outputType = getCc (58);
        const bool hasLoop = capturedReady.load (std::memory_order_acquire);
        const int loopSamples = juce::jlimit (128, histSize,
            (int) (rate * (0.08 + 3.0 * time * time) * (0.25 + 0.75 * length)));
        if (captureFlag.exchange (false, std::memory_order_acq_rel))
        {
            capturedLength = loopSamples;
            // O(1) capture: swap two *preallocated* 8-second stereo rings.
            // Never copy megabytes of audio in one realtime callback.
            capturedStart = (historyPos + histSize - capturedLength) % histSize;
            history.swap (captured);
            historyPos = 0;
            playHead = 0.0;
            capturedReady.store (true, std::memory_order_release);
        }
        const bool active = hasLoop || capturedReady.load (std::memory_order_relaxed);
        if (note != lastNote)
        {
            playHead = 0.0;
            envStage = note >= 0 ? 1 : 4;
            lastNote = note;
        }
        const double noteRate = noteGate ? std::pow (2.0, ((double) note - 60.0) / 12.0) : 1.0;
        // TAPE/STRETCH are approximate. CLK ratio changes both channels as on hardware.
        const double clockRate = std::pow (2.0, (double) (clock - 0.8f) * 2.0);
        // Preserve the pedal's documented *stepped* TAPE/STRETCH speed
        // ranges, including Reverse and Stall. ENV micro-loop uses 1x.
        double microRate = 1.0;
        const int speedCc = getCc (19);
        if (loopMode == 1)
        {
            if (speedCc <= 4) microRate = -4.0;
            else if (speedCc <= 23) microRate = -2.0;
            else if (speedCc <= 42) microRate = -1.0;
            else if (speedCc <= 61) microRate = -0.5;
            else if (speedCc <= 84) microRate = 0.5;
            else if (speedCc <= 97) microRate = 1.0;
            else if (speedCc <= 119) microRate = 2.0;
            else microRate = 4.0;
        }
        else if (loopMode == 2)
        {
            if (speedCc <= 15) microRate = -1.0;
            else if (speedCc <= 30) microRate = -1.5;
            else if (speedCc <= 46) microRate = -2.0;
            else if (speedCc <= 60) microRate = -4.0;
            else if (speedCc <= 80) microRate = 0.0;
            else if (speedCc <= 96) microRate = 4.0;
            else if (speedCc <= 111) microRate = 2.0;
            else if (speedCc <= 126) microRate = 1.5;
        }
        const double targetSpeed = noteRate * clockRate * microRate;
        const double portamento = (double) getCc (84) / 127.0;
        const double portSeconds = 0.005 + 0.8 * portamento * portamento;
        const double glide = portamento < 0.0001
            ? 1.0
            : 1.0 - std::exp (-1.0 / (rate * portSeconds));
        const int delayTap = juce::jlimit (64, delaySize - 1, (int) ((0.065f + time * 0.68f) * rate));
        for (int i = 0; i < n; ++i)
        {
            float source[2] {};
            for (int ch = 0; ch < 2; ++ch)
            {
                const float in = audio.getSample (juce::jmin (ch, audio.getNumChannels() - 1), i);
                history[(size_t) ch][(size_t) historyPos] = in; // always listening
                source[ch] = in;
            }
            historyPos = (historyPos + 1) % histSize;
            playSpeed += (targetSpeed - playSpeed) * glide;
            // A simple bipolar ramp modulation for wet feedback.
            // This is an approximation, not the pedal's ramp waveform engine.
            rampPhase += (0.05 + 2.2 * clock) / rate;
            if (rampPhase >= 1.0) rampPhase -= 1.0;
            const float rampWave = 1.0f - 4.0f * std::abs ((float) rampPhase - 0.5f);
            if (active && capturedLength >= 128 && loopOn)
            {
                const int a = ((int) playHead) % capturedLength;
                const int b = (a + 1) % capturedLength;
                const int readA = (capturedStart + a) % histSize;
                const int readB = (capturedStart + b) % histSize;
                const float frac = (float) (playHead - std::floor (playHead));
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float c0 = captured[(size_t) ch][(size_t) readA];
                    const float c1 = captured[(size_t) ch][(size_t) readB];
                    const float loop = c0 + (c1 - c0) * frac;
                    const float glide = (float) (juce::jmin (a, capturedLength - a) + 1) / (float) juce::jmax (1, capturedLength / 24);
                    const float edge = juce::jlimit (0.0f, 1.0f, glide);
                    const float voiced = loop * edge;
                    if (ch == 0)
                    {
                        const float a = 1.0f - std::exp (-1.0f / ((float) rate * attack));
                        const float d = 1.0f - std::exp (-1.0f / ((float) rate * decay));
                        const float r = 1.0f - std::exp (-1.0f / ((float) rate * release));
                        if (envStage == 1)
                        {
                            env += (1.0f - env) * a;
                            if (env > 0.995f) envStage = 2;
                        }
                        else if (envStage == 2)
                        {
                            env += (sustain - env) * d;
                            if (std::abs (env - sustain) < 0.003f) envStage = 3;
                        }
                        else if (envStage == 3) env = sustain;
                        else if (envStage == 4)
                        {
                            env += (0.0f - env) * r;
                            if (env < 0.0001f) { env = 0.0f; envStage = 0; }
                        }
                    }
                    const float noteGain = noteGate ? noteVelocity : 1.0f;
                    const float gate = (outputType == 0 ? 1.0f
                                      : outputType == 1 ? (noteGate ? 1.0f : 0.0f)
                                      : env) * noteGain;
                    // Routing follows hardware options IN / LOOP+IN / LOOP.
                    if (getCc (106) > 0)
                        captured[(size_t) ch][(size_t) readA] =
                            0.7f * captured[(size_t) ch][(size_t) readA]
                            + 0.3f * source[ch]; // overdub, no allocation
                    source[ch] = (routing == 0 ? source[ch] :
                                  routing == 1 ? 0.35f * source[ch] + 0.65f * voiced * gate
                                               : voiced * gate);
                }
                playHead += playSpeed;
                while (playHead >= capturedLength) playHead -= capturedLength;
                while (playHead < 0.0) playHead += capturedLength;
            }
            if (wetOn)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    const int tap = (spatialPos + delaySize - delayTap - ch * 23 + delaySize) % delaySize;
                    const float delayed = spatial[(size_t) ch][(size_t) tap];
                    // REVERB smears feedback; DELAY preserves transients; SLIP offsets read phase.
                    const float modulatedWet = juce::jlimit (0.0f, 1.0f,
                        wetModify + rampDepth * rampWave * 0.30f);
                    const float feedback = wetMode == 0 ? (0.55f + 0.30f * modulatedWet)
                                                        : (0.20f + 0.65f * modulatedWet);
                    const float smoothed = wetMode == 1 ? delayed : 0.65f * filtered[(size_t) ch] + 0.35f * delayed;
                    filtered[(size_t) ch] = smoothed;
                    const bool frozen = getCc (105) > 0;
                    spatial[(size_t) ch][(size_t) spatialPos] =
                        juce::jlimit (-0.9f, 0.9f,
                                     (frozen ? 0.0f : source[ch])
                                     + (frozen ? 0.996f : feedback) * smoothed);
                    source[ch] = source[ch] * (1.0f - mix) + smoothed * mix;
                }
            }
            spatialPos = (spatialPos + 1) % delaySize;
            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                audio.setSample (ch, i, std::tanh (source[juce::jmin (ch, 1)]));
        }
    }
private:
    double rate = 48000.0;
    std::array<std::vector<float>, 2> history, captured, spatial;
    std::array<float, 2> filtered {};
    int historyPos = 0, spatialPos = 0, capturedLength = 0, capturedStart = 0;
    double playHead = 0.0, playSpeed = 1.0, rampPhase = 0.0;
    float env = 0.0f, noteVelocity = 1.0f;
    int envStage = 0; // 0 off, 1 attack, 2 decay, 3 sustain, 4 release
    int midiNote = -1, lastNote = -1;
    std::atomic<int> uiNote { -1 }, playingNote { -1 };
    std::array<std::atomic<int>, 128> cc {};
    std::atomic<bool> captureFlag { false }, capturedReady { false };
    std::atomic<int> axisX { 14 }, axisY { 19 };
    std::atomic<float> x { 0.5f }, y { 0.78f };
};
