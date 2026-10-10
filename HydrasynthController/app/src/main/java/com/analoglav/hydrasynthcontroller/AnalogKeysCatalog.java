package com.analoglav.hydrasynthcontroller;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Elektron Analog Keys MIDI implementation (OS 1.55, June 2026),
 * official manual Appendix D, pp. D-2 through D-11.
 *
 * Each named single-byte parameter is an NRPN with Data Entry MSB 0..127,
 * not the low 7 bits of a 14-bit value. The two-byte pitch/frequency/depth
 * controls are explicitly marked U14. Never borrow ASM/Hydrasynth mappings.
 * This catalog is SOUND/PARAMETER editing, not +Drive Sound/Kit SysEx.
 */
public final class AnalogKeysCatalog {
  private static final ArrayList<ParameterCatalog.Param> PARAMS=new ArrayList<>();
  private static final String[] WAVE={"SAW","TRP","PUL","TRI","INL","INR","FDB/NEI","OFF"};
  private static final String[] SUB={"OFF","1OCT","2OCT","2PUL","5TH"};
  private static final String[] FILTER2={"LP2","LP1","BP","HP1","HP2","BS","PK"};
  private static final String[] LFO_WAVE={"TRI","SIN","SQR","SAW","EXP","RMP","RND"};
  private static final String[] ENVELOPE_SHAPE={"0","1","2","3","4","5","6","7","8","9","10","11"};
  private static void add(String group,String name,int msb,int lsb,int low,int high,String[] choices) {
    PARAMS.add(new ParameterCatalog.Param(group,name,msb,lsb,low,high,"",
      ParameterCatalog.Encoding.MSB7,0,choices));
  }
  private static void v(String group,String name,int msb,int lsb){add(group,name,msb,lsb,0,127,null);}
  private static void e(String group,String name,int msb,int lsb,String[] labels){
    add(group,name,msb,lsb,0,labels.length-1,labels);
  }
  private static void bit(String group,String name,int msb,int lsb){
    e(group,name,msb,lsb,new String[]{"OFF","ON"});
  }
  private static void hi(String group,String name,int msb,int lsb){
    PARAMS.add(new ParameterCatalog.Param(group,name,msb,lsb,0,16383," /16383",
      ParameterCatalog.Encoding.U14,0,null));
  }
  private static void tens(String group,int msb,int start,String[] labels){
    for(int i=0;i<labels.length;i++)if(labels[i]!=null&&!labels[i].isEmpty())
      v(group,labels[i],msb,start+i);
  }
  static {
    // OSC 1/2. Pitch has MSB+LSB; parameters with missing NRPN address
    // in official appendix (e.g. second Pitch encoder and PWM speed) omitted.
    for(int i=0;i<2;i++){
      final String g="OSC "+(i+1);
      final int base=i==0?0:20;
      hi(g,"Pitch",1,base);
      v(g,"Detune",1,base+2);
      bit(g,"Keytrack",1,base+3);
      v(g,"Level",1,base+4);
      e(g,"Waveform",1,base+5,WAVE);
      e(g,"Sub Osc",1,base+6,SUB);
      v(g,"Pulse Width",1,base+7);
      v(g,"PWM Depth",1,base+9);
    }
    v("NOISE","Sample and Hold",1,10);
    v("NOISE","Fade",1,12);
    v("NOISE","Level",1,14);
    v("OSC COMMON","OSC 1 AM",1,30);
    v("OSC COMMON","Sync Mode",1,31);
    v("OSC COMMON","Sync Amount",1,32);
    v("OSC COMMON","Bend Amount",1,33);
    v("OSC COMMON","Slide Time",1,34);
    v("OSC COMMON","OSC 2 AM",1,35);
    v("OSC COMMON","Note Sync",1,36);
    v("OSC COMMON","Vibrato Fade",1,37);
    v("OSC COMMON","Vibrato Speed",1,38);
    v("OSC COMMON","Vibrato Depth",1,39);

    hi("FILTERS","Filter 1 Freq",1,40);
    v("FILTERS","Filter 1 Res",1,41);
    v("FILTERS","Overdrive",1,42);
    v("FILTERS","Filter 1 Keytrack",1,43);
    v("FILTERS","Filter 1 Env",1,44);
    hi("FILTERS","Filter 2 Freq",1,45);
    v("FILTERS","Filter 2 Res",1,46);
    e("FILTERS","Filter 2 Type",1,47,FILTER2);
    v("FILTERS","Filter 2 Keytrack",1,48);
    v("FILTERS","Filter 2 Env",1,49);

    v("AMP","Attack",1,50);
    v("AMP","Decay",1,51);
    v("AMP","Sustain",1,52);
    v("AMP","Release",1,53);
    e("AMP","Envelope Shape",1,54,ENVELOPE_SHAPE);
    v("AMP","Chorus Send",1,55);
    v("AMP","Delay Send",1,56);
    v("AMP","Reverb Send",1,57);
    v("AMP","Pan",1,58);
    v("AMP","Volume",1,59);

    for(int i=0;i<2;i++){
      final String g=i==0?"ENV F":"ENV 2";
      final int b=i==0?60:70;
      v(g,"Attack",1,b);
      v(g,"Decay",1,b+1);
      v(g,"Sustain",1,b+2);
      v(g,"Release",1,b+3);
      e(g,"Envelope Shape",1,b+4,ENVELOPE_SHAPE);
      v(g,"Gate Length",1,b+5);
      v(g,"Destination A",1,b+6);
      hi(g,"Depth A",1,b+7);
      v(g,"Destination B",1,b+8);
      hi(g,"Depth B",1,b+9);
    }
    for(int i=0;i<2;i++){
      final String g="LFO "+(i+1);
      final int b=i==0?80:90;
      v(g,"Speed",1,b);
      v(g,"Multiplier",1,b+1);
      v(g,"Fade",1,b+2);
      v(g,"Start Phase",1,b+3);
      v(g,"Mode",1,b+4);
      e(g,"Waveform",1,b+5,LFO_WAVE);
      v(g,"Destination A",1,b+6);
      hi(g,"Depth A",1,b+7);
      v(g,"Destination B",1,b+8);
      hi(g,"Depth B",1,b+9);
    }
    // Official Appendix D TRACK NRPN uses Data Entry LSB only, unlike
    // the majority of Sound NRPN values which use Data Entry MSB.
    PARAMS.add(new ParameterCatalog.Param("TRACK","Track Level",1,100,0,127,"",
      ParameterCatalog.Encoding.U14,0,null));
    PARAMS.add(new ParameterCatalog.Param("TRACK","Mute",1,101,0,1,"",
      ParameterCatalog.Encoding.BINARY127,0,new String[]{"OFF","ON"}));

    String[] ext={"Ch1 Chorus","Ch1 Delay","Ch1 Reverb","Ch1 Pan","Ch1 Level",
                  "Ch2 Chorus","Ch2 Delay","Ch2 Reverb","Ch2 Pan","Ch2 Level"};
    tens("EXT IN",2,0,ext);
    String[] cho={"Predelay","Speed","Depth","Width","Feedback","HP Filter",
                  "LP Filter","Delay Send","Reverb Send","Send Level"};
    tens("CHORUS",2,40,cho);
    // Appendix D has unnamed entries 52 and 64, 67, 68: intentionally disabled.
    String[] del={"Time","Mode",null,"Width","Feedback","HP Filter",
                  "LP Filter","Overdrive","Reverb Send","Send Level"};
    tens("DELAY",2,50,del);
    String[] rev={"Predelay","Decay Time","Shelving Freq","Shelving Gain",null,
                  "HP Filter","LP Filter",null,null,"Send Level"};
    tens("REVERB",2,60,rev);
    for(int i=0;i<2;i++){
      final String g="FX LFO "+(i+1);
      final int b=i==0?80:90;
      v(g,"Speed",2,b);v(g,"Multiplier",2,b+1);
      v(g,"Fade",2,b+2);v(g,"Start Phase",2,b+3);
      v(g,"Mode",2,b+4);e(g,"Waveform",2,b+5,LFO_WAVE);
      v(g,"Destination 1",2,b+6);hi(g,"Depth 1",2,b+7);
      v(g,"Destination 2",2,b+8);hi(g,"Depth 2",2,b+9);
    }
    for(int i=0;i<10;i++)
      v("PERFORMANCE","Macro "+(char)('A'+i),0,i);
  }
  private AnalogKeysCatalog(){}
  public static boolean allowed(String group,int track){
    if(track>=0&&track<=3)
      return !(group.equals("PERFORMANCE")||group.equals("EXT IN")||group.equals("CHORUS")||
        group.equals("DELAY")||group.equals("REVERB")||group.startsWith("FX LFO"));
    if(track==4)
      return group.equals("EXT IN")||group.equals("CHORUS")||group.equals("DELAY")||
        group.equals("REVERB")||group.startsWith("FX LFO");
    return track==5&&group.equals("PERFORMANCE");
  }
  public static List<ParameterCatalog.Param> params(String group,int track){
    ArrayList<ParameterCatalog.Param> out=new ArrayList<>();
    if(!allowed(group,track))return out;
    for(ParameterCatalog.Param p:PARAMS)if(p.group.equals(group))out.add(p);
    return out;
  }
  public static List<ParameterCatalog.Param> all(int track){
    ArrayList<ParameterCatalog.Param> out=new ArrayList<>();
    for(ParameterCatalog.Param p:PARAMS)if(allowed(p.group,track))out.add(p);
    return out;
  }
  public static ParameterCatalog.Param find(String key){
    if(key==null)return null;
    for(ParameterCatalog.Param p:PARAMS)if(p.key().equals(key))return p;
    return null;
  }
  public static int mappedCount(){return PARAMS.size();}
}
