package com.analoglav.amps;

/** Allocation-free per-sample AC30-inspired processor; NOT an exact VOX circuit model. */
public final class AmpDsp {
  public volatile float gain=.43f, volume=.65f, bass=.54f, treble=.63f,
      cut=.24f, master=.65f, spring=.22f, tremSpeed=.31f, tremDepth=.0f;
  public volatile int channel=1, cabinet=0;
  public volatile boolean bypass=false;
  private static final double SR=48000.0;
  private double oldInput, highPass, bassFilter, brightFilter, cabLow,
      cabHigh, oldCab, driveFilter, trimFilter, lfo=0;
  private final Delay[] comb={
    new Delay(1601,.47), new Delay(2351,.49), new Delay(3173,.51)
  };
  private double reverbSmooth;
  private static final class Delay {
    final double[] data;
    final double feedback;
    int at=0;
    double damping;
    Delay(int size,double fb){data=new double[size];feedback=fb;}
    double step(double in){
      double from=data[at];
      damping+=.24*(from-damping);
      data[at]=in+feedback*damping;
      if(++at==data.length)at=0;
      return from;
    }
  }
  private static double clamp(double v,double a,double b){return Math.max(a,Math.min(b,v));}
  public void reset(){
    oldInput=highPass=bassFilter=brightFilter=cabLow=cabHigh=oldCab=driveFilter=trimFilter=lfo=reverbSmooth=0;
    for(Delay c:comb){java.util.Arrays.fill(c.data,0);c.at=0;c.damping=0;}
  }
  private static double ampSaturate(double x) {
    // Unbalanced triode-like transfer; smooth, with limited excursion.
    return Math.tanh(x+.12)-Math.tanh(.12);
  }
  /** Fills one stereo frame in out[0],out[1]; no memory allocation. */
  public void process(float input,float[] out){
    double dry=clamp(input,-1,1);
    if(bypass){out[0]=out[1]=(float)dry;return;}
    double s=dry;
    double hp=s-oldInput+.996*highPass;
    oldInput=s;highPass=hp;s=hp;
    bassFilter+=.017*(s-bassFilter);
    brightFilter+=.085*(s-brightFilter);
    double top=(channel==1)?1.0:0.0;
    // TOP BOOST accentuates the upper mids; bass/treble shaping follows drive.
    double bright=s-brightFilter;
    double inputShape=s + top*(.23+treble*.52)*bright - top*.10*bassFilter;
    double drive=1.0+Math.pow(gain,1.7)*17.0;
    double inputLevel=.18+volume*1.20;
    double driven=inputShape*drive*inputLevel;
    double halfway=.5*(driven+driveFilter);
    driveFilter=driven;
    double pre=.50*(ampSaturate(halfway)+ampSaturate(driven));
    double lows=bassFilter*.28+pre*.72;
    double shaped=pre*(.68+treble*.38)+lows*(bass-.50)*.80
        +(pre-trimFilter)*(treble-.5)*.48;
    trimFilter+=.07*(pre-trimFilter);
    // AC-style cut acts AFTER the preamplifier. Full cut damps upper treble.
    double cutoff=.21-.185*cut;
    cabLow+=cutoff*(shaped-cabLow);
    double postCut=cabLow;
    // Distinct speaker high-frequency damping for Alnico and Green.
    double cabAlpha=cabinet==0?.14:.085;
    cabHigh+=cabAlpha*(postCut-cabHigh);
    double topRoll=cabHigh;
    double cabHp=topRoll-oldCab+.995*cabLowHp;
    oldCab=topRoll;
    cabLowHp=cabHp;
    double cabSignal=cabHp*(cabinet==0?1.06:.97);
    // Tremolo is power/output amplitude modulation, not chorus.
    double speed=.45+Math.pow(tremSpeed,2.0)*11.0;
    lfo+=2.0*Math.PI*speed/SR;
    if(lfo>Math.PI*2)lfo-=Math.PI*2;
    double depth=tremDepth;
    double amp=(1.0-depth)+depth*(.5+.5*Math.sin(lfo));
    double cleanAmp=cabSignal*amp;
    double outStage=Math.tanh(cleanAmp*1.15);
    // Three short feedback combs, damped to spring-ish reverb tail.
    double wet=0;
    double revDrive=outStage*.26;
    for(Delay c:comb)wet+=c.step(revDrive);
    wet/=3.0;
    reverbSmooth+=.12*(wet-reverbSmooth);
    double mixed=outStage*(1.0-spring*.18)+reverbSmooth*spring*1.45;
    double result=Math.tanh(mixed*(.15+master*1.65)*1.08);
    out[0]=(float)clamp(result,-.99,.99);
    out[1]=(float)clamp(result,-.99,.99);
  }
  private double cabLowHp=0;
  private double oldCabHigh(){return cabLowHp;}
}
