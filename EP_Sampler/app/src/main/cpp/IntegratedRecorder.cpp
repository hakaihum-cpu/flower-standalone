#include "IntegratedRecorder.h"
#include <algorithm>
#include <cmath>

void IntegratedRecorder::prepare(int sampleRate) {
    sampleRate_ = sampleRate > 1000 ? sampleRate : 48000;
    segmentSamples_ = std::max(1, int(std::lround(sampleRate_ * kSegmentSeconds)));
    for (int s = 0; s < kSlots; ++s) {
        slotL_[s].assign(segmentSamples_, 0.f);
        slotR_[s].assign(segmentSamples_, 0.f);
        validSamples_[s].store(0);
        playPositions_[s] = 0;
        uiPlaying_[s].store(false);
        uiPlaybackProgress_[s].store(0.f);
        for (auto& p : peaks_[s]) p.store(0.f);
    }
    recordingEnabled_.store(false);
    randomEnabled_.store(false);
    clearRequested_.store(false);
    requestedPlayMask_.store(0);
    requestedRecordSlot_.store(-1);
    writeSlot_ = -1;
    writePosition_ = 0;
    recordingWasEnabled_ = false;
    uiRecordingSlot_.store(-1);
    stopRandomPlayback();
    randomWasEnabled_ = false;
    inputLevel_.store(0.f);
    midiClockTicks_ = 0;
    midiClockRunning_ = true;
}

void IntegratedRecorder::requestPlaySlot(int slot) {
    if (slot >= 0 && slot < kSlots)
        requestedPlayMask_.fetch_or(1u << uint32_t(slot));
}

void IntegratedRecorder::requestRecordSlot(int slot) {
    if (slot >= 0 && slot < kSlots)
        requestedRecordSlot_.store(slot);
}

void IntegratedRecorder::setInternalBpm(int bpm) {
    internalBpm_.store(std::clamp(bpm, 30, 300));
}

void IntegratedRecorder::handleMidiRealtime(int status) {
    if (status == 0xFA || status == 0xFB) {
        midiClockRunning_ = true;
        midiClockTicks_ = 0;
    } else if (status == 0xFC) {
        midiClockRunning_ = false;
        midiClockTicks_ = 0;
    } else if (status == 0xF8 && midiClockMode_.load() && midiClockRunning_) {
        if (++midiClockTicks_ >= 24) midiClockTicks_ = 0;
    }
}

bool IntegratedRecorder::isSlotPlaying(int slot) const {
    if (slot < 0 || slot >= kSlots) return false;
    return uiPlaying_[slot].load() || uiRandomSlot_.load() == slot;
}

float IntegratedRecorder::slotProgress(int slot) const {
    if (slot < 0 || slot >= kSlots) return 0.f;
    if (uiRandomSlot_.load() == slot) return uiRandomProgress_.load();
    return uiPlaybackProgress_[slot].load();
}

int IntegratedRecorder::validSamples(int slot) const {
    return (slot >= 0 && slot < kSlots) ? validSamples_[slot].load() : 0;
}

float IntegratedRecorder::peak(int slot, int bin) const {
    if (slot < 0 || slot >= kSlots || bin < 0 || bin >= kPeakBins) return 0.f;
    return peaks_[slot][bin].load();
}

void IntegratedRecorder::beginRecordingSegment() {
    beginRecordingAtSlot((writeSlot_ + 1) % kSlots);
}

void IntegratedRecorder::beginRecordingAtSlot(int slot) {
    if (slot < 0 || slot >= kSlots) return;
    writeSlot_ = slot;
    writePosition_ = 0;
    std::fill(slotL_[slot].begin(), slotL_[slot].end(), 0.f);
    std::fill(slotR_[slot].begin(), slotR_[slot].end(), 0.f);
    validSamples_[slot].store(0);
    playPositions_[slot] = 0;
    uiPlaying_[slot].store(false);
    uiPlaybackProgress_[slot].store(0.f);
    for (auto& p : peaks_[slot]) p.store(0.f);
    uiRecordingSlot_.store(slot);
}

