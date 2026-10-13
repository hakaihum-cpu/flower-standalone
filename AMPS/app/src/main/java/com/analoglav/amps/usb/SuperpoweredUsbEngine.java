package com.analoglav.amps.usb;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import com.analoglav.amps.AmpDsp;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Actual Superpowered AndroidUSB transport, separate from Android AAudio.
 * Private evaluation only. Execute on an existing AMPS audio worker thread.
 */
public final class SuperpoweredUsbEngine {
  static { System.loadLibrary("amps_audio"); }
  private SuperpoweredUsbEngine(){}
  private static final AtomicLong cancelEpoch=new AtomicLong();
  private static final Object waitLock=new Object();
  private static native int nativeStart(int deviceID,int fd,byte[] raw,int buffer);
  private static native void nativeStop();
  private static native int[] nativeCounters();
  private static native void nativeControls(float[] controls,int channel,
      int cabinet,int bypass);

  public static long epoch(){return cancelEpoch.get();}
  public static void stop(){
    cancelEpoch.incrementAndGet();
    nativeStop();
    synchronized(waitLock){waitLock.notifyAll();}
  }
  public static void update(AmpDsp dsp){
    float[] controls={dsp.gain,dsp.volume,dsp.bass,dsp.treble,dsp.cut,
        dsp.master,dsp.spring,dsp.tremSpeed,dsp.tremDepth};
    nativeControls(controls,dsp.channel,dsp.cabinet,dsp.bypass?1:0);
  }
  public static boolean isRunning(){
    int[] s=nativeCounters();
    return s!=null && s.length>=3 && s[0]==1 && s[1]>0;
  }
  public static String stats(){
    int[] s=nativeCounters();
    if(s==null || s.length<6)return "SUPERPOWERED STATS UNAVAILABLE";
    return "RUN="+s[0]+" CB="+s[1]+" FR="+s[2]+
        " BAD_IO="+s[3]+" IN_PEAK_x1000="+s[4]+
        " OUT_PEAK_x1000="+s[5]+" NOT_ANALOG_RTT";
  }
  public static String execute(Context ctx,UsbDevice device,int bufferIndex,
      AmpDsp dsp,long expectedEpoch){
    if(device==null)return "SUPERPOWERED_ERROR=NO_DEVICE\n";
    UsbManager manager=(UsbManager)ctx.getSystemService(Context.USB_SERVICE);
    if(manager==null)return "SUPERPOWERED_ERROR=NO_USB_MANAGER\n";
    if(!manager.hasPermission(device))
      return "SUPERPOWERED_ERROR=PERMISSION_DENIED\n";
    UsbDeviceConnection conn=null;
    try{
      if(expectedEpoch!=cancelEpoch.get())
        return "SUPERPOWERED_ERROR=CANCELLED_BEFORE_START\n";
      conn=manager.openDevice(device);
      if(conn==null)return "SUPERPOWERED_ERROR=OPEN_DEVICE_FAILED\n";
      byte[] raw=conn.getRawDescriptors();
      if(raw==null||raw.length<18)
        return "SUPERPOWERED_ERROR=RAW_DESCRIPTOR_MISSING\n";
      if(expectedEpoch!=cancelEpoch.get())
        return "SUPERPOWERED_ERROR=CANCELLED_BEFORE_NATIVE\n";
      update(dsp);
      int code=nativeStart(device.getDeviceId(),conn.getFileDescriptor(),raw,
          Math.max(0,Math.min(2,bufferIndex)));
      if(code!=0)
        return "SUPERPOWERED_ERROR=NATIVE_START_"+code+"\n";
      // The USB library now invokes its own real audio callback.
      // Wait for user-selected mode change, pause or device/engine stop.
      synchronized(waitLock){
        while(expectedEpoch==cancelEpoch.get()){
          int[] status=nativeCounters();
          if(status==null || status.length==0 || status[0]!=1)break;
          try{waitLock.wait(120);}
          catch(InterruptedException error){
            Thread.currentThread().interrupt();
            return "SUPERPOWERED_ERROR=INTERRUPTED\n";
          }
        }
      }
      return "SUPERPOWERED_STATS="+stats()+"\n"+
          "ENGINE=SUPERPOWERED_ANDROID_USB_DIRECT\n"+
          "ANALOG_ROUND_TRIP_MS=NOT_MEASURED\n";
    }catch(Throwable error){
      return "SUPERPOWERED_ERROR="+error.getClass().getSimpleName()+
          ": "+error.getMessage()+"\n";
    }finally{
      nativeStop();
      if(conn!=null)conn.close();
    }
  }
}
