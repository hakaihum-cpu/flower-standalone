package com.analoglav.amps;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;

/** Live mono-guitar-in / stereo-out audio path; actual routes are checked. */
public final class AudioBridge {
  private static final int RATE=48000, FRAMES=256;
  public static final float[] BURSTS={0,.5f,.75f,1f,1.5f,2f,3f,4f,5f,6f,7f,8f};
  public static final String[] BURST_LABELS={
      "AUTO","0.5","0.75","1","1.5","2","3","4","5","6","7","8"};
  private final Context ctx;
  private final AudioManager manager;
  public final AmpDsp dsp=new AmpDsp();
  public volatile int inputDevice=-1,outputDevice=-1,burstIndex=0;
  public volatile String status="NOT STARTED", actualIn="--", actualOut="--";
  public volatile int actualInId=-1,actualOutId=-1,outputFrames=0,inputFrames=0,xruns=0,burstFrames=192;
  public volatile float inputPeak=0,outputPeak=0;
  private volatile boolean running;
  private Thread worker;
  private volatile AudioRecord recorder;
  private volatile AudioTrack player;
  public AudioBridge(Context c){
    ctx=c;manager=(AudioManager)c.getSystemService(Context.AUDIO_SERVICE);
    String prop=manager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
    try{burstFrames=Integer.parseInt(prop);}catch(Exception ignored){}
    if(burstFrames<32 || burstFrames>2048)burstFrames=192;
  }
  private AudioDeviceInfo find(int id,boolean source){
    int what=source?AudioManager.GET_DEVICES_INPUTS:AudioManager.GET_DEVICES_OUTPUTS;
    for(AudioDeviceInfo d:manager.getDevices(what))if(d.getId()==id)return d;
    return null;
  }
  private static String shortName(AudioDeviceInfo d){
    if(d==null)return "--";
    String name=String.valueOf(d.getProductName());
    return name+" ["+d.getId()+"]";
  }
  public synchronized void stop(){
    running=false;
    AudioRecord r=recorder;AudioTrack t=player;
    if(r!=null)try{r.stop();}catch(Exception ignored){}
    if(t!=null)try{t.pause();}catch(Exception ignored){}
    Thread w=worker;
    if(w!=null&&w!=Thread.currentThread()){
      w.interrupt();
      try{w.join(900);}catch(InterruptedException e){Thread.currentThread().interrupt();}
    }
    if(worker==w)worker=null;
  }
  public synchronized void start(){
    stop();
    if(ctx.checkSelfPermission(Manifest.permission.RECORD_AUDIO)!=PackageManager.PERMISSION_GRANTED){
      status="RECORD_AUDIO PERMISSION REQUIRED";return;
    }
    running=true;status="OPENING AUDIO...";
    Thread next=new Thread(this::runAudio,"AMPS-audio");
    next.setPriority(Thread.MAX_PRIORITY);
    worker=next;next.start();
  }
  @SuppressLint("MissingPermission")
  private void runAudio(){
    AudioRecord r=null;AudioTrack t=null;
    try{
      AudioDeviceInfo requestedInput=inputDevice<0?null:find(inputDevice,true);
      AudioDeviceInfo requestedOutput=outputDevice<0?null:find(outputDevice,false);
      if(inputDevice>=0&&requestedInput==null)throw new IllegalStateException("INPUT DEVICE DISCONNECTED");
      if(outputDevice>=0&&requestedOutput==null)throw new IllegalStateException("OUTPUT DEVICE DISCONNECTED");
      int formatIn=AudioFormat.CHANNEL_IN_MONO;
      int formatOut=AudioFormat.CHANNEL_OUT_STEREO;
      int minIn=AudioRecord.getMinBufferSize(RATE,formatIn,AudioFormat.ENCODING_PCM_16BIT);
      int minOut=AudioTrack.getMinBufferSize(RATE,formatOut,AudioFormat.ENCODING_PCM_16BIT);
      if(minIn<=0||minOut<=0)throw new IllegalStateException("48K PCM NOT SUPPORTED");
      // Input >= platform minimum. Output buffer chosen in burst units, clamped to hardware minimum.
      int wanted=burstIndex==0?minOut/4:
          Math.round(burstFrames*BURSTS[Math.max(1,Math.min(burstIndex,BURSTS.length-1))]);
      int targetOut=Math.max(Math.max(minOut/4,FRAMES),wanted);
      int inBytes=Math.max(minIn,FRAMES*2*4);
      int outBytes=Math.max(minOut,targetOut*4);
      AudioFormat inFormat=new AudioFormat.Builder()
          .setSampleRate(RATE).setEncoding(AudioFormat.ENCODING_PCM_16BIT)
          .setChannelMask(formatIn).build();
      AudioFormat outFormat=new AudioFormat.Builder()
          .setSampleRate(RATE).setEncoding(AudioFormat.ENCODING_PCM_16BIT)
          .setChannelMask(formatOut).build();
      // Request unprocessed guitar input where supported. If builder throws,
      // explicitly retry with standard MIC source rather than failing open.
      try{
        r=new AudioRecord.Builder()
            .setAudioSource(MediaRecorder.AudioSource.UNPROCESSED)
            .setAudioFormat(inFormat).setBufferSizeInBytes(inBytes).build();
      }catch(RuntimeException unsupported){r=null;}
      if(r==null||r.getState()!=AudioRecord.STATE_INITIALIZED){
        if(r!=null)r.release();
        r=new AudioRecord.Builder().setAudioSource(MediaRecorder.AudioSource.MIC)
            .setAudioFormat(inFormat).setBufferSizeInBytes(inBytes).build();
      }
      if(r.getState()!=AudioRecord.STATE_INITIALIZED)throw new IllegalStateException("AUDIO IN FAILED");
      if(requestedInput!=null&&!r.setPreferredDevice(requestedInput))
        throw new IllegalStateException("CANNOT SELECT INPUT");
      t=new AudioTrack.Builder().setAudioAttributes(new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
          .setAudioFormat(outFormat).setTransferMode(AudioTrack.MODE_STREAM)
          .setBufferSizeInBytes(outBytes)
          .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY).build();
      if(t.getState()!=AudioTrack.STATE_INITIALIZED)throw new IllegalStateException("AUDIO OUT FAILED");
      if(requestedOutput!=null&&!t.setPreferredDevice(requestedOutput))
        throw new IllegalStateException("CANNOT SELECT OUTPUT");
      if(burstIndex!=0)t.setBufferSizeInFrames(targetOut);
      inputFrames=r.getBufferSizeInFrames();
      outputFrames=t.getBufferSizeInFrames();
      dsp.reset();
      recorder=r;player=t;
      r.startRecording();t.play();
      if(r.getRecordingState()!=AudioRecord.RECORDSTATE_RECORDING||
          t.getPlayState()!=AudioTrack.PLAYSTATE_PLAYING)
        throw new IllegalStateException("AUDIO STREAM NOT RUNNING");
      status="STREAM STARTED / VERIFY ROUTE";
      short[] input=new short[FRAMES];
      short[] output=new short[FRAMES*2];
      float[] processed=new float[2];
      int readBlocks=0;
      while(running&&!Thread.currentThread().isInterrupted()){
        int n=r.read(input,0,input.length,AudioRecord.READ_BLOCKING);
        if(n<=0)throw new IllegalStateException("READ ERROR "+n);
        float pi=0,po=0;
        for(int i=0;i<n;i++){
          float sample=input[i]*(1f/32768f);
          pi=Math.max(pi,Math.abs(sample));
          dsp.process(sample,processed);
          float left=processed[0],right=processed[1];
          po=Math.max(po,Math.max(Math.abs(left),Math.abs(right)));
          output[2*i]=(short)(Math.max(-1,Math.min(1,left))*32767f);
          output[2*i+1]=(short)(Math.max(-1,Math.min(1,right))*32767f);
        }
        int sent=0,need=n*2;
        while(sent<need&&running){
          int k=t.write(output,sent,need-sent,AudioTrack.WRITE_BLOCKING);
          if(k<=0)throw new IllegalStateException("WRITE ERROR "+k);
          sent+=k;
        }
        inputPeak=inputPeak*.78f+pi*.22f;
        outputPeak=outputPeak*.78f+po*.22f;
        if(++readBlocks%8==0){
          xruns=t.getUnderrunCount();
          AudioDeviceInfo actI=r.getRoutedDevice(),actO=t.getRoutedDevice();
          actualIn=shortName(actI);actualOut=shortName(actO);
          actualInId=actI==null?-1:actI.getId();
          actualOutId=actO==null?-1:actO.getId();
          if(readBlocks>24){
            if(inputDevice>=0&&actualInId!=inputDevice)
              throw new IllegalStateException("INPUT ROUTED TO WRONG DEVICE");
            if(outputDevice>=0&&actualOutId!=outputDevice)
              throw new IllegalStateException("OUTPUT ROUTED TO WRONG DEVICE");
            status=actI!=null&&actO!=null?"AUDIO ACTIVE":"ROUTE UNVERIFIED";
          }
        }
      }
      if(status.equals("AUDIO ACTIVE"))status="STOPPED";
    }catch(SecurityException e){status="MIC PERMISSION DENIED";}
    catch(Exception e){status="AUDIO ERROR: "+e.getMessage();}
    finally{
      running=false;
      recorder=null;player=null;
      if(r!=null){try{r.stop();}catch(Exception ignored){}r.release();}
      if(t!=null){try{t.pause();t.flush();t.stop();}catch(Exception ignored){}t.release();}
    }
  }
}
