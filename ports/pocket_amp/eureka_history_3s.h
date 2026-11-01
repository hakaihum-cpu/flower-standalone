#pragma once
// Memory-oriented storage primitive for the POCKET AMP Eureka port.
// It is NOT the complete Eureka DSP or hardware audio driver.
// Mono guitar input may feed two independently processed lanes;
// this structure keeps two lanes because their echo/loop histories can diverge.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace pocket_amp {
template <unsigned SampleRate> class EurekaHistory3s {
 public:
  static_assert(SampleRate >= 8000 && SampleRate <= 48000, "Unsupported rate");
  static constexpr unsigned capacity = 3U * SampleRate;
  static constexpr unsigned lanes = 2;
  static constexpr unsigned bits_bytes = (capacity + 7U) / 8U;
  static constexpr unsigned storage_bytes = lanes * (capacity * sizeof(std::int16_t) + bits_bytes);
  void clear() noexcept {
    for (auto& bits : valid_) bits.fill(0);
    write_pos_ = {0, 0};
    valid_count_ = {0, 0};
  }
  EurekaHistory3s() noexcept { clear(); }
  void write(unsigned lane, float in, unsigned loop, bool echo) noexcept {
    if (lane >= lanes || loop < 64 || loop > capacity) return;
    unsigned& w = write_pos_[lane];
    if (w >= loop) w = 0;
    const float old = valid(lane, w) ? from_pcm(audio_[lane][w]) : 0.0f;
    const float sample = echo ? (0.5f * old + 0.5f * in) : in;
    audio_[lane][w] = to_pcm(sample);
    valid_[lane][w >> 3] |= static_cast<std::uint8_t>(1U << (w & 7U));
    valid_count_[lane] = std::min(loop, valid_count_[lane] + 1U);
    if (++w >= loop) w = 0;
  }
  float read(unsigned lane, float position, unsigned loop) const noexcept {
    if (lane >= lanes || loop < 64 || loop > capacity || !std::isfinite(position)) return 0.0f;
    const float wrapped = position - std::floor(position / static_cast<float>(loop)) * static_cast<float>(loop);
    const unsigned i0 = static_cast<unsigned>(wrapped);
    const unsigned i1 = (i0 + 1U) % loop;
    const float frac = wrapped - static_cast<float>(i0);
    const float a = valid(lane,i0) ? from_pcm(audio_[lane][i0]) : 0.0f;
    const float b = valid(lane,i1) ? from_pcm(audio_[lane][i1]) : 0.0f;
    return a + (b - a) * frac;
  }
  unsigned written(unsigned lane) const noexcept { return lane < lanes ? valid_count_[lane] : 0U; }
  unsigned write_position(unsigned lane) const noexcept { return lane < lanes ? write_pos_[lane] : 0U; }
 private:
  static std::int16_t to_pcm(float x) noexcept {
    if (!std::isfinite(x)) x = 0.0f;
    x = std::max(-1.0f, std::min(1.0f, x));
    return static_cast<std::int16_t>(std::lrint(x * 32767.0f));
  }
  static float from_pcm(std::int16_t x) noexcept { return static_cast<float>(x) / 32767.0f; }
  bool valid(unsigned lane, unsigned i) const noexcept {
    return (valid_[lane][i >> 3] & static_cast<std::uint8_t>(1U << (i & 7U))) != 0;
  }
  alignas(4) std::array<std::array<std::int16_t, capacity>, lanes> audio_{};
  std::array<std::array<std::uint8_t, bits_bytes>, lanes> valid_{};
  std::array<unsigned, lanes> write_pos_{};
  std::array<unsigned, lanes> valid_count_{};
};
}
