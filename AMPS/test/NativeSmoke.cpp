#include "../app/src/main/cpp/AmpModel.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
  AmpModel m;
  AmpModel::Controls c;
  m.setSampleRate(48000);
  m.setControls(c);
  m.reset();
  double cleanEnergy=0;
  for(int i=0;i<48000;i++){
    const float x=.12f*std::sin(6.283185307179586*440.0*i/48000.0);
    const float o=m.process(x);
    assert(std::isfinite(o) && std::abs(o)<=1.f);
    cleanEnergy+=std::abs(o);
  }
  assert(cleanEnergy>10);
  c.spring=1;c.gain=1;c.tremDepth=1;c.tremSpeed=1;c.cabinet=1;
  m.setControls(c);m.reset();
  double fxEnergy=0;
  for(int i=0;i<48000;i++){
    const float o=m.process(.5f*std::sin(6.283185307179586*110.0*i/48000.0));
    assert(std::isfinite(o) && std::abs(o)<=1.f);
    fxEnergy+=std::abs(o);
  }
  assert(fxEnergy>10);
  c.bypass=1;m.setControls(c);
  assert(std::abs(m.process(.5f)-.5f)<.0001f);
  m.setSampleRate(44100);
  c.bypass=0;m.setControls(c);m.reset();
  for(int i=0;i<1000;i++)assert(std::isfinite(m.process(0.f)));
  std::cout<<"AMPS native DSP smoke OK"<<std::endl;
}
