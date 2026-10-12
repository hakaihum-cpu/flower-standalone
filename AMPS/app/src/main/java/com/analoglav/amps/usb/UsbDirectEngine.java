package com.analoglav.amps.usb;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import com.analoglav.amps.AmpDsp;

/**
 * Blocking, native direct USB PCM stream. Invoke execute() on a worker thread.
 * Never routes audio via AAudio. Fails closed if UAC1 48k PCM cannot be proven.
 */
public final class UsbDirectEngine {
  static { System.loadLibrary("amps_audio"); }
  private UsbDirectEngine(){}
  private static native String nativeRun(int fd,int inputEp,int outputEp,
      int maxIn,int maxOut,int intervalIn,int intervalOut,
      int subframeBytes,int validBits,int queueDepth);
  private static native void nativeStop();
  private static native int[] nativeCounters();
  private static native void nativeControls(float[] values,int channel,int cabinet,int bypass);

  public static void stop(){nativeStop();}
  public static boolean isRunning(){
    int[] nativeValues=nativeCounters();
    return nativeValues!=null && nativeValues.length>=1 && nativeValues[0]==1;
  }
  public static String stats(){
    int[] s=nativeCounters();
    if(s==null||s.length!=7)return "USB DIRECT STATS UNAVAILABLE";
    return "RUN="+s[0]+" IN="+s[1]+" OUT="+s[2]+
        " RX_BYTES="+s[3]+" TX_BYTES="+s[4]+
        " UNDERRUN="+s[5]+" USB_ERRORS="+s[6];
  }
  public static void update(AmpDsp dsp){
    float[] values={dsp.gain,dsp.volume,dsp.bass,dsp.treble,dsp.cut,
        dsp.master,dsp.spring,dsp.tremSpeed,dsp.tremDepth};
    nativeControls(values,dsp.channel,dsp.cabinet,dsp.bypass?1:0);
  }

