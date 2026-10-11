#include "../app/src/main/cpp/AudioFifo.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
int main() {
  AudioFifo queue;queue.reset();
  int16_t pcm[960];
  for(int j=0;j<960;j++)pcm[j]=static_cast<int16_t>(
      10000*std::sin(j*.13));
  float out[256];
  queue.push(pcm,960);
  // AAudio may choose a 960 frame (20ms @48k) callback. The app processes
  // it in 256/256/256/192 frame chunks. Target queue is 960 frames.
  for(int n=0;n<5000;n++){
    queue.push(pcm,960);
    for(int j=0;j<3;j++)queue.pop(out,256,960);
    queue.pop(out,192,960);
  }
  assert(queue.starvation()==0);
  assert(queue.overflow()==0);
  assert(queue.available()<2500);
  std::cout<<"AMPS large callback smoke OK\n";
}
