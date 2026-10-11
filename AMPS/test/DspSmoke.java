package com.analoglav.amps;
public final class DspSmoke {
  public static void main(String[] args) {
    AmpDsp dsp=new AmpDsp();
    float[] l=new float[2];
    float energy=0;
    for(int i=0;i<48000;i++){
      dsp.process((float)(.12*Math.sin(2*Math.PI*440*i/48000.)),l);
      if(!Float.isFinite(l[0]) || !Float.isFinite(l[1]))
        throw new AssertionError("DSP non-finite output");
      if(Math.abs(l[0])>1 || Math.abs(l[1])>1)
        throw new AssertionError("DSP output overshoot");
      energy+=Math.abs(l[0]);
    }
    if(energy<1)throw new AssertionError("DSP produced silence");
    dsp.gain=1;dsp.spring=1;dsp.tremDepth=1;dsp.reset();
    for(int i=0;i<24000;i++){
      dsp.process((float)(.6*Math.sin(2*Math.PI*110*i/48000.)),l);
      if(!Float.isFinite(l[0]))throw new AssertionError("FX non-finite");
    }
    dsp.bypass=true;dsp.process(.5f,l);
    if(Math.abs(l[0]-.5f)>.0001)throw new AssertionError("bypass mismatch");
    System.out.println("AMPS DSP static smoke OK");
  }
}