void IntegratedRecorder::finishRecordingSegment() {
    if (writeSlot_ >= 0)
        validSamples_[writeSlot_].store(std::clamp(writePosition_, 0, segmentSamples_));
    uiRecordingSlot_.store(-1);
}

void IntegratedRecorder::clearAll() {
    recordingEnabled_.store(false);
    recordingWasEnabled_ = false;
    requestedPlayMask_.store(0);
    requestedRecordSlot_.store(-1);
    writeSlot_ = -1;
    writePosition_ = 0;
    uiRecordingSlot_.store(-1);
    for (int s = 0; s < kSlots; ++s) {
        std::fill(slotL_[s].begin(), slotL_[s].end(), 0.f);
        std::fill(slotR_[s].begin(), slotR_[s].end(), 0.f);
        validSamples_[s].store(0);
        playPositions_[s] = 0;
        uiPlaying_[s].store(false);
        uiPlaybackProgress_[s].store(0.f);
        for (auto& p : peaks_[s]) p.store(0.f);
    }
    stopRandomPlayback();
}

void IntegratedRecorder::beginPlayback(int slot) {
    if (slot < 0 || slot >= kSlots || slot == uiRecordingSlot_.load()) return;
    if (validSamples_[slot].load() <= 0) return;
    playPositions_[slot] = 0;
    uiPlaying_[slot].store(true);
    uiPlaybackProgress_[slot].store(0.f);
}

int IntegratedRecorder::chooseRandomValidSlot() {
    int candidates[kSlots]{};
    int count = 0;
    const int rec = uiRecordingSlot_.load();
    for (int i = 0; i < kSlots; ++i) {
        if (i == rec || i == randomPlaySlot_ || validSamples_[i].load() <= 0) continue;
        if (!uiPlaying_[i].load()) candidates[count++] = i;
    }
    if (count == 0) {
        for (int i = 0; i < kSlots; ++i)
            if (i != rec && i != randomPlaySlot_ && validSamples_[i].load() > 0)
                candidates[count++] = i;
    }
    if (count == 0 && randomPlaySlot_ >= 0 && validSamples_[randomPlaySlot_].load() > 0)
        return randomPlaySlot_;
    if (count == 0) return -1;
    randomState_ = randomState_ * 1664525u + 1013904223u;
    return candidates[randomState_ % uint32_t(count)];
}

void IntegratedRecorder::beginRandomPlayback(int slot) {
    if (slot < 0 || slot >= kSlots || slot == uiRecordingSlot_.load()) return;
    if (validSamples_[slot].load() <= 0) return;
    randomPlaySlot_ = slot;
    randomPlayPosition_ = 0;
    uiRandomSlot_.store(slot);
    uiRandomProgress_.store(0.f);
}

void IntegratedRecorder::stopRandomPlayback() {
    randomPlaySlot_ = -1;
    randomPlayPosition_ = 0;
    randomSamplesRemaining_ = 0;
    uiRandomSlot_.store(-1);
    uiRandomProgress_.store(0.f);
}

void IntegratedRecorder::scheduleNextRandomSwitch() {
    randomState_ = randomState_ * 1664525u + 1013904223u;
    const int seconds = 1 + int(randomState_ % 5u);
    randomSamplesRemaining_ = int64_t(std::llround(sampleRate_ * double(seconds)));
}

void IntegratedRecorder::updateRandomState() {
    const bool enabled = randomEnabled_.load();
    if (enabled && !randomWasEnabled_) {
        const int slot = chooseRandomValidSlot();
        if (slot >= 0) {
            beginRandomPlayback(slot);
            scheduleNextRandomSwitch();
        }
    } else if (!enabled && randomWasEnabled_) {
        stopRandomPlayback();
    }
    randomWasEnabled_ = enabled;
}

