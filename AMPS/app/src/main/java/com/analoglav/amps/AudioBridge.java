package com.analoglav.amps;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import java.util.Locale;

/**
 * Native AAudio duplex routing. There is no Java read/write loop and no
 * hard-coded 256-frame buffer floor. Requested versus ACTUAL is reported.
 * AAudio may round or refuse sub-burst requests on particular USB hardware.
 */
public final class AudioBridge {
  static { System.loadLibrary("amps_audio"); }
  public static final float[] BURSTS={0,.5f,.75f,1f,1.5f,2f,3f,4f,5f,6f,7f,8f};
  public static final String[] BURST_LABELS={
      "AUTO","0.5","0.75","1","1.5","2","3","4","5","6","7","8"};
  private native int nativeStart(int inputId,int outputId,int burstSelection);
  private native void nativeStop();
  private native int[] nativeStats();
  private native void nativeControls(float[] controlValues,int channel,int cabinet,int bypass);
  private final Context context;
  private final AudioManager manager;
  public final AmpDsp dsp=new AmpDsp(); // UI/preset parameters, native DSP owns audio.
  public volatile int inputDevice=-1,outputDevice=-1,burstIndex=0;
  public volatile String status="NOT STARTED",actualIn="--",actualOut="--";
  public volatile int actualInId=-1,actualOutId=-1,outputFrames=0,inputFrames=0,
      xruns=0,burstFrames=0,outputCapacity=0,inputCapacity=0,
      sampleRate=0,ringDepth=0,ringDrops=0,ringStarvation=0,
      inputMode=0,outputMode=0,requestFrames=0,outputCbMaxUs=0,
      outputCbMaxFrames=0,inputCbMaxFrames=0,clockCorrectionPpm=0;
  public volatile float inputPeak=0,outputPeak=0;
  private boolean started;
  public AudioBridge(Context c){
    context=c;manager=(AudioManager)c.getSystemService(Context.AUDIO_SERVICE);
  }
  // Match spelling variations RG Rotate / RG-Rotate / RG_Rotate / RGRotate.
  // Do not exclude USB interfaces simply because connected to that tablet.
  public static boolean excluded(AudioDeviceInfo d){
    if(d==null)return false;
    String name=String.valueOf(d.getProductName()).toLowerCase(Locale.ROOT);
    return name.replaceAll("[^a-z0-9]","").contains("rgrotate");
  }
  public AudioDeviceInfo[] visibleDevices(boolean input){
    AudioDeviceInfo[] all=manager.getDevices(input?
        AudioManager.GET_DEVICES_INPUTS:AudioManager.GET_DEVICES_OUTPUTS);
    java.util.ArrayList<AudioDeviceInfo> shown=new java.util.ArrayList<>();
    for(AudioDeviceInfo d:all)if(!excluded(d))shown.add(d);
    return shown.toArray(new AudioDeviceInfo[0]);
  }
  private AudioDeviceInfo find(int id,boolean input){
    for(AudioDeviceInfo d:visibleDevices(input))if(d.getId()==id)return d;
    return null;
  }
  private static String shortName(AudioDeviceInfo d){
    return d==null?"NOT IDENTIFIED":String.valueOf(d.getProductName())+
        " [ID "+d.getId()+"]";
  }
  public void updateControls(){
    float[] vals={dsp.gain,dsp.volume,dsp.bass,dsp.treble,dsp.cut,
        dsp.master,dsp.spring,dsp.tremSpeed,dsp.tremDepth};
    nativeControls(vals,dsp.channel,dsp.cabinet,dsp.bypass?1:0);
  }
  public synchronized void stop(){
    nativeStop();
    started=false;
    status="STOPPED";
  }
  public synchronized void start(){
    nativeStop();
    started=false;
    if(context.checkSelfPermission(Manifest.permission.RECORD_AUDIO)!=
        PackageManager.PERMISSION_GRANTED){
      status="RECORD_AUDIO PERMISSION REQUIRED";
      return;
    }
    if(inputDevice>=0&&find(inputDevice,true)==null){
      status="SELECTED INPUT REMOVED / RESELECT";
      return;
    }
    if(outputDevice>=0&&find(outputDevice,false)==null){
      status="SELECTED OUTPUT REMOVED / RESELECT";
      return;
    }
    updateControls();
    int result=nativeStart(inputDevice,outputDevice,burstIndex);
    if(result!=0){
      status="AAUDIO OPEN FAILED "+result;
      return;
    }
    started=true;
    status="AAUDIO STREAM STARTED / ROUTE VERIFY";
    poll();
  }
  /** Poll on the UI, never on an audio callback. */
  public void poll(){
    int[] s=nativeStats();
    if(s==null||s.length<28)return;
    inputFrames=s[3];outputFrames=s[4];burstFrames=s[5];sampleRate=s[6];
    xruns=s[7];outputCapacity=s[9];inputCapacity=s[10];
    outputMode=s[11];inputMode=s[12];
    ringDrops=s[13];ringStarvation=s[14];ringDepth=s[15];
    outputCbMaxUs=s[24];outputCbMaxFrames=s[25];inputCbMaxFrames=s[26];
    clockCorrectionPpm=s[27];
    actualInId=s[1];actualOutId=s[2];
    inputPeak=s[19]/1000f;outputPeak=s[20]/1000f;
    actualIn=shortName(find(actualInId,true));
    actualOut=shortName(find(actualOutId,false));
    // Actual device may be the builtin tablet when SYSTEM DEFAULT is used:
    // for diagnostics, show what really routed, even if it is excluded from
    // the picker. That is not an additional selectable device.
    if(actualInId>0&&!hasVisibleDevice(actualInId,true))
      actualIn="SYSTEM DEFAULT / ID "+actualInId;
    if(actualOutId>0&&!hasVisibleDevice(actualOutId,false))
      actualOut="SYSTEM DEFAULT / ID "+actualOutId;
    int ratioIndex=Math.max(0,Math.min(burstIndex,BURSTS.length-1));
    requestFrames=burstFrames>0?Math.max(1,Math.round(burstFrames*
        (ratioIndex==0?2.0f:BURSTS[ratioIndex]))):0;
    if(s[0]==2){
      started=false;
      status="AAUDIO ERROR "+s[16]+" / RESTART";
    }else if(s[0]==1){
      if(inputDevice>=0 && actualInId>0 && actualInId!=inputDevice){
        status="INPUT ROUTE MISMATCH";
      }else if(outputDevice>=0 && actualOutId>0 && actualOutId!=outputDevice){
        status="OUTPUT ROUTE MISMATCH";
      }else if(actualInId<=0||actualOutId<=0){
        status="ROUTE UNVERIFIED";
      }else{
        status="AAUDIO ACTIVE / "+(outputMode==0?"EXCLUSIVE":"SHARED")
            +" / PERF "+s[23];
      }
    }else if(!started && !status.startsWith("AAUDIO OPEN FAILED") &&
             !status.contains("RESELECT") && !status.contains("PERMISSION")){
      status="STOPPED";
    }
  }
  private boolean hasVisibleDevice(int id,boolean input){
    return find(id,input)!=null;
  }
}
