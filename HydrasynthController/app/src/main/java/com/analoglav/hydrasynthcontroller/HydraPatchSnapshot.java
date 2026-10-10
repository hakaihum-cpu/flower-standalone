package com.analoglav.hydrasynthcontroller;

import java.util.Arrays;
import java.util.LinkedHashMap;
import java.nio.charset.StandardCharsets;

/**
 * Full raw 2790-byte stored patch snapshot PLUS strictly mapped UI subset.
 * Never fabricates absent parameter values or claims unsaved current edit buffer.
 * Offsets: Edisyn ASMHydrasynth.java parseReal(), firmware 1.5.5/2.0/2.2.
 */
public final class HydraPatchSnapshot {
  public final byte[] raw;
  public final String name;
  public final int bank,slot,version;
  public final LinkedHashMap<String,Integer> mapped=new LinkedHashMap<>();
  public HydraPatchSnapshot(byte[] bytes){
    if(bytes==null||bytes.length!=2790)throw new IllegalArgumentException("Incomplete patch");
    raw=Arrays.copyOf(bytes,bytes.length);
    bank=u8(2);slot=u8(3);version=u8(4);
    if(bank>7||slot>127)throw new IllegalArgumentException("Invalid stored slot");
    String n=new String(raw,9,16,StandardCharsets.ISO_8859_1);
    String cleaned=n.replace((char)0,' ').trim();
    name=cleaned.isEmpty()?"UNTITLED":cleaned;
    decode();
  }
  private int u8(int pos){return raw[pos]&255;}
  private int u16(int pos){return u8(pos)|(u8(pos+1)<<8);}
  private int s8(int pos){return (int)raw[pos];}
  private void put(String group,String field,int value){
    ParameterCatalog.Param p=ParameterCatalog.find(group+"|"+field);
    if(p!=null&&value>=p.min&&value<=p.max)mapped.put(p.key(),value);
  }
  // Most current continuous editors use NRPN [0,8192] internally. Keep exact
  // raw patch in raw[]; UI conversion is only an approximate coarse position.
  private void pos(String group,String field,int rawValue){
    if(rawValue>=0&&rawValue<=8192)
      put(group,field,(rawValue*100+4096)/8192);
  }
  private void decode(){
    for(int i=0;i<3;i++){
      String g="OSC "+(i+1);
      int off=i==0?80:i==1?108:136;
      if(i<2)put(g,"OSC mode",u8(off));
      put(g,"Waveform",u16(off+(i==2?0:2)));
      put(g,"Semitone",s8(off+(i==2?2:4)));
      put(g,"Keytrack",u8(off+(i==2?6:8)));
      if(i<2)for(int w=0;w<8;w++)
        put(g,"WaveScan WAV "+(w+1),u16(off+12+w*2));
    }
    int[] mutant={144,158,204,218};
    for(int i=0;i<4;i++){
      String g="MUTANT "+(i+1);
      int mode=u8(mutant[i]);
      put(g,"Mode",mode);
      if(mode==2)put(g,"Sync source",u8(mutant[i]+2));
      else put(g,"FM source",u8(mutant[i]+2));
    }
    put("RING / NOISE","Noise type",u8(272));
    put("MIXER","Filter routing",u8(302));
    for(int i=0;i<3;i++)pos("MIXER","OSC "+(i+1)+" level",u16(274+i*2));
    pos("MIXER","Ring Mod level",u16(280));
    pos("MIXER","Noise level",u16(282));
    put("FILTER 1","Model",u8(308));
    pos("FILTER 1","Cutoff position",u16(310));
    pos("FILTER 1","Resonance",u16(312));
    pos("FILTER 1","Drive",u16(326));
    put("FILTER 1","Drive position",u8(328));
    put("FILTER 1","Vowel order",u8(330));
    put("FILTER 2","Type",u8(472));
    pos("FILTER 2","Morph position",u16(332));
    pos("FILTER 2","Cutoff position",u16(334));
    pos("FILTER 2","Resonance",u16(336));
    pos("AMP","Level",u16(350));
    put("DELAY","Type",u16(368));
    put("DELAY","BPM sync",u16(370));
    pos("DELAY","Feedback",u16(374));
    pos("DELAY","Wet mix",u16(382));
    put("REVERB","Type",u16(384));
    pos("REVERB","Time position",u16(388));
    pos("REVERB","Wet mix",u16(398));
    for(int i=0;i<5;i++){
      String g="ENV "+(i+1);int off=478+28*i;
      int sync=u8(off+8);
      put(g,"BPM sync",sync);
      // Synced envelopes use a different enum/scale; don't pretend their
      // attack/decay/release are the normal free-time percentages.
      if(sync==0){
        pos(g,"Attack position",u16(off));
        pos(g,"Decay position",u16(off+2));
        pos(g,"Release position",u16(off+6));
        pos(g,"Hold position",u16(off+12));
      }
      pos(g,"Sustain position",u16(off+4));
      put(g,"Legato",u8(off+20));
      put(g,"Freerun",u8(off+24));
    }
    for(int i=0;i<5;i++){
      String g="LFO "+(i+1);int off=618+38*i;
      put(g,"BPM sync",u8(off+4));
      put(g,"One shot",u8(off+20));
      put(g,"Phase",u16(off+12));
    }
    put("ARPEGGIATOR","Division",u8(810));
    put("ARPEGGIATOR","Swing",u8(812));
    put("ARPEGGIATOR","Gate",u8(814));
    put("ARPEGGIATOR","Octaves",u8(818));
    put("ARPEGGIATOR","Length",u8(822));
    put("ARPEGGIATOR","Tap trigger",u8(824));
    put("ARPEGGIATOR","Phrase",u8(826));
    put("ARPEGGIATOR","Ratchet",u8(828));
    put("ARPEGGIATOR","Chance",u8(830));
    put("VOICE","Density",u8(32));
    put("VOICE","Stereo width",u8(42));
    put("VOICE","Detune",u8(34));
    put("VOICE","Pitch bend range",u8(44));
    for(int i=0;i<8;i++)put("MACRO "+(i+1),"Panel value",u16(1606+i*2));
  }
}
