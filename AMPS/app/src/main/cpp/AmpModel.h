#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <algorithm>

// Standalone AC30-inspired (not circuit-measured) amplifier model.
// Owned exclusively by the AAudio output callback: no locks or allocations.
class AmpModel {
public:
  struct Controls {
    float gain=.43f, volume=.65f, bass=.54f, treble=.63f,
          cut=.24f, master=.65f, spring=.22f,
          tremSpeed=.31f, tremDepth=0;
    int channel=1, cabinet=0, bypass=0;
  };
  void setSampleRate(int rate) { sr_ = rate > 0 ? static_cast<double>(rate) : 48000.0; }
  void setControls(const Controls& c) { c_ = c; }
  void reset() {
    previous_=hp_=low_=bright_=cabLp_=cabHp_=cabLast_=preLast_=reverbLp_=phase_=0;
    a_.clear(); b_.clear(); d_.clear();
  }
  float process(float source) {
    const double dry = limit(source, -1.0, 1.0);
    if (c_.bypass) return static_cast<float>(dry);
    const double hp = dry - previous_ + .996 * hp_;
    previous_ = dry; hp_ = hp;
    low_ += .017 * (hp - low_);
    bright_ += .085 * (hp - bright_);
    const double top = c_.channel == 1 ? 1.0 : 0.0;
    const double shape = hp + top * (.23 + c_.treble*.52) * (hp - bright_)
                       - top*.10*low_;
    const double input = shape * (1 + std::pow(c_.gain, 1.7f)*17)
                             * (.18 + c_.volume*1.2);
    const double pre = .5*(saturate(input) + saturate(.5*(input+preLast_)));
    preLast_ = input;
    const double shaped = pre*(.68+c_.treble*.38)
                        + (.28*low_+.72*pre)*(c_.bass-.5)*.8;
    // A low-pass tone cut after the preamp, followed by the 2x12 speaker rolloff.
    cabLp_ += (.21-.185*c_.cut)*(shaped-cabLp_);
    cabHp_ += (c_.cabinet==0 ? .14 : .085)*(cabLp_-cabHp_);
    const double speaker = cabHp_ - cabLast_ + .995*speakerPrevious_;
    cabLast_=cabHp_; speakerPrevious_=speaker;
    phase_ += 6.283185307179586 * (.45 + c_.tremSpeed*c_.tremSpeed*11.0)/sr_;
    if (phase_ >= 6.283185307179586) phase_ -= 6.283185307179586;
    const double amp = (1.0-c_.tremDepth)
                     + c_.tremDepth*(.5+.5*std::sin(phase_));
    const double stage = std::tanh(speaker * (c_.cabinet==0?1.06:.97) * amp * 1.15);
    const double fxInput = stage*.26;
    const double wet = (a_.step(fxInput)+b_.step(fxInput)+d_.step(fxInput))/3.;
    reverbLp_ += .12*(wet-reverbLp_);
    const double mixed=stage*(1.0-c_.spring*.18)+reverbLp_*c_.spring*1.45;
    return static_cast<float>(limit(std::tanh(mixed*(.15+c_.master*1.65)*1.08), -.99, .99));
  }
private:
  template<size_t N> class Comb {
    std::array<double,N> ring_ {};
    size_t pos_=0;
    double damp_=0;
    const double feedback_;
  public:
    explicit Comb(double fb):feedback_(fb){}
    double step(double value) {
      const double old=ring_[pos_];
      damp_+=.24*(old-damp_);
      ring_[pos_]=value+feedback_*damp_;
      if(++pos_==N)pos_=0;
      return old;
    }
    void clear() {ring_.fill(0);pos_=0;damp_=0;}
  };
  Controls c_{};
  double sr_=48000.0,previous_=0,hp_=0,low_=0,bright_=0,cabLp_=0,
         cabHp_=0,cabLast_=0,speakerPrevious_=0,preLast_=0,
         reverbLp_=0,phase_=0;
  Comb<1601> a_{.47};
  Comb<2351> b_{.49};
  Comb<3173> d_{.51};
  static double limit(double v,double min,double max) {
    return std::max(min,std::min(max,v));
  }
  static double saturate(double v) {return std::tanh(v+.12)-std::tanh(.12);}
};
