import com.analoglav.hydrasynthcontroller.HydraDumpProtocol;
import com.analoglav.hydrasynthcontroller.HydraPatchSnapshot;
import com.analoglav.hydrasynthcontroller.ParameterCatalog;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.ArrayList;

/** Pure Java offline SysEx READ-only regression, no Android device required. */
public final class HydraDumpTest {
  static void eq(Object got,Object expected){
    if(!got.equals(expected))throw new AssertionError(got+" != "+expected);
  }
  static final class Peer implements HydraDumpProtocol.Listener {
    final ArrayList<byte[]> sent=new ArrayList<>();
    String status="",error="";
    byte[] completed;
    @Override public void send(byte[] p){sent.add(p);}
    @Override public void status(String t){status=t;}
    @Override public void complete(byte[] p){completed=p;}
    @Override public void failed(String t){error=t;}
  }
  static byte[] payload(int index,byte[] patch) {
    int length=index==21?102:128;
    byte[] info=new byte[length+4];
    info[0]=0x16; info[1]=0; info[2]=(byte)index; info[3]=0x16;
    System.arraycopy(patch,index*128,info,4,length);
    return HydraDumpProtocol.encode(info);
  }
  public static void main(String[] args) {
    byte[] known=HydraDumpProtocol.encode((byte)4,(byte)0,(byte)0,(byte)127);
    eq(new String(known,6,known.length-7,StandardCharsets.US_ASCII),"GdtjkQQAAH8=");
    eq(Arrays.toString(HydraDumpProtocol.decode(known)),
       Arrays.toString(new byte[]{4,0,0,127}));
    byte[] corrupt=known.clone();
    corrupt[8]=(byte)(corrupt[8]=='A'?'B':'A');
    boolean crcRejected=false;
    try{HydraDumpProtocol.decode(corrupt);}
    catch(IllegalArgumentException ex){crcRejected=true;}
    if(!crcRejected)throw new AssertionError("CRC tampering accepted");
    byte[] original=new byte[2790];
    original[2]=2; original[3]=17; original[4]=(byte)0xDC;
    byte[] n="MY NEW PATCH".getBytes(StandardCharsets.ISO_8859_1);
    System.arraycopy(n,0,original,9,n.length);
    original[82]=4; // OSC 1 Waveform: Square
    original[310]=0; original[311]=0x10; // Filter1 cutoff raw=4096, UI=50%
    original[308]=1; // LP Ladder 24
    original[1606]=100; // Macro 1 panel
    Peer peer=new Peer();
    HydraDumpProtocol.Reader reader=new HydraDumpProtocol.Reader(peer);
    reader.start(2,17);
    eq(peer.sent.size(),1);
    eq(Arrays.toString(HydraDumpProtocol.decode(peer.sent.get(0))),
       Arrays.toString(new byte[]{0x18,0}));
    // The framed MIDI callback may arrive in fragments with MIDI realtime bytes.
    final ArrayList<byte[]> assembled=new ArrayList<>();
    HydraDumpProtocol.Assembler assembler=new HydraDumpProtocol.Assembler(assembled::add);
    byte[] header=HydraDumpProtocol.encode((byte)0x19,(byte)0);
    assembler.feed(header,0,4);
    assembler.feed(new byte[]{(byte)0xF8},0,1);
    assembler.feed(header,4,header.length-4);
    eq(assembled.size(),1);
    reader.accept(assembled.get(0));
    eq(Arrays.toString(HydraDumpProtocol.decode(peer.sent.get(1))),
       Arrays.toString(new byte[]{4,0,2,17}));
    for(int i=0;i<22;i++){
      reader.accept(payload(i,original));
      eq(Arrays.toString(HydraDumpProtocol.decode(peer.sent.get(2+i))),
         Arrays.toString(new byte[]{0x17,0,(byte)i,0x16}));
    }
    eq(Arrays.toString(HydraDumpProtocol.decode(peer.sent.get(24))),
       Arrays.toString(new byte[]{0x1A,0}));
    reader.accept(HydraDumpProtocol.encode((byte)0x1B,(byte)0));
    if(reader.active()||peer.completed==null)throw new AssertionError("Patch read incomplete");
    eq(Arrays.toString(peer.completed),Arrays.toString(original));
    if(!peer.error.isEmpty())throw new AssertionError(peer.error);
    for(byte[] packet:peer.sent)
      if((HydraDumpProtocol.decode(packet)[0]&255)==0x14)
        throw new AssertionError("Read-only flow emitted FLASH WRITE");
    HydraPatchSnapshot snap=new HydraPatchSnapshot(peer.completed);
    eq(snap.name,"MY NEW PATCH");
    eq(snap.bank,2);eq(snap.slot,17);eq(snap.version,0xDC);
    eq(snap.mapped.get("OSC 1|Waveform"),4);
    eq(snap.mapped.get("FILTER 1|Model"),1);
    eq(snap.mapped.get("FILTER 1|Cutoff position"),50);
    eq(snap.mapped.get("MACRO 1|Panel value"),100);
    // Wrong chunk never replaces the editor's last complete patch.
    Peer bad=new Peer();
    HydraDumpProtocol.Reader damaged=new HydraDumpProtocol.Reader(bad);
    damaged.start(2,17);
    damaged.accept(HydraDumpProtocol.encode((byte)0x19,(byte)0));
    damaged.accept(payload(1,original));
    if(bad.error.isEmpty()||bad.completed!=null)
      throw new AssertionError("Out-of-sequence chunk accepted");
    System.out.println("HYDRA DUMP TEST PASS: CRC, fragments, 22 ACKs, slot, values, abort");
  }
}
