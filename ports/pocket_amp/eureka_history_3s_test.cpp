#include "eureka_history_3s.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
using pocket_amp::EurekaHistory3s;
static EurekaHistory3s<44100> h;
int main() {
  static_assert(EurekaHistory3s<44100>::storage_bytes == 562276, "Unexpected bitset budget");
  assert(h.read(0, 0, 128) == 0.0f);
  h.write(0, 0.5f, 128, false);
  assert(std::abs(h.read(0, 0, 128) - 0.5f) < 1.0f/32767.0f);
  assert(h.read(0, 1, 128) == 0.0f);
  assert(h.read(1, 0, 128) == 0.0f);
  h.write(1, -0.25f, 128, false);
  assert(std::abs(h.read(1, 0, 128) + 0.25f) < 1.0f/32767.0f);
  assert(std::abs(h.read(0, 0.5f, 128) - 0.25f) < 1.0f/32767.0f);
  h.clear();
  assert(h.read(0, 0, 128)==0.0f && h.read(1, 0, 128)==0.0f);
  assert(h.written(0)==0 && h.written(1)==0);
  // Echo must never use stale audio after reset.
  h.write(0, 0.5f, 128, true);
  assert(std::abs(h.read(0, 0, 128) - 0.25f) < 1.0f/32767.0f);
  h.clear();
  // Independent loop lengths, repeated dynamic resize and wrap.
  for (unsigned n = 0; n < 10000; ++n) {
    const unsigned length0 = 64 + (n/256 % 7)*211;
    const unsigned length1 = 127 + (n/331 % 9)*119;
    h.write(0,std::sin(n*0.15f)*0.7f,length0,(n%31)==0);
    h.write(1,std::cos(n*0.021f)*0.6f,length1,(n%43)==0);
    const float a=h.read(0,static_cast<float>(n % length0)+0.37f,length0);
    const float b=h.read(1,static_cast<float>(n % length1)-0.75f,length1);
    assert(std::isfinite(a) && std::abs(a)<=1.0001f);
    assert(std::isfinite(b) && std::abs(b)<=1.0001f);
  }
  h.clear();
  assert(h.read(0,0,512)==0.0f && h.read(1,0,512)==0.0f);
  // Entire 3 s capacity must be addressable and survive wrapping.
  for(unsigned i=0;i<EurekaHistory3s<44100>::capacity;++i) h.write(0,0.25f,EurekaHistory3s<44100>::capacity,false);
  assert(h.written(0)==EurekaHistory3s<44100>::capacity);
  assert(std::abs(h.read(0,EurekaHistory3s<44100>::capacity-1.0f,EurekaHistory3s<44100>::capacity)-0.25f)<1.0f/32767.0f);
  std::puts("PASS: 3-second storage, two lanes, clear/echo/variable loop/wrap and bounds");
}
