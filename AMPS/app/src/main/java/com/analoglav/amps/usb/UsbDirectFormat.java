package com.analoglav.amps.usb;

import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import java.util.Locale;

/**
 * Restrictive 48kHz UAC1 PCM Type-I matcher. No guesses about UAC2 clock trees,
 * sample packing or alternative configurations. Other formats fail closed.
 */
public final class UsbDirectFormat {
  private UsbDirectFormat(){}
  public static final class Pair {
    public UsbInterface inputInterface,outputInterface;
    public UsbEndpoint input,output;
    public int subframeBytes,channels=2,rate=48000;
    public int inputFormatBits,outputFormatBits;
    public boolean inputRateControl,outputRateControl;
    public int outputFeedbackAddress;
    public String summary(){
      return String.format(Locale.US,
        "UAC1 PCM 48000Hz %dch x %d-byte samples (%d bits)\nIN IF=%d ALT=%d EP=0x%02X %dbytes\nOUT IF=%d ALT=%d EP=0x%02X %dbytes\nIN_RATE_CTRL=%s OUT_RATE_CTRL=%s OUT_SYNC_TYPE=%d OUT_FEEDBACK_EP=0x%02X\n",
        channels,subframeBytes,inputFormatBits,
        inputInterface.getId(),inputInterface.getAlternateSetting(),
        input.getAddress(),input.getMaxPacketSize(),
        outputInterface.getId(),outputInterface.getAlternateSetting(),
        output.getAddress(),output.getMaxPacketSize(),
        inputRateControl,outputRateControl,
        (output.getAttributes() >> 2)&3,outputFeedbackAddress);
    }
  }
  private static int u8(byte[] d,int p){return d[p]&255;}
  private static int freq(byte[] d,int p){return u8(d,p)|(u8(d,p+1)<<8)|(u8(d,p+2)<<16);}
  private static boolean supports48k(byte[] d,int start,int len){
    if(len<8)return false;
    int count=u8(d,start+7);
    if(count==0)return len>=14 && freq(d,start+8)<=48000 &&
                              freq(d,start+11)>=48000;
    if(len<8+count*3)return false;
    for(int i=0;i<count;i++)if(freq(d,start+8+i*3)==48000)return true;
    return false;
  }
  private static class Alt {
    int ifNum,alt,channels,bytes,bits;
    boolean uac1,pcm,rate48,rateCtrlIn,rateCtrlOut;
    int feedbackAddress=0;
  }
  private static Alt[] discover(byte[] raw){
    if(raw==null)return new Alt[0];
    java.util.ArrayList<Alt> alts=new java.util.ArrayList<>();
    Alt active=null;int p=0;int lastEpDirection=-1;
    while(p+2<=raw.length){
      int len=u8(raw,p),type=u8(raw,p+1);
      if(len<2||p+len>raw.length)break;
      if(type==4 && len>=9){
        active=null;lastEpDirection=-1;
        if(u8(raw,p+5)==1 && u8(raw,p+6)==2){
          active=new Alt();active.ifNum=u8(raw,p+2);
          active.alt=u8(raw,p+3);active.uac1=u8(raw,p+7)==0;
          alts.add(active);
        }
      }else if(type==5 && active!=null && len>=7) {
        lastEpDirection=(u8(raw,p+2)&0x80)!=0?1:0;
        if(lastEpDirection==0 && len>=9)active.feedbackAddress=u8(raw,p+8);
      }else if(type==0x25 && active!=null && len>=4 && u8(raw,p+2)==1){
        boolean canSetRate=(u8(raw,p+3)&1)!=0;
        if(lastEpDirection==1)active.rateCtrlIn=canSetRate;
        else if(lastEpDirection==0)active.rateCtrlOut=canSetRate;
      }else if(type==0x24 && active!=null && len>=5){
        int sub=u8(raw,p+2);
        if(sub==1 && len>=7)active.pcm=u8(raw,p+5)==1 && u8(raw,p+6)==0;
        if(sub==2 && len>=8 && u8(raw,p+3)==1){
          active.channels=u8(raw,p+4);
          active.bytes=u8(raw,p+5);
          active.bits=u8(raw,p+6);
          active.rate48=supports48k(raw,p,len);
        }
      }
      p+=len;
    }
    return alts.toArray(new Alt[0]);
  }
  private static Alt matching(Alt[] all,UsbInterface f){
    for(Alt a:all)if(a.uac1&&a.pcm&&a.rate48&&a.channels==2 &&
      a.bytes>=2 && a.bytes<=4 && a.bits>0 && a.bits<=a.bytes*8 &&
      a.ifNum==f.getId() && a.alt==f.getAlternateSetting())return a;
    return null;
  }
  public static Pair find(UsbDevice device,byte[] raw) {
    Alt[] alts=discover(raw);
    Pair pair=new Pair();Alt fmtIn=null,fmtOut=null;
    for(int i=0;i<device.getInterfaceCount();i++){
      UsbInterface f=device.getInterface(i);
      if(f.getInterfaceClass()!=1 || f.getInterfaceSubclass()!=2 ||
          f.getAlternateSetting()==0)continue;
      Alt fmt=matching(alts,f);
      if(fmt==null)continue;
      for(int e=0;e<f.getEndpointCount();e++){
        UsbEndpoint ep=f.getEndpoint(e);
        if(ep.getType()!=UsbConstants.USB_ENDPOINT_XFER_ISOC ||
            (ep.getAttributes() & 0x30)!=0)continue; // no feedback endpoint
        if(ep.getMaxPacketSize()<12 || ep.getMaxPacketSize()>4096)continue;
        if(ep.getDirection()==UsbConstants.USB_DIR_IN){
          if(pair.input==null){pair.inputInterface=f;pair.input=ep;fmtIn=fmt;}
        }else{
          if(pair.output==null){pair.outputInterface=f;pair.output=ep;fmtOut=fmt;}
        }
      }
    }
    if(fmtIn==null || fmtOut==null || fmtIn.bytes!=fmtOut.bytes ||
       fmtIn.bits!=fmtOut.bits ||
       pair.inputInterface.getId()==pair.outputInterface.getId())return null;
    pair.subframeBytes=fmtIn.bytes;
    pair.inputFormatBits=fmtIn.bits;
    pair.outputFormatBits=fmtOut.bits;
    pair.inputRateControl=fmtIn.rateCtrlIn;
    pair.outputRateControl=fmtOut.rateCtrlOut;
    pair.outputFeedbackAddress=fmtOut.feedbackAddress;
    return pair;
  }
}