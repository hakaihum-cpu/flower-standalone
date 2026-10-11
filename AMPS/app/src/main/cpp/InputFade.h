#pragma once
#include <algorithm>
#include <cstdint>

/** Callback-owned, allocation-free input dropout ramp. */
class InputFade {
public:
  void reset(){last_=0;volume_=0;}
  float next(int16_t raw,bool hasInput){
    if(hasInput){
      last_=static_cast<float>(raw)*(1.f/32768.f);
      volume_=std::min(1.f,volume_+1.f/64.f);
    }else{
      volume_=std::max(0.f,volume_-1.f/32.f);
    }
    return last_*volume_;
  }
private:
  float last_=0,volume_=0;
};
