#include "../app/src/main/cpp/AudioFifo.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

static void writeSine(int16_t (&pcm)[256],int count,int frame){
  for(int i=0;i<count;i++){
    const double phase=(frame+i)*6.283185307179586*440/48000.;
    pcm[i]=static_cast<int16_t>(10000*std::sin(phase));
  }
}
int main(){
  AudioFifo q;
  q.reset();
  int16_t pcm[256]{};
  float out[256]{};
  // Simulate prefilled capture with synchronized USB input/output callbacks.
  writeSine(pcm,192,0);
  q.push(pcm,192);q.push(pcm,192);
  for(int n=0;n<2000;n++){
    writeSine(pcm,192,n*192);
    q.push(pcm,192);
    q.pop(out,192,192);
    for(float f:out)assert(std::isfinite(f)&&std::abs(f)<=1.f);
  }
  assert(q.overflow()==0);
  assert(q.starvation()==0);
  assert(q.available()<1024);

  // Two separate clocks drift slightly. Fractional reader must keep FIFO
  // bounded instead of chopping a whole block (previous audible buzz).
  q.reset(); q.push(pcm,192);q.push(pcm,192);
  for(int n=0;n<16000;n++){
    q.push(pcm,n%15==0?193:192);
    q.pop(out,192,192);
  }
  assert(q.overflow()==0);
  assert(q.available()<1800);

  q.reset();q.push(pcm,192);q.push(pcm,192);
  for(int n=0;n<16000;n++){
    q.push(pcm,n%15==0?191:192);
    q.pop(out,192,192);
  }
  assert(q.starvation()==0);

  // Input stops abruptly: no invalid values or residual sustained DC.
  q.reset();for(int i=0;i<192;i++)pcm[i]=8000;
  q.push(pcm,192);q.push(pcm,192);
  q.pop(out,192,192);
  q.pop(out,192,192);
  q.pop(out,192,192);
  assert(q.starvation()>0);
  assert(std::abs(out[191])<.0001f);
  std::cout<<"AMPS FIFO smoke passed\n";
}
