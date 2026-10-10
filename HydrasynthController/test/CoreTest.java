import com.analoglav.hydrasynthcontroller.NrpnEncoder;
import com.analoglav.hydrasynthcontroller.ParameterCatalog;
import java.util.Arrays;
public final class CoreTest {
  private static void eq(Object a,Object b){if(!a.equals(b))throw new AssertionError(a+" != "+b);}
  public static void main(String[] args){
    byte[] a=NrpnEncoder.encode(1,63,24,1);
    eq(Arrays.toString(a),Arrays.toString(new byte[]{
      (byte)0xB0,99,63,(byte)0xB0,98,24,(byte)0xB0,6,0,(byte)0xB0,38,1}));
    byte[] b=NrpnEncoder.encode(16,63,24,129);
    eq((int)(b[0]&0xff),0xBF);eq((int)b[8],1);eq((int)b[11],1);
    boolean rejected=false;
    try{NrpnEncoder.encode(17,1,1,1);}catch(IllegalArgumentException e){rejected=true;}
    if(!rejected)throw new AssertionError("Unsafe channel");
    ParameterCatalog.Param osc=null,semi=null,filter=null,mut=null;
    for(ParameterCatalog.Param p:ParameterCatalog.params("OSC 1")){
      if(p.name.equals("Waveform"))osc=p;
      if(p.name.equals("Semitone"))semi=p;
    }
    for(ParameterCatalog.Param p:ParameterCatalog.params("FILTER 1")){
      if(p.name.equals("Model"))filter=p;
    }
    for(ParameterCatalog.Param p:ParameterCatalog.params("MUTANT 2")){
      if(p.name.equals("Mode"))mut=p;
    }
    if(osc==null||semi==null||filter==null||mut==null)throw new AssertionError("Missing mappings");
    eq(osc.max,218);eq(osc.options.length,219);eq(osc.display(0),"Sine");
    eq(semi.encode(-36),92);eq(semi.encode(36),36);
    eq(filter.max,10);eq(mut.encode(7),135);
    if(ParameterCatalog.params("MOD MATRIX").size()!=0)throw new AssertionError("Unmapped mod matrix must be disabled");
    System.out.println("CORE TEST PASS: "+ParameterCatalog.mappedCount()+" params, MIDI NRPN packing");
  }
}
