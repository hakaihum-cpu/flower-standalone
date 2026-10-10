import com.analoglav.hydrasynthcontroller.AnalogKeysCatalog;
import com.analoglav.hydrasynthcontroller.ParameterCatalog;
import com.analoglav.hydrasynthcontroller.NrpnEncoder;
import java.util.Arrays;
import java.util.HashSet;

/** Pure Java regression of the official Analog Keys NRPN addresses and value encodings. */
public final class AnalogKeysCoreTest {
  private static void eq(Object got,Object expected){
    if(!got.equals(expected))throw new AssertionError(got+" != "+expected);
  }
  private static ParameterCatalog.Param get(String key){
    ParameterCatalog.Param p=AnalogKeysCatalog.find(key);
    if(p==null)throw new AssertionError("Missing Analog Keys mapping: "+key);
    return p;
  }
  public static void main(String[] args) {
    ParameterCatalog.Param osc1=get("OSC 1|Waveform");
    eq(osc1.msb,1);eq(osc1.lsb,5);eq(osc1.max,7);
    eq(osc1.encode(3),384);
    eq(osc1.display(3),"TRI");
    byte[] note=NrpnEncoder.encode(2,osc1.msb,osc1.lsb,osc1.encode(3));
    eq(Arrays.toString(note),Arrays.toString(new byte[]{
      (byte)0xB1,99,1,(byte)0xB1,98,5,(byte)0xB1,6,3,(byte)0xB1,38,0}));
    eq(get("OSC 2|Waveform").lsb,25);
    eq(get("OSC 1|Pitch").encode(16383),16383);
    eq(get("FILTERS|Filter 1 Freq").lsb,40);
    eq(get("FILTERS|Filter 1 Freq").encode(8192),8192);
    eq(get("AMP|Attack").lsb,50);
    eq(get("AMP|Attack").encode(127),16256);
    eq(get("ENV 2|Attack").lsb,70);
    eq(get("LFO 2|Waveform").lsb,95);
    eq(get("CHORUS|Feedback").msb,2);
    eq(get("CHORUS|Feedback").lsb,44);
    eq(get("DELAY|Time").lsb,50);
    eq(get("REVERB|Decay Time").lsb,61);
    eq(get("PERFORMANCE|Macro J").msb,0);
    eq(get("PERFORMANCE|Macro J").lsb,9);
    // Track mute/level specifically use Data Entry LSB in the official manual.
    eq(get("TRACK|Mute").encode(127),127);
    eq(get("TRACK|Track Level").encode(64),64);
    if(!AnalogKeysCatalog.allowed("OSC 1",0)||
      AnalogKeysCatalog.allowed("OSC 1",4)||
      AnalogKeysCatalog.allowed("DELAY",0)||
      !AnalogKeysCatalog.allowed("DELAY",4)||
      AnalogKeysCatalog.allowed("PERFORMANCE",4)||
      !AnalogKeysCatalog.allowed("PERFORMANCE",5))
      throw new AssertionError("Track/FX/performance module isolation broken");
    if(AnalogKeysCatalog.params("OSC 1",3).isEmpty()||
      !AnalogKeysCatalog.params("OSC 1",4).isEmpty()||
      AnalogKeysCatalog.all(5).size()!=10)
      throw new AssertionError("Analog Keys track/FX field catalog mismatch");
    HashSet<String> names=new HashSet<>();
    for(int t=0;t<6;t++)for(ParameterCatalog.Param p:AnalogKeysCatalog.all(t)){
      if(p.msb<0||p.msb>3)throw new AssertionError("Unsafe NRPN MSB");
      if(p.encode(p.max)>16383)throw new AssertionError("Unsafe NRPN value");
      if(p.encoding==ParameterCatalog.Encoding.PERCENT8192)
        throw new AssertionError("Hydrasynth encoding leaked into Analog Keys");
      names.add(p.key());
    }
    if(names.size()!=AnalogKeysCatalog.mappedCount())
      throw new AssertionError("Missing or duplicate Analog Keys catalog entries");
    System.out.println("ANALOG KEYS TEST PASS: "+names.size()+
      " official sound/FX/performance parameter mappings, MSB7 and U14 separation");
  }
}
