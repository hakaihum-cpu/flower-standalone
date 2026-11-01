#pragma once
// EFFECTS EUREKA video scheduler port: frame indices only; no frame assets,
// UI toolkit, MIDI, device driver, dynamic memory, or background threads.
// Derived from the approved 2026-10-31 PluginEditor.cpp and VideoInteraction.h.
// This alone does not establish display hardware support or run audio DSP.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pocket_amp {
class EurekaVisualClock final {
public:
    struct Update { int frame; bool changed; bool cut; float speed_fps; };
    explicit EurekaVisualClock(int frames = 396) noexcept : frame_count_(frames) {}
    Update step(float activity, bool entered, float x) noexcept {
        if (frame_count_ < frames_per_clip) return {0, false, false, 0.0f};
        if (!std::isfinite(activity)) activity = 0.0f;
        if (!std::isfinite(x)) x = 0.0f;
        x = std::clamp(x, 0.0f, 1.0f);
        const bool attack = activity > 0.006f &&
            activity > previous_activity_ * 1.28f + 0.0015f;
        if (visual_cooldown_ > 0) --visual_cooldown_;
        if (clip_cooldown_ > 0) --clip_cooldown_;
        const int clips = frame_count_ / frames_per_clip;
        const bool random_sustain_cut = activity > 0.006f && clip_cooldown_ == 0 &&
            pick(64, -1) == 0;
        const bool cut = clips > 1 && (entered ||
            (clip_cooldown_ == 0 && (attack || random_sustain_cut)));
        const int old_frame = frame_index_;
        if (cut) {
            const int old_clip = frame_index_ / frames_per_clip;
            const int new_clip = pick(clips, old_clip);
            frame_index_ = new_clip * frames_per_clip + pick(frames_per_clip, -1);
            clip_cooldown_ = 18;
            visual_cooldown_ = 8;
            rewind_ = 0;
            frame_phase_ = 0.0f;
        } else if (attack && visual_cooldown_ == 0) {
            visual_cooldown_ = 8;
            rewind_ = std::clamp(1 + static_cast<int>(std::lround(x * 2.0f)), 1, 3);
        }
        // Same energy curve as effects_video::eurekaReactiveFrameRate().
        const float energy = std::clamp((activity - 0.002f) * 20.0f, 0.0f, 1.0f);
        const float fps = energy < 0.01f ? 0.0f : 1.0f + 3.5f * energy;
        speed_fps_ += 0.24f * (fps - speed_fps_);
        if (fps == 0.0f && speed_fps_ < 0.10f) speed_fps_ = 0.0f;
        frame_phase_ += speed_fps_ / 30.0f;
        while (frame_phase_ >= 1.0f) {
            frame_phase_ -= 1.0f;
            const int position = frame_index_ % frames_per_clip;
            const int clip = frame_index_ / frames_per_clip;
            if (rewind_ > 0) {
                frame_index_ = clip * frames_per_clip +
                    (position + frames_per_clip - 1) % frames_per_clip;
                --rewind_;
            } else {
                frame_index_ = clip * frames_per_clip + (position + 1) % frames_per_clip;
            }
        }
        previous_activity_ = 0.60f * previous_activity_ + 0.40f * activity;
        return {frame_index_, frame_index_ != old_frame, cut, speed_fps_};
    }
private:
    int pick(int count, int avoid) noexcept {
        if (count <= 1) return 0;
        random_state_ ^= random_state_ << 13;
        random_state_ ^= random_state_ >> 17;
        random_state_ ^= random_state_ << 5;
        int n = static_cast<int>(random_state_ % static_cast<std::uint32_t>(count));
        if (n == avoid) n = (n + 1) % count;
        return n;
    }
    static constexpr int frames_per_clip = 36;
    int frame_count_ = 396;
    int frame_index_ = 0;
    int visual_cooldown_ = 0;
    int clip_cooldown_ = 0;
    int rewind_ = 0;
    float previous_activity_ = 0.0f;
    float frame_phase_ = 0.0f;
    float speed_fps_ = 0.0f;
    std::uint32_t random_state_ = 0x45464658u;
};
}
