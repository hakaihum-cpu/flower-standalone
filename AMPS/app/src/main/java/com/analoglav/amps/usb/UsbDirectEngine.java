package com.analoglav.amps.usb;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
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

  public static String execute(Context ctx,UsbDevice device,int depth,AmpDsp dsp) {
    if(device==null)return "USB_DIRECT_ERROR=NO_DEVICE\n";
    UsbManager manager=(UsbManager)ctx.getSystemService(Context.USB_SERVICE);
    if(manager==null)return "USB_DIRECT_ERROR=NO_MANAGER\n";
    if(!manager.hasPermission(device))return "USB_DIRECT_ERROR=PERMISSION_DENIED\n";
    UsbDeviceConnection c=null;
    UsbDirectFormat.Pair pair=null;
    boolean claimInput=false,claimOutput=false;
    StringBuilder report=new StringBuilder();
    try{
      c=manager.openDevice(device);
      if(c==null)return "USB_DIRECT_ERROR=OPEN_DEVICE_FAILED\n";
      pair=UsbDirectFormat.find(device,c.getRawDescriptors());
      if(pair==null)
        return "USB_DIRECT_ERROR=NO_VERIFIED_UAC1_48K_STEREO_PCM_PAIR\n"+
            "No AAudio fallback. USB descriptor export required.\n";
      report.append(pair.summary());
      if(!c.claimInterface(pair.inputInterface,true))
        return report.append("USB_DIRECT_ERROR=CLAIM_INPUT_FAILED\n").toString();
      claimInput=true;
      if(!c.claimInterface(pair.outputInterface,true))
        return report.append("USB_DIRECT_ERROR=CLAIM_OUTPUT_FAILED\n").toString();
      claimOutput=true;
      if(!c.setInterface(pair.inputInterface))
        return report.append("USB_DIRECT_ERROR=SET_INPUT_ALT_FAILED\n").toString();
      if(!c.setInterface(pair.outputInterface))
        return report.append("USB_DIRECT_ERROR=SET_OUTPUT_ALT_FAILED\n").toString();
      update(dsp);
      report.append(nativeRun(c.getFileDescriptor(),
         pair.input.getAddress(),pair.output.getAddress(),
         pair.input.getMaxPacketSize(),pair.output.getMaxPacketSize(),
         pair.input.getInterval(),pair.output.getInterval(),
         pair.subframeBytes,pair.inputFormatBits,depth));
    }catch(Throwable t){
      report.append("USB_DIRECT_EXCEPTION=").append(t.getClass().getSimpleName())
          .append(": ").append(t.getMessage()).append('\n');
    }finally{
      if(c!=null){
        if(claimOutput)c.releaseInterface(pair.outputInterface);
        if(claimInput)c.releaseInterface(pair.inputInterface);
        c.close();
      }
    }
    report.append("ENGINE_PATH=USBFS_ONLY; NO_AAUDIO_FALLBACK\n");
    return report.toString();
  }
}
