/* Deterministic host-only bounds test for the three Wingie2 SET1 zones. */
#include "mi_piano_set1.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
static uint32_t rng=0x51a7be91u;
static uint32_t rand32(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
int main(void){
 unsigned z,i,transitions=0u;
 for(z=0u;z<MI_PIANO_SET1_COUNT;z++){
  for(i=0u;i<50000u;i++){
   float pos,rate,sample;
   unsigned seed=rand32();
   pos=(seed&3u)==0u?100000.f:(float)(rand32()%MI_PIANO_SET1_SAMPLES);
   rate=(float)(rand32()%1601u)*0.01f;
   sample=mi_piano_set1_tick(z,&pos,rate);
   assert(isfinite(sample));
   assert(sample>=-1.f&&sample<=1.f);
   assert(isfinite(pos));
   assert(pos>=0.f&&pos<(float)MI_PIANO_SET1_SAMPLES);
   if(sample>0.03f||sample<-0.03f)transitions++;
  }
 }
 assert(transitions>10000u);
 printf("PASS: 150000 SET1 flash sample/pitch/random-position stress cases\n");
 return 0;
}
