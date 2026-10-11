#include "../app/src/main/cpp/InputFade.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  InputFade fade;fade.reset();
  float x=0;
  for(int i=0;i<80;i++){
    x=fade.next(16384,true);
    assert(std::isfinite(x)&&x>=0&&x<=.5f);
  }
  assert(std::abs(x-.5f)<1e-4f);
  float before=x;
  for(int i=0;i<40;i++){
    x=fade.next(0,false);
    assert(x<=before+1e-7f);
    before=x;
  }
  assert(std::abs(x)<1e-7f);
  for(int i=0;i<100;i++){
    x=fade.next(-16384,true);
    assert(x<=0&&x>=-.5f);
  }
  assert(std::abs(x+.5f)<1e-4f);
  std::cout<<"AMPS InputFade host smoke OK\n";
}
