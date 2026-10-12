package com.analoglav.amps;
public final class TuningProfileTest {
  public static void main(String[] args){
    TuningProfile p=new TuningProfile();
    for(int i=0;i<TuningProfile.OPTIONS.length;i++){
      if(p.get(i)!=0)throw new AssertionError("initial");
      for(int n=0;n<TuningProfile.OPTIONS[i].length;n++){
        p.next(i);
        if(p.get(i)!=TuningProfile.OPTIONS[i][(n+1)%TuningProfile.OPTIONS[i].length])
          throw new AssertionError("cycle");
      }
      p.setValue(i,987654);
      if(p.get(i)!=0)throw new AssertionError("invalid fallback");
      p.previous(i);
      if(p.get(i)!=TuningProfile.OPTIONS[i][TuningProfile.OPTIONS[i].length-1])
        throw new AssertionError("reverse");
    }
    p.reset();
    if(!p.report().contains("SHARING MODE=PREFER EXCLUSIVE"))throw new AssertionError("report");
    System.out.println("PASS TuningProfile values, clamp, reset, report");
  }
}
