package com.analoglav.hydrasynthcontroller;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import java.util.Base64;
import java.util.zip.CRC32;

/**
 * READ-ONLY Hydrasynth slot dump protocol.
 * Reverse-engineered reference: Edisyn's SysexEncoding.txt and Decode.java,
 * NOT a manufacturer-published current-edit-buffer query.
 * Never emits a write request (14 00).
 */
public final class HydraDumpProtocol {
  private HydraDumpProtocol() {}
  private static final byte[] PREFIX={(byte)0xF0,0,0x20,0x2B,0,0x6F};
  public static byte[] encode(byte... info) {
    CRC32 crc=new CRC32();crc.update(info);
    long sum=crc.getValue();
    byte[] payload=new byte[info.length+4];
    for(int i=0;i<4;i++)payload[i]=(byte)(255-((sum >>> (8*i))&255));
    System.arraycopy(info,0,payload,4,info.length);
    byte[] ascii=Base64.getEncoder().encode(payload);
    byte[] packet=new byte[PREFIX.length+ascii.length+1];
    System.arraycopy(PREFIX,0,packet,0,PREFIX.length);
    System.arraycopy(ascii,0,packet,PREFIX.length,ascii.length);
    packet[packet.length-1]=(byte)0xF7;
    return packet;
  }
  public static byte[] decode(byte[] packet){
    if(packet==null||packet.length<15||packet[packet.length-1]!=(byte)0xF7)
      throw new IllegalArgumentException("Invalid SysEx frame");
    for(int i=0;i<PREFIX.length;i++)if(packet[i]!=PREFIX[i])
      throw new IllegalArgumentException("Not a Hydrasynth SysEx");
    byte[] body=Base64.getDecoder().decode(Arrays.copyOfRange(packet,6,packet.length-1));
    if(body.length<6)throw new IllegalArgumentException("SysEx payload too short");
    byte[] info=Arrays.copyOfRange(body,4,body.length);
    CRC32 crc=new CRC32();crc.update(info);
    long sum=crc.getValue();
    for(int i=0;i<4;i++)if((body[i]&255)!=(255-((sum>>>(8*i))&255)))
      throw new IllegalArgumentException("Hydrasynth SysEx CRC mismatch");
    return info;
  }

  /** Fragment-tolerant frame collector; MidiReceiver.onSend may split packets arbitrarily. */
  public static final class Assembler {
    public interface Listener{void onFrame(byte[] frame);}
    private final Listener listener;
    private ByteArrayOutputStream bytes;
    public Assembler(Listener l){listener=l;}
    public void reset(){bytes=null;}
    public void feed(byte[] b,int offset,int count){
      if(b==null||offset<0||count<0||offset>b.length-count)
        throw new IllegalArgumentException("MIDI input bounds");
      for(int i=offset;i<offset+count;i++){
        int v=b[i]&255;
        if(v==0xF0){bytes=new ByteArrayOutputStream(256);bytes.write(v);continue;}
        if(bytes==null)continue;
        if(v==0xF7){
          bytes.write(v);byte[] frame=bytes.toByteArray();bytes=null;
          listener.onFrame(frame);continue;
        }
        if(v>=0xF8)continue; // interleaved real-time messages are legal
        if(v>=0x80){bytes=null;continue;} // malformed SysEx; resynchronize at F0
        if(bytes.size()>=4096){bytes=null;continue;}
        bytes.write(v);
      }
    }
  }

  public interface Listener {
    void send(byte[] sysex);
    void status(String message);
    void complete(byte[] fullPatch);
    void failed(String reason);
  }
  /** Owns read transaction state; timeout and serialization are supplied by Activity. */
  public static final class Reader {
    private final Listener listener;
    private int state; // 0 idle, 1 header ACK, 2 incoming chunks, 3 footer ACK
    private int bank,slot,chunk;
    private final ByteArrayOutputStream dump=new ByteArrayOutputStream(2790);
    public Reader(Listener l){listener=l;}
    public boolean active(){return state!=0;}
    public void start(int bankIndex,int slotIndex) {
      if(active())throw new IllegalStateException("Read already active");
      if(bankIndex<0||bankIndex>7||slotIndex<0||slotIndex>127)
        throw new IllegalArgumentException("Bank/slot out of range");
      bank=bankIndex;slot=slotIndex;chunk=0;dump.reset();
      state=1;
      listener.status("CURRENT LOAD / REQUESTING SAVED SLOT");
      listener.send(encode((byte)0x18,(byte)0));
    }
    public void cancel(String why){
      if(!active())return;
      state=0;dump.reset();
      try{listener.send(encode((byte)0x1A,(byte)0));}
      finally{listener.failed(why);}
    }
    public void accept(byte[] sysex) {
      if(!active())return;
      final byte[] m;
      try{m=decode(sysex);}
      catch(IllegalArgumentException e){cancel("INVALID SYSEX / "+e.getMessage());return;}
      if(m.length<2)return;
      final int command=m[0]&255;
      if(state==1){
        if(command!=0x19||m[1]!=0)return;
        state=2;
        listener.send(encode((byte)0x04,(byte)0,(byte)bank,(byte)slot));
        listener.status("READING SLOT "+(char)('A'+bank)+"-"+(slot+1)+" / 0 OF 22");
        return;
      }
      if(state==2){
        if(command!=0x16)return;
        int count=(chunk==21?102:128);
        if(m.length!=count+4||(m[1]&255)!=0||(m[2]&255)!=chunk||(m[3]&255)!=0x16){
          cancel("BAD PATCH CHUNK "+chunk);return;
        }
        dump.write(m,4,count);
        listener.send(encode((byte)0x17,(byte)0,(byte)chunk,(byte)0x16));
        chunk++;
        listener.status("READING SLOT / "+chunk+" OF 22");
        if(chunk==22){
          state=3;
          listener.send(encode((byte)0x1A,(byte)0));
        }
        return;
      }
      if(state==3&&command==0x1B&&m[1]==0){
        byte[] result=dump.toByteArray();
        state=0;dump.reset();
        if(result.length!=2790){
          listener.failed("INCOMPLETE PATCH: "+result.length);return;
        }
        if((result[2]&255)!=bank||(result[3]&255)!=slot){
          listener.failed("PATCH SLOT MISMATCH");return;
        }
        int version=result[4]&255;
        if(version!=0x9B&&version!=0xC8&&version!=0xDC&&version!=0xCB){
          listener.failed("UNRECOGNIZED PATCH VERSION "+version);return;
        }
        listener.complete(result);
      }
    }
  }
}