  /**
   * UAC1 endpoint Sampling Frequency Control (only if explicitly advertised).
   * 48,000 Hz is encoded as 80 BB 00, SET_CUR to endpoint control selector 1.
   */
  private static boolean set48kWhenSupported(UsbDeviceConnection c,
      UsbEndpoint ep,boolean advertised,StringBuilder report){
    if(!advertised){
      report.append("SAMPLE_RATE_CONTROL EP=0x")
          .append(Integer.toHexString(ep.getAddress()))
          .append(" FIXED_OR_UNADVERTISED\n");
      return true;
    }
    byte[] rate=new byte[]{(byte)0x80,(byte)0xBB,0};
    int rc=c.controlTransfer(0x22,0x01,0x0100,ep.getAddress(),rate,3,1200);
    report.append("UAC1_SET_CUR_48K EP=0x")
        .append(Integer.toHexString(ep.getAddress())).append(" RESULT=")
        .append(rc).append('\n');
    if(rc!=3)return false;
    byte[] actual=new byte[3];
    int got=c.controlTransfer(0xA2,0x81,0x0100,ep.getAddress(),actual,3,1200);
    if(got==3){
      int confirmed=(actual[0]&255)|((actual[1]&255)<<8)|((actual[2]&255)<<16);
      report.append("UAC1_GET_CUR_HZ=").append(confirmed).append('\n');
      return confirmed==48000;
    }
    report.append("UAC1_GET_CUR=UNAVAILABLE (SET_CUR ACKNOWLEDGED)\n");
    return true;
  }
  // USB standard GET_INTERFACE (bRequest=0x0A). Checking the active
  // alternate setting helps distinguish endpoint shutdown from a HAL or
  // kernel USB-audio reconfiguration while this process claims the device.
  private static void reportInterfaceAlt(UsbDeviceConnection c,int iface,
      String phase,StringBuilder report){
    byte[] alt=new byte[1];
    int rc=c.controlTransfer(0x81,0x0A,0,iface,alt,1,800);
    report.append("GET_INTERFACE_").append(phase)
        .append("_IF").append(iface).append("_RC=").append(rc);
    if(rc==1)report.append(" ALT=").append(alt[0]&255);
    report.append('\n');
  }
  /**
   * Match a UAC1 AudioControl HEADER's baInterfaceNr[] to the specific
   * playback and capture streaming interfaces. iRig PRO DUO has TWO AC
   * interfaces: IF0 owns audio IF1/IF2; IF3 owns MIDI IF4. Never claim IF3.
   */
  private static UsbInterface findAudioControl(UsbDevice d,byte[] raw,
      int playbackIf,int captureIf){
    if(raw==null)return null;
    int audioControlId=-1,matchedId=-1,p=0;
    while(p+2<=raw.length){
      final int len=raw[p]&255, type=raw[p+1]&255;
      if(len<2||p+len>raw.length)break;
      if(type==4 && len>=9){
        if((raw[p+5]&255)==1 && (raw[p+6]&255)==1 &&
            (raw[p+7]&255)==0)audioControlId=raw[p+2]&255;
        else audioControlId=-1;
      }else if(type==0x24 && len>=8 && audioControlId>=0 &&
          (raw[p+2]&255)==1) {
        final int n=raw[p+7]&255;
        if(len>=8+n){
          boolean playback=false,capture=false;
          for(int k=0;k<n;k++){
            final int target=raw[p+8+k]&255;
            if(target==playbackIf)playback=true;
            if(target==captureIf)capture=true;
          }
          if(playback && capture){
            if(matchedId>=0)return null; // Ambiguous: fail closed
            matchedId=audioControlId;
          }
        }
      }
      p+=len;
    }
    if(matchedId<0)return null;
    for(int i=0;i<d.getInterfaceCount();i++){
      UsbInterface f=d.getInterface(i);
      if(f.getId()==matchedId && f.getInterfaceClass()==1 &&
          f.getInterfaceSubclass()==1 && f.getAlternateSetting()==0)
        return f;
    }
    return null;
  }
  public static String execute(Context ctx,UsbDevice device,int depth,AmpDsp dsp) {
    if(device==null)return "USB_DIRECT_ERROR=NO_DEVICE\n";
    UsbManager manager=(UsbManager)ctx.getSystemService(Context.USB_SERVICE);
    if(manager==null)return "USB_DIRECT_ERROR=NO_MANAGER\n";
    if(!manager.hasPermission(device))return "USB_DIRECT_ERROR=PERMISSION_DENIED\n";
    UsbDeviceConnection c=null;
    UsbDirectFormat.Pair pair=null;
    boolean claimInput=false,claimOutput=false,claimControl=false;
    UsbInterface control=null;
    StringBuilder report=new StringBuilder();
    try{
      c=manager.openDevice(device);
      if(c==null)return "USB_DIRECT_ERROR=OPEN_DEVICE_FAILED\n";
      pair=UsbDirectFormat.find(device,c.getRawDescriptors());
      if(pair==null)
        return "USB_DIRECT_ERROR=NO_VERIFIED_UAC1_48K_STEREO_PCM_PAIR\n"+
            "No AAudio fallback. USB descriptor export required.\n";
      report.append(pair.summary());
      // snd-usb-audio typically owns the AudioControl interface (IF0) and
      // can coordinate streaming alternates. Claim it as well as AS IFs to
      // prevent a second owner from reconfiguring playback mid-transfer.
      control=findAudioControl(device,c.getRawDescriptors(),
          pair.outputInterface.getId(),pair.inputInterface.getId());
      if(control!=null && control.getId()!=pair.outputInterface.getId() &&
          control.getId()!=pair.inputInterface.getId()){
        report.append("UAC_CONTROL_IF=").append(control.getId()).append('\n');
        if(!c.claimInterface(control,true))
          return report.append("USB_DIRECT_ERROR=CLAIM_UAC_CONTROL_FAILED\n").toString();
        claimControl=true;
        report.append("UAC_CONTROL_CLAIMED=true\n");
      }else{
        return report.append(
            "USB_DIRECT_ERROR=NO_VERIFIED_AUDIO_CONTROL_FOR_IN_OUT\n").toString();
      }
      // Configure USB playback before enabling capture on its separate interface.
      report.append("DIRECT_INIT_ORDER=OUT_FIRST\n");
      if(!c.claimInterface(pair.outputInterface,true))
        return report.append("USB_DIRECT_ERROR=CLAIM_OUTPUT_FAILED\n").toString();
      claimOutput=true;
      if(!c.setInterface(pair.outputInterface))
        return report.append("USB_DIRECT_ERROR=SET_OUTPUT_ALT_FAILED\n").toString();
      if(!set48kWhenSupported(c,pair.output,pair.outputRateControl,report))
        return report.append("USB_DIRECT_ERROR=OUTPUT_SAMPLE_RATE_SET_FAILED\n").toString();
      if(!c.claimInterface(pair.inputInterface,true))
        return report.append("USB_DIRECT_ERROR=CLAIM_INPUT_FAILED\n").toString();
      claimInput=true;
      if(!c.setInterface(pair.inputInterface))
        return report.append("USB_DIRECT_ERROR=SET_INPUT_ALT_FAILED\n").toString();
      if(!set48kWhenSupported(c,pair.input,pair.inputRateControl,report))
        return report.append("USB_DIRECT_ERROR=INPUT_SAMPLE_RATE_SET_FAILED\n").toString();
      reportInterfaceAlt(c,pair.outputInterface.getId(),"PRE_RUN_OUT",report);
      reportInterfaceAlt(c,pair.inputInterface.getId(),"PRE_RUN_IN",report);
      if(((pair.output.getAttributes()>>2)&3)==1 && pair.outputFeedbackAddress!=0)
        report.append("WARNING=ASYNC_OUTPUT_FEEDBACK_NOT_YET_IMPLEMENTED\n");
      update(dsp);
      report.append(nativeRun(c.getFileDescriptor(),
         pair.input.getAddress(),pair.output.getAddress(),
         pair.input.getMaxPacketSize(),pair.output.getMaxPacketSize(),
         pair.input.getInterval(),pair.output.getInterval(),
         pair.subframeBytes,pair.inputFormatBits,depth));
      reportInterfaceAlt(c,pair.outputInterface.getId(),"AFTER_RUN_OUT",report);
      reportInterfaceAlt(c,pair.inputInterface.getId(),"AFTER_RUN_IN",report);
    }catch(Throwable t){
      report.append("USB_DIRECT_EXCEPTION=").append(t.getClass().getSimpleName())
          .append(": ").append(t.getMessage()).append('\n');
    }finally{
      if(c!=null){
        if(claimOutput)c.releaseInterface(pair.outputInterface);
        if(claimInput)c.releaseInterface(pair.inputInterface);
        if(claimControl)c.releaseInterface(control);
        c.close();
      }
    }
    boolean stillEnumerated=false;
    for(UsbDevice d:manager.getDeviceList().values()){
      if(d.getDeviceName().equals(device.getDeviceName()) &&
          d.getVendorId()==device.getVendorId() &&
          d.getProductId()==device.getProductId()){
        stillEnumerated=true;break;
      }
    }
    report.append("TARGET_DEVICE_STILL_ENUMERATED=").append(stillEnumerated)
          .append('\n');
    report.append("ENGINE_PATH=USBFS_ONLY; NO_AAUDIO_FALLBACK\n");
    return report.toString();
  }
}
