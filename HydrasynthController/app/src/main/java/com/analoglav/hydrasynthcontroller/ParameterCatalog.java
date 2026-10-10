package com.analoglav.hydrasynthcontroller;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Source: ASM MIDI NRPN/CC v1.5 + Explorer Manual 2.2.0.
 *  Unmapped/undocumented parameters are not transmitted.
 *  The waveform list's 0..218 index order requires an Explorer FW2.2 hardware check.
 */
public final class ParameterCatalog {
  public enum Encoding { U14, PACKED, SEMITONE, PERCENT8192, MSB7, BINARY127 }
  public static final class Param {
    public final String group,name,unit;
    public final int msb,lsb,min,max,selector;
    public final Encoding encoding;
    public final String[] options;
    public Param(String g,String n,int a,int b,int lo,int hi,String u,
                 Encoding e,int s,String[] o) {
      group=g;name=n;msb=a;lsb=b;min=lo;max=hi;unit=u;encoding=e;selector=s;options=o;
      if(lo>hi || a<0 || a>127 || b<0 || b>127 || s<0 || s>127
          || (e==Encoding.U14&&lo<0) || hi>16383
          || (o!=null&&o.length!=hi-lo+1))
        throw new IllegalArgumentException("Bad mapping: "+g+"/"+n);
    }
    public String display(int v) {
      return options!=null?options[v-min]:(encoding==Encoding.PERCENT8192?v+"%":v+unit);
    }
    public int encode(int v) {
      if(v<min||v>max)throw new IllegalArgumentException("Outside range");
      if(encoding==Encoding.PERCENT8192)return (v*8192+50)/100;
      if(encoding==Encoding.MSB7)return v<<7;
      if(encoding==Encoding.BINARY127)return v*127;
      if(encoding==Encoding.PACKED)return selector*128+v;
      if(encoding==Encoding.SEMITONE)return selector*128+(v<0?v+128:v);
      return v;
    }
    public String key(){return group+"|"+name;}
  }
  private static final ArrayList<Param> PARAMS=new ArrayList<>();
  private static final ArrayList<String> GROUPS=new ArrayList<>();
  private static final String[] ON={"Off","On"};
  private static void add(Param p){
    PARAMS.add(p);if(!GROUPS.contains(p.group))GROUPS.add(p.group);
  }
  private static void choose(String g,String n,int a,int b,String...opts) {
    add(new Param(g,n,a,b,0,opts.length-1,"",Encoding.U14,0,opts));
  }
  private static void packed(String g,String n,int a,int b,int selector,String...opts) {
    add(new Param(g,n,a,b,0,opts.length-1,"",Encoding.PACKED,selector,opts));
  }
  private static void numeric(String g,String n,int a,int b,int lo,int hi,String suffix) {
    add(new Param(g,n,a,b,lo,hi,suffix,Encoding.U14,0,null));
  }
  /**
   * Coarse, explicitly labeled 0-100% position mapping, never fictional Hz
   * or milliseconds. NRPN v1.5 default range 0..0x2000 => 0..8192.
   * Device/FW-specific curves remain subject to real-hardware validation.
   */
  private static void normalized(String g,String n,int a,int b){
    add(new Param(g,n,a,b,0,100,"%",Encoding.PERCENT8192,0,null));
  }
  private static void waveGroup(List<String> a,String name,int n){
    for(int i=1;i<=n;i++)a.add(name+" "+i);
  }
  private static String[] waveforms() {
    ArrayList<String> a=new ArrayList<>();
    Collections.addAll(a,"Sine","Triangle","TriSaw","Saw","Square");
    waveGroup(a,"Pulse",6);waveGroup(a,"Horizon",8);waveGroup(a,"SyncLav",5);
    waveGroup(a,"Esquire",4);waveGroup(a,"ChriMey",6);waveGroup(a,"Spect A",7);
    waveGroup(a,"Spect X",7);waveGroup(a,"Klangor",5);waveGroup(a,"Induct",3);
    waveGroup(a,"Scorpio",9);waveGroup(a,"Belview",5);waveGroup(a,"Chendom",8);
    waveGroup(a,"Glefan",7);waveGroup(a,"Sqarbel",2);waveGroup(a,"Obob",3);
    waveGroup(a,"Ingvay",3);waveGroup(a,"Particl",3);waveGroup(a,"Vokz",6);
    waveGroup(a,"Flux",5);waveGroup(a,"Alweg",8);waveGroup(a,"Tronic",6);
    waveGroup(a,"Duotone",6);waveGroup(a,"Bobanab",4);waveGroup(a,"Melotic",7);
    waveGroup(a,"Cluster",8);waveGroup(a,"Micoten",5);waveGroup(a,"Orland",8);
    waveGroup(a,"Neuton",7);waveGroup(a,"Xfer",7);waveGroup(a,"Resyn",4);
    waveGroup(a,"Sano",4);waveGroup(a,"SquRoo",15);waveGroup(a,"Harmon",23);
    if(a.size()!=219)throw new IllegalStateException("Wave count "+a.size());
    return a.toArray(new String[0]);
  }
  static {
    String[] w=waveforms();
    for(int i=0;i<3;i++){
      String g="OSC "+(i+1);
      if(i!=2)packed(g,"OSC mode",0x3F,0x18,i,"Single","WaveScan");
      add(new Param(g,"Semitone",0x3F,0x11,-36,36," st",Encoding.SEMITONE,i,null));
      add(new Param(g,"Waveform",0x3F,i==0?0x19:i==1?0x1A:0x0D,
                    0,218,"",Encoding.U14,0,w));
      numeric(g,"Keytrack",0x3F,0x54+i,0,200,"%");
      if(i!=2)for(int k=0;k<8;k++)
        add(new Param(g,"WaveScan WAV "+(k+1),0x3F,
               (i==0?0x60:0x68)+k,0,218,"",Encoding.U14,0,w));
    }
    String[] mutantModes={"FM-Lin","WavStack","OSC Sync","PW-Orig",
                         "PW-Sqeez","PW-ASM","Harmonic","PhazDiff"};
    String[] fm={"Sine","Triangle","OSC 1","OSC 2","OSC 3","Ring Mod",
                 "Noise","Mutator 1","Mutator 2","Mutator 3","Mutator 4",
                 "Mod In 1","Mod In 2"};
    for(int i=0;i<4;i++){
      String g="MUTANT "+(i+1);
      packed(g,"Mode",0x3F,0x21,i,mutantModes);
      packed(g,"FM source",0x3F,0x24,i,fm);
      packed(g,"Sync source",0x3F,0x22,i,"OSC 1","OSC 2","OSC 3");
    }
    choose("RING / NOISE","Noise type",0x3F,0x27,
           "White","Pink","Brown","Red","Blue","Violet","Grey");
    choose("MIXER","Filter routing",0x3F,0x2C,"Series","Parallel");
    normalized("MIXER","OSC 1 level",0x40,0x07);
    normalized("MIXER","OSC 2 level",0x40,0x09);
    normalized("MIXER","OSC 3 level",0x40,0x0B);
    normalized("MIXER","Noise level",0x40,0x0D);
    normalized("MIXER","Ring Mod level",0x40,0x01);
    normalized("FILTER 1","Cutoff position",0x40,0x28);
    normalized("FILTER 1","Resonance",0x40,0x29);
    normalized("FILTER 1","Drive",0x40,0x2B);
    choose("FILTER 1","Model",0x3F,0x28,
           "LP Ladder 12","LP Ladder 24","LP Fat 12","LP Fat 24",
           "Low Pass Gate","LP MS20","HP MS20","LP Threeler","BP Threeler",
           "HP Threeler","Vowel");
    choose("FILTER 1","Drive position",0x3F,0x29,"Pre","Post");
    choose("FILTER 1","Vowel order",0x3F,0x2E,
           "AEIOU","AIUEO","AUIOE","AOUIE","IOUAE","UEAOI","IOEAU","UIEAO");
    normalized("FILTER 2","Cutoff position",0x40,0x2C);
    normalized("FILTER 2","Resonance",0x40,0x2D);
    normalized("FILTER 2","Morph position",0x40,0x2E);
    choose("FILTER 2","Type",0x3F,0x23,"LP-BP-HP","LP-NO-HP");
    choose("FILTER 2","Drive position",0x3F,0x2B,"Pre","Post");
    normalized("DELAY","Wet mix",0x41,0x78);
    normalized("DELAY","Feedback",0x41,0x75);
    choose("DELAY","BPM sync",0x3B,0x70,ON);
    choose("DELAY","Type",0x3B,0x71,
           "Basic","Basic stereo","Pan Delay","LRC Delay","Reverse");
    normalized("REVERB","Wet mix",0x41,0x7E);
    normalized("REVERB","Time position",0x41,0x79);
    choose("REVERB","Type",0x3C,0x72,"Hall","Room","Plate","Cloud");
    normalized("AMP","Level",0x40,0x02);
    for(int i=0;i<5;i++) {
      String g="LFO "+(i+1);
      packed(g,"BPM sync",0x3F,0x04+i,1,ON);
      packed(g,"One shot",0x3F,0x04+i,0x14,ON);
      numeric(g,"Phase",0x3F,0x30+i,0,360," deg");
    }
    // ENV1-5: packed selectors and trigger-source lists confirmed in legacy v1.5.
    final String[] trig={"Off","Note On","LFO 1","LFO 2","LFO 3",
      "LFO 4","LFO 5","Ribbon On","Ribbon Release","Sustain On","Mod In 1","Mod In 2"};
    for(int e=0;e<5;e++){
      String g="ENV "+(e+1);
      // Core envelope contour first: the user sees ADSR together, not a raw list.
      // Source FW1.5 NRPN table: ENV1-5 contiguous address ranges.
      normalized(g,"Attack position",0x41,0x11+e);
      normalized(g,"Decay position",0x41,0x1B+e);
      normalized(g,"Sustain position",0x41,0x20+e);
      normalized(g,"Release position",0x41,0x25+e);
      normalized(g,"Hold position",0x41,0x16+e);
      packed(g,"Legato",0x3F,e,0x07,ON);
      packed(g,"BPM sync",0x3F,e,0x0C,ON);
      packed(g,"Freerun",0x3F,e,0x0D,ON);
      numeric(g,"Attack curve",0x3F,0x70+e,0,128,"");
      numeric(g,"Decay curve",0x3F,0x75+e,0,128,"");
      numeric(g,"Release curve",0x3F,0x7A+e,0,128,"");
      for(int s=0;s<4;s++)
        choose(g,"Trigger "+(s+1),0x3A,0x60+e*4+s,trig);
    }

    // User-friendly ARP fields: packed VV selector in 0x39/0x03.
    packed("ARPEGGIATOR","Division",0x39,0x03,0x01,
      "1/1","1/2","1/4","1/8","1/16","1/32",
      "1/1T","1/2T","1/4T","1/8T","1/16T","1/32T");
    add(new Param("ARPEGGIATOR","Swing",0x39,0x03,50,75,"%",Encoding.PACKED,0x02,null));
    add(new Param("ARPEGGIATOR","Gate",0x39,0x03,5,100,"%",Encoding.PACKED,0x03,null));
    add(new Param("ARPEGGIATOR","Octaves",0x39,0x03,1,4,"",Encoding.PACKED,0x05,null));
    add(new Param("ARPEGGIATOR","Length",0x39,0x03,0,32,"",Encoding.PACKED,0x07,null));
    packed("ARPEGGIATOR","Tap trigger",0x39,0x03,0x08,ON);
    add(new Param("ARPEGGIATOR","Phrase",0x39,0x03,0,63,"",Encoding.PACKED,0x09,null));
    add(new Param("ARPEGGIATOR","Ratchet",0x39,0x03,0,127,"",Encoding.PACKED,0x0A,null));
    add(new Param("ARPEGGIATOR","Chance",0x39,0x03,0,100,"%",Encoding.PACKED,0x0B,null));

    // VOICE: direct 14-bit parameter values documented in v1.5.
    // FW1.5 describes Glide Off/On, but Explorer manual 2.2.0 describes
    // Off/Glide/Glissando. Disagreement: intentionally disable until verified.
    choose("VOICE","Glide legato",0x3F,0x1F,ON);
    choose("VOICE","Polyphonic",0x3F,0x13,ON);
    choose("VOICE","Random phase",0x3F,0x1E,ON);
    choose("VOICE","Warm mode",0x3F,0x4F,ON);
    choose("VOICE","Vibrato BPM",0x3F,0x49,ON);
    numeric("VOICE","Pitch bend range",0x3F,0x41,0,24," st");
    numeric("VOICE","Density",0x3F,0x3C,1,8,"");
    numeric("VOICE","Stereo width",0x3F,0x44,0,127,"");
    numeric("VOICE","Detune",0x3F,0x39,0,127,"");
    numeric("VOICE","Glide time",0x3F,0x15,0,127,"");

    // Macro panel values; Macro Assign/Target/Depth are not yet enabled.
    for(int m=0;m<8;m++)
      numeric("MACRO "+(m+1),"Panel value",0x3F,0x58+m,0,1024,"");

    Collections.addAll(GROUPS,"PRE-FX","POST-FX","MOD MATRIX","SYSTEM");
  }
  private ParameterCatalog(){}
  public static List<String> modules(){return Collections.unmodifiableList(GROUPS);}
  public static List<Param> params(String g){
    ArrayList<Param> a=new ArrayList<>();
    for(Param p:PARAMS)if(p.group.equals(g))a.add(p);
    return a;
  }
  public static int mappedCount(){return PARAMS.size();}
  public static List<Param> all(){return Collections.unmodifiableList(PARAMS);}
  public static Param find(String key){
    if(key==null)return null;
    for(Param p:PARAMS)if(p.key().equals(key))return p;
    return null;
  }
}
