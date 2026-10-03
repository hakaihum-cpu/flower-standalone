#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class IntegratedRecorder {
public:
    static constexpr int kSlots = 4;
    static constexpr int kPeakBins = 64;
    static constexpr float kSegmentSeconds = 5.0f;

    void prepare(int sampleRate);
    void process(float inputL, float inputR, float& outputL, float& outputR);

    void toggleRecording() { recordingEnabled_.store(!recordingEnabled_.load()); }
    bool isRecording() const { return recordingEnabled_.load(); }

    void toggleRandom() { randomEnabled_.store(!randomEnabled_.load()); }
    bool isRandom() const { return randomEnabled_.load(); }

    void requestClear() { clearRequested_.store(true); }
    void requestPlaySlot(int slot);
    void requestRecordSlot(int slot);

    void toggleMidiClockMode() { midiClockMode_.store(!midiClockMode_.load()); }
    bool isMidiClockMode() const { return midiClockMode_.load(); }
    void setInternalBpm(int bpm);
    int internalBpm() const { return internalBpm_.load(); }
    void handleMidiRealtime(int status);

    int recordingSlot() const { return uiRecordingSlot_.load(); }
    bool isSlotPlaying(int slot) const;
    float slotProgress(int slot) const;
    int validSamples(int slot) const;
    float peak(int slot, int bin) const;
    float inputLevel() const { return inputLevel_.load(); }

private:
    int sampleRate_ = 48000;
    int segmentSamples_ = 240000;

    std::array<std::vector<float>, kSlots> slotL_;
    std::array<std::vector<float>, kSlots> slotR_;
    std::array<std::atomic<int>, kSlots> validSamples_{};
    std::array<std::array<std::atomic<float>, kPeakBins>, kSlots> peaks_{};

    std::atomic<bool> recordingEnabled_{false};
    std::atomic<bool> randomEnabled_{false};
    std::atomic<bool> clearRequested_{false};
    std::atomic<uint32_t> requestedPlayMask_{0};
    std::atomic<int> requestedRecordSlot_{-1};

    int writeSlot_ = -1;
    int writePosition_ = 0;
    bool recordingWasEnabled_ = false;

    std::array<int, kSlots> playPositions_{};
    std::array<std::atomic<bool>, kSlots> uiPlaying_{};
    std::array<std::atomic<float>, kSlots> uiPlaybackProgress_{};
    std::atomic<int> uiRecordingSlot_{-1};

    int randomPlaySlot_ = -1;
    int randomPlayPosition_ = 0;
    int64_t randomSamplesRemaining_ = 0;
    bool randomWasEnabled_ = false;
    std::atomic<int> uiRandomSlot_{-1};
    std::atomic<float> uiRandomProgress_{0.0f};
    uint32_t randomState_ = 0x45505234u;

    std::atomic<bool> midiClockMode_{false};
    std::atomic<int> internalBpm_{120};
    std::atomic<float> inputLevel_{0.0f};
    int midiClockTicks_ = 0;
    bool midiClockRunning_ = true;

    void beginRecordingSegment();
    void beginRecordingAtSlot(int slot);
    void finishRecordingSegment();
    void clearAll();
    void beginPlayback(int slot);
    int chooseRandomValidSlot();
    void beginRandomPlayback(int slot);
    void stopRandomPlayback();
    void scheduleNextRandomSwitch();
    void updateRandomState();
    void addManualPlayback(float& l, float& r);
    void addRandomPlayback(float& l, float& r);
};