void IntegratedRecorder::addManualPlayback(float& l, float& r) {
    for (int s = 0; s < kSlots; ++s) {
        if (!uiPlaying_[s].load()) continue;
        const int len = validSamples_[s].load();
        int& p = playPositions_[s];
        if (len <= 0 || p >= len) {
            p = 0;
            uiPlaying_[s].store(false);
            uiPlaybackProgress_[s].store(0.f);
            continue;
        }
        l += slotL_[s][p];
        r += slotR_[s][p];
        ++p;
        uiPlaybackProgress_[s].store(float(p) / float(len));
        if (p >= len) {
            p = 0;
            uiPlaying_[s].store(false);
            uiPlaybackProgress_[s].store(0.f);
        }
    }
}

void IntegratedRecorder::addRandomPlayback(float& l, float& r) {
    if (!randomEnabled_.load()) return;
    if (randomPlaySlot_ < 0) {
        const int next = chooseRandomValidSlot();
        if (next < 0) return;
        beginRandomPlayback(next);
        scheduleNextRandomSwitch();
    }

    const int len = validSamples_[randomPlaySlot_].load();
    if (len <= 0) {
        stopRandomPlayback();
        return;
    }
    if (randomSamplesRemaining_ <= 0) {
        const int next = chooseRandomValidSlot();
        if (next >= 0) beginRandomPlayback(next);
        scheduleNextRandomSwitch();
    }
    if (randomPlayPosition_ >= len) randomPlayPosition_ = 0;

    l += slotL_[randomPlaySlot_][randomPlayPosition_];
    r += slotR_[randomPlaySlot_][randomPlayPosition_];
    ++randomPlayPosition_;
    --randomSamplesRemaining_;
    if (randomPlayPosition_ >= len) randomPlayPosition_ = 0;
    uiRandomProgress_.store(float(randomPlayPosition_) / float(len));
}

void IntegratedRecorder::process(float inputL, float inputR, float& outputL, float& outputR) {
    outputL = inputL;
    outputR = inputR;

    const float level = std::max(std::abs(inputL), std::abs(inputR));
    inputLevel_.store(inputLevel_.load() * 0.985f + level * 0.015f);

    if (clearRequested_.exchange(false)) clearAll();

    bool rec = recordingEnabled_.load();
    if (rec && !recordingWasEnabled_) beginRecordingSegment();
    else if (!rec && recordingWasEnabled_) finishRecordingSegment();
    recordingWasEnabled_ = rec;

    const int requestedSlot = requestedRecordSlot_.exchange(-1);
    if (rec && requestedSlot >= 0 && requestedSlot < kSlots
            && requestedSlot != writeSlot_ && validSamples_[requestedSlot].load() == 0) {
        finishRecordingSegment();
        beginRecordingAtSlot(requestedSlot);
    }

    rec = recordingEnabled_.load();
    if (rec && writeSlot_ >= 0) {
        if (writePosition_ >= segmentSamples_) {
            finishRecordingSegment();
            if (writeSlot_ == kSlots - 1) {
                recordingEnabled_.store(false);
                recordingWasEnabled_ = false;
            } else {
                beginRecordingSegment();
            }
        }
        if (recordingEnabled_.load() && writeSlot_ >= 0 && writePosition_ < segmentSamples_) {
            slotL_[writeSlot_][writePosition_] = inputL;
            slotR_[writeSlot_][writePosition_] = inputR;
            const int bin = std::clamp(int((int64_t(writePosition_) * kPeakBins) / segmentSamples_), 0, kPeakBins - 1);
            const float a = std::max(std::abs(inputL), std::abs(inputR));
            float old = peaks_[writeSlot_][bin].load();
            while (a > old && !peaks_[writeSlot_][bin].compare_exchange_weak(old, a)) {}
            ++writePosition_;
            validSamples_[writeSlot_].store(writePosition_);
            if (writePosition_ >= segmentSamples_ && writeSlot_ == kSlots - 1) {
                finishRecordingSegment();
                recordingEnabled_.store(false);
                recordingWasEnabled_ = false;
            }
        }
    }

    const uint32_t requests = requestedPlayMask_.exchange(0);
    for (int s = 0; s < kSlots; ++s)
        if ((requests & (1u << uint32_t(s))) != 0) beginPlayback(s);

    updateRandomState();
    addManualPlayback(outputL, outputR);
    addRandomPlayback(outputL, outputR);
}
