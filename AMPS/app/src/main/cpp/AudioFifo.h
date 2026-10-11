#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

// Lock-free, no-heap, single-producer / single-consumer PCM16 FIFO.
// Producer: AAudio input callback. Consumer: AAudio output callback.
// The fractional consumer speed tracks modest input/output clock drift
// without continuously discarding whole blocks (which made periodic buzz).
class AudioFifo {
 public:
  static constexpr uint32_t kCapacity = 16384;
  void reset() {
    reader_.store(0, std::memory_order_relaxed);
    writer_.store(0, std::memory_order_relaxed);
    overflow_.store(0, std::memory_order_relaxed);
    starvation_.store(0, std::memory_order_relaxed);
    frac_ = 0;
    ratioPpm_.store(0,std::memory_order_relaxed);
    lastSample_ = 0;
    envelope_ = 0;
    ratio_ = 1;
  }
  uint32_t available() const {
    return writer_.load(std::memory_order_acquire) -
           reader_.load(std::memory_order_acquire);
  }
  uint32_t overflow() const {return overflow_.load(std::memory_order_relaxed);}
  uint32_t starvation() const {return starvation_.load(std::memory_order_relaxed);}
  int ratioPartsPerMillion() const {
    return ratioPpm_.load(std::memory_order_relaxed);
  }
  void push(const int16_t* src, int frames) {
    uint32_t w=writer_.load(std::memory_order_relaxed);
    // Loading read once avoids an atomic operation for every sample.
    uint32_t r=reader_.load(std::memory_order_acquire);
    uint32_t lost=0;
    for(int i=0;i<frames;i++){
      if(w-r<kCapacity-2) buffer_[w++%kCapacity]=src[i];
      else ++lost;
    }
    writer_.store(w,std::memory_order_release);
    if(lost)overflow_.fetch_add(lost,std::memory_order_relaxed);
  }
  // Callback thread only. Writes floats for further DSP processing.
  // 1-2 burst prefill is supplied by the AAudio owner before output starts.
  // A gentle latency stabilizer corrects clock drift (+/-1%) by continuous
  // resampling instead of dumping discontinuous blocks at every callback.
  void pop(float* dest,int frames,int targetFrames) {
    uint32_t r=reader_.load(std::memory_order_relaxed);
    const uint32_t begin=writer_.load(std::memory_order_acquire);
    const int target=std::max(64,std::min(targetFrames,2048));
    // Control ratio per callback, not once per sample. Slow its response.
    const int error=static_cast<int>(begin-r)-(target+frames);
    const double ideal=std::max(.990,std::min(1.010,
                                           1. + .000015*static_cast<double>(error)));
    ratio_ += .12*(ideal-ratio_);
    ratioPpm_.store(static_cast<int>((ratio_-1.)*1000000.),
                    std::memory_order_relaxed);
    uint32_t writeSnapshot=begin;
    uint32_t missing=0;
    for(int i=0;i<frames;i++){
      if(writeSnapshot-r<2)
        writeSnapshot=writer_.load(std::memory_order_acquire);
      float sample=0;
      if(writeSnapshot-r>=2){
        const float first=buffer_[r%kCapacity]*(1.f/32768.f);
        const float second=buffer_[(r+1)%kCapacity]*(1.f/32768.f);
        sample=static_cast<float>(first+(second-first)*frac_);
        frac_+=ratio_;
        const auto advance=static_cast<uint32_t>(frac_);
        frac_-=advance;
        r+=advance;
        lastSample_=sample;
        envelope_=std::min(1.f,envelope_+(1.f/64.f));
      } else {
        // Smooth very short discontinuities, then mute. Never jump directly
        // between full-amplitude PCM and zero as in previous implementation.
        sample=lastSample_;
        envelope_=std::max(0.f,envelope_-(1.f/32.f));
        ++missing;
      }
      dest[i]=sample*envelope_;
    }
    reader_.store(r,std::memory_order_release);
    if(missing)starvation_.fetch_add(missing,std::memory_order_relaxed);
  }
 private:
  std::array<int16_t,kCapacity> buffer_{};
  std::atomic<uint32_t> reader_{0},writer_{0};
  std::atomic<uint32_t> overflow_{0},starvation_{0};
  std::atomic<int> ratioPpm_{0};
  double frac_=0,ratio_=1;
  float lastSample_=0,envelope_=0;
};
