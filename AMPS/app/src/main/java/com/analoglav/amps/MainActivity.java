package com.analoglav.amps;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.net.Uri;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Build;
import com.analoglav.amps.usb.UsbAudioDeviceProbe;
import com.analoglav.amps.usb.UsbIsoTester;
import com.analoglav.amps.usb.UsbDirectEngine;
import com.analoglav.amps.usb.UsbDiagnosticStore;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Original amber-dot UI, designed at logical 720x720 and rescaled with touch. */
public final class MainActivity extends Activity {
  private static final int PERMISSION=42;
  private static final int SAVE_DIAGNOSTIC_TXT=107;
  private static final String USB_PERMISSION="com.analoglav.amps.USB_DIAG_PERMISSION";
  private static final int GOLD=Color.rgb(255,185,46), DIM=Color.rgb(133,88,25),
      LIT=Color.rgb(255,211,96), DARK=Color.rgb(10,10,8), CASE=Color.rgb(28,23,15);
  private static final String[] KNOB_NAMES={
      "GAIN","VOLUME","BASS","TREBLE","CUT","MASTER","REVERB","TREM SPEED","TREM DEPTH"};
  private static final String[] PRESET_NAMES={
      "Hallway Chime","Dusty Drive","Empty Classroom","After School","Late Night Practice"};
  private static final float[][] FACTORY={
      {.38f,.65f,.52f,.7f,.22f,.7f,.20f,.3f,.0f,1,0},
      {.68f,.63f,.57f,.47f,.56f,.60f,.12f,.32f,.1f,1,1},
      {.3f,.65f,.66f,.58f,.32f,.65f,.6f,.16f,.0f,0,0},
      {.52f,.63f,.5f,.6f,.42f,.67f,.23f,.53f,.63f,1,0},
      {.74f,.56f,.4f,.48f,.58f,.52f,.38f,.12f,.25f,0,1}
  };
  private AudioManager audioManager;
  private AudioBridge bridge;
  private Screen screen;
  private SharedPreferences prefs;
  private int preset=0;
  private boolean config=false,tuningPage=false;
  private final TuningProfile tuning=new TuningProfile();
  private String usbReport="USB NOT PROBED; DIRECT USB ENGINE NOT IMPLEMENTED";
  private String usbStatus="USB SCAN: NOT RUN";
  private String pendingDiagnosticText;
  private UsbDevice selectedUsb;
  private boolean directIsoRunning=false;
  private boolean ranDirectIso=false;
  private int usbNextAction=1; // 1=descriptor, 2=usbfs ISO transfer test
  private int isoQueueDepth=3;
  private boolean usbDirectEnabled=false;
  private boolean appForeground=false;
  private String installedVersion="UNKNOWN";
  private volatile boolean directWorkerActive=false;
  private int directGeneration=0;
  private boolean directPermissionPending=false;

  private int grabbed=-1;
  private float startY,startValue;
  private long downAt;
  private float downX,downY;
  private final BroadcastReceiver usbPermissionReceiver=new BroadcastReceiver(){
    @Override public void onReceive(Context context,Intent intent){
      if(!USB_PERMISSION.equals(intent.getAction()))return;
      UsbManager m=(UsbManager)getSystemService(Context.USB_SERVICE);
      if(selectedUsb!=null&&m!=null&&m.hasPermission(selectedUsb)){
        directPermissionPending=false;
        if(usbNextAction==3)startDirectSelected();
        else if(usbNextAction==2)runUsbIsoTest();
        else probeSelectedUsb();
      }
      else {
        directPermissionPending=false;
        usbReport="USB PERMISSION DENIED / NO DESCRIPTORS READ";
        usbStatus=usbDirectEnabled?"USB DIRECT: PERMISSION DENIED":"USB SCAN: PERMISSION DENIED";
        showUsbReport();
      }
      if(screen!=null)screen.postInvalidate();
    }
  };
  private final AudioDeviceCallback deviceCallback=new AudioDeviceCallback(){
    @Override public void onAudioDevicesAdded(AudioDeviceInfo[] added){
      if(screen!=null)screen.postInvalidate();
    }
    @Override public void onAudioDevicesRemoved(AudioDeviceInfo[] removed){
      boolean lost=false;
      for(AudioDeviceInfo d:removed)if(d.getId()==bridge.inputDevice||
          d.getId()==bridge.outputDevice)lost=true;
      if(lost){bridge.stop();bridge.status="SELECTED AUDIO DEVICE REMOVED";}
      if(screen!=null)screen.postInvalidate();
    }
  };
  @Override public void onCreate(Bundle state){
    super.onCreate(state);
    try {
      installedVersion=getPackageManager().getPackageInfo(getPackageName(),0).versionName;
    }catch(PackageManager.NameNotFoundException e) {
      installedVersion="UNKNOWN";
    }
    getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
        WindowManager.LayoutParams.FLAG_FULLSCREEN);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    prefs=getPreferences(MODE_PRIVATE);
    audioManager=(AudioManager)getSystemService(Context.AUDIO_SERVICE);
    bridge=new AudioBridge(this);
    bridge.inputDevice=restoreEndpoint(true);
    bridge.outputDevice=restoreEndpoint(false);
    bridge.burstIndex=Math.max(0,Math.min(AudioBridge.BURSTS.length-1,prefs.getInt("burst",0)));
    for(int i=0;i<TuningProfile.OPTIONS.length;i++)
      tuning.setValue(i,prefs.getInt("tune-"+i,0));
    bridge.setTuning(tuning);
    isoQueueDepth=Math.max(1,Math.min(4,prefs.getInt("usb-iso-depth",3)));
    usbDirectEnabled=prefs.getBoolean("usb-direct-enabled",false);
    IntentFilter usbFilter=new IntentFilter(USB_PERMISSION);
    if(Build.VERSION.SDK_INT>=33)
      registerReceiver(usbPermissionReceiver,usbFilter,Context.RECEIVER_NOT_EXPORTED);
    else registerReceiver(usbPermissionReceiver,usbFilter);
    screen=new Screen();setContentView(screen);
    audioManager.registerAudioDeviceCallback(deviceCallback,null);
    preset=Math.max(0,Math.min(4,prefs.getInt("preset",0)));
    loadPreset(preset);
    if(checkSelfPermission(Manifest.permission.RECORD_AUDIO)!=PackageManager.PERMISSION_GRANTED)
      requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO},PERMISSION);
    // onResume opens the stream once; no duplicate audio restart on launch.
  }
  @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] results){
    super.onRequestPermissionsResult(request,permissions,results);
    if(request==PERMISSION&&results.length>0&&results[0]==PackageManager.PERMISSION_GRANTED){
      if(usbDirectEnabled)startDirectIfPossible();
      else bridge.start();
    }
    else if(request==PERMISSION)bridge.status="MIC PERMISSION NEEDED - CONFIG";
    screen.invalidate();
  }
  @Override protected void onPause(){
    appForeground=false;
    stopDirectWorker();bridge.stop();super.onPause();
  }
  @Override protected void onResume(){
    super.onResume();
    appForeground=true;
    if(bridge==null)return;
    if(usbDirectEnabled)startDirectIfPossible();
    else if(!directIsoRunning && !ranDirectIso &&
       checkSelfPermission(Manifest.permission.RECORD_AUDIO)==PackageManager.PERMISSION_GRANTED)
      bridge.start();
  }
  @Override protected void onDestroy(){
    stopDirectWorker();
    bridge.stop();
    unregisterReceiver(usbPermissionReceiver);
    audioManager.unregisterAudioDeviceCallback(deviceCallback);
    super.onDestroy();
  }
  private AudioDeviceInfo[] endpoints(boolean input){
    return bridge.visibleDevices(input);
  }
  private static String labelDevice(AudioDeviceInfo d){
    if(d==null)return "DEFAULT";
    return String.valueOf(d.getProductName())+"  ID "+d.getId();
  }
  private AudioDeviceInfo selectedDevice(boolean input){
    int id=input?bridge.inputDevice:bridge.outputDevice;
    for(AudioDeviceInfo d:endpoints(input))if(d.getId()==id)return d;
    return null;
  }
  private int restoreEndpoint(boolean input){
    String value=prefs.getString(input?"in-token":"out-token","");
    if(value.isEmpty())return -1;
    for(AudioDeviceInfo d:endpoints(input)){
      String key=d.getType()+"|"+d.getProductName();
      if(key.equals(value))return d.getId();
    }
    return -1;
  }
  private void chooseDevice(boolean input){
    ArrayList<String> choices=new ArrayList<>();
    choices.add("SYSTEM DEFAULT");
    AudioDeviceInfo[] devices=endpoints(input);
    for(AudioDeviceInfo d:devices)choices.add(labelDevice(d));
    new AlertDialog.Builder(this)
        .setTitle(input?"AUDIO INPUT":"AUDIO OUTPUT")
        .setItems(choices.toArray(new String[0]),(dialog,position)->{
          AudioDeviceInfo dev=position==0?null:devices[position-1];
          int id=dev==null?-1:dev.getId();
          String key=dev==null?"":dev.getType()+"|"+dev.getProductName();
          prefs.edit().putString(input?"in-token":"out-token",key).apply();
          if(input)bridge.inputDevice=id;else bridge.outputDevice=id;
          restartBaselineAudio();screen.invalidate();
        }).setNegativeButton("CLOSE",null).show();
  }
  private void chooseBurst(){
    new AlertDialog.Builder(this).setTitle("AUDIO BUFFER / BURSTS")
      .setSingleChoiceItems(AudioBridge.BURST_LABELS,bridge.burstIndex,(d,index)->{
        bridge.burstIndex=index;prefs.edit().putInt("burst",index).apply();
        d.dismiss();restartBaselineAudio();screen.invalidate();
      }).setNegativeButton("CLOSE",null).show();
  }
  private void selectTuneValue(int row){
    final int draftIndex=tuning.index(row);
    String[] choices=new String[TuningProfile.OPTIONS[row].length];
    for(int j=0;j<choices.length;j++){
      tuning.setIndex(row,j);
      choices[j]=tuning.label(row);
    }
    int selected=draftIndex;
    tuning.setIndex(row,selected);
    new AlertDialog.Builder(this).setTitle(TuningProfile.NAMES[row])
      .setItems(choices,(d,index)->{
        tuning.setIndex(row,index);
        screen.invalidate();
      }).setNegativeButton("CLOSE",null).show();
  }
  private void restartBaselineAudio(){
    if(usbDirectEnabled){
      usbStatus="USB DIRECT SELECTED: AAUDIO SETTINGS NOT APPLIED";
      bridge.stop();
      if(!directWorkerActive)startDirectIfPossible();
      return;
    }
    if(directIsoRunning){
      bridge.status="USBFS ISO TEST ACTIVE - WAIT";
      return;
    }
    ranDirectIso=false;
    bridge.start();
  }
  private void applyTune(){
    SharedPreferences.Editor edit=prefs.edit();
    for(int i=0;i<TuningProfile.OPTIONS.length;i++)
      edit.putInt("tune-"+i,tuning.get(i));
    edit.apply();
    bridge.setTuning(tuning);
    restartBaselineAudio();
    screen.invalidate();
  }
  private void resetTune(){
    tuning.reset();
    applyTune();
  }
  private void stopDirectWorker(){
    directGeneration++;
    directPermissionPending=false;
    UsbDirectEngine.stop();
  }
  private void chooseAudioEngine(){
    usbDirectEnabled=!usbDirectEnabled;
    prefs.edit().putBoolean("usb-direct-enabled",usbDirectEnabled).apply();
    if(usbDirectEnabled){
      bridge.stop();
      if(directIsoRunning){
        usbStatus="USB DIRECT: WAIT FOR USBFS TEST";
      }else startDirectIfPossible();
    }else{
      stopDirectWorker();
      usbStatus="USB DIRECT DISABLED BY USER";
      if(!directWorkerActive)restartBaselineAudio();
      else bridge.status="WAIT USB DIRECT RELEASE BEFORE AAUDIO";
    }
    screen.invalidate();
  }
  private UsbDevice preferredDirectDevice(){
    UsbManager manager=(UsbManager)getSystemService(Context.USB_SERVICE);
    if(manager==null)return null;
    java.util.ArrayList<UsbDevice> choices=new java.util.ArrayList<>(manager.getDeviceList().values());
    choices.sort((a,b)->a.getDeviceName().compareTo(b.getDeviceName()));
    UsbDevice fallback=null;int matches=0;
    for(UsbDevice d:choices) {
      String name=String.valueOf(d.getProductName()).toLowerCase(Locale.ROOT);
      if(name.contains("irig") || name.contains("pro duo"))return d;
      boolean audio=false;
      for(int i=0;i<d.getInterfaceCount();i++){
        android.hardware.usb.UsbInterface f=d.getInterface(i);
        if(f.getInterfaceClass()==1 && f.getInterfaceSubclass()==2)audio=true;
      }
      if(audio){fallback=d;matches++;}
    }
    // Only fall back to an unambiguously single attached USB audio device.
    return matches==1?fallback:null;
  }
  private void startDirectIfPossible(){
    if(!usbDirectEnabled||directWorkerActive||directPermissionPending||directIsoRunning)return;
    selectedUsb=preferredDirectDevice();
    if(selectedUsb==null){
      usbStatus="USB DIRECT: NO IRIG PRO DUO USB DEVICE FOUND";
      usbReport="Connect iRig PRO DUO; no AAudio fallback.";
      bridge.stop();
      if(screen!=null)screen.invalidate();
      return;
    }
    UsbManager manager=(UsbManager)getSystemService(Context.USB_SERVICE);
    if(manager==null){
      usbStatus="USB DIRECT: USB MANAGER MISSING";
      return;
    }
    if(!manager.hasPermission(selectedUsb)){
      directPermissionPending=true;
      usbNextAction=3;
      usbStatus="USB DIRECT: REQUESTING USB PERMISSION";
      Intent intent=new Intent(USB_PERMISSION).setPackage(getPackageName());
      PendingIntent action=PendingIntent.getBroadcast(this,79,intent,
          PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_MUTABLE);
      manager.requestPermission(selectedUsb,action);
      return;
    }
    startDirectSelected();
  }
  private void startDirectSelected(){
    if(!appForeground||!usbDirectEnabled||directWorkerActive||selectedUsb==null)return;
    bridge.stop();
    directWorkerActive=true;
    final int generation=++directGeneration;
    final UsbDevice device=selectedUsb;
    usbStatus="USB HOST: STARTING LIBUSB ASYNC ISO";
    usbReport="USB_DIRECT_START_REQUESTED\n"+UsbAudioDeviceProbe.inspect(this,device);
    try{
      UsbDiagnosticStore.save(this,diagnosticReport());
    }catch(IOException e){
      usbStatus="USB DIRECT: SNAPSHOT SAVE FAILED "+e.getMessage();
    }
    screen.invalidate();
    new Thread(()->{
      String result;
      try{
        result=UsbDirectEngine.execute(this,device,isoQueueDepth,bridge.dsp);
      }catch(Throwable t){
        result="USB_DIRECT_WORKER_ERROR="+t.getClass().getSimpleName()+": "+
            t.getMessage()+"\\n";
      }
      final String done=result;
      // Preserve raw descriptors and USB endpoints even on transfer failure.
      // Failure report must contain the information needed to fix OUT errors.
      String details;
      try{details=UsbAudioDeviceProbe.inspect(this,device);}
      catch(Throwable t){details="USB_PROBE_ERROR="+t.toString();}
      final String descriptorDetails=details;
      runOnUiThread(()->{
        directWorkerActive=false;
        usbReport="TARGET_USB VID="+device.getVendorId()+
            " PID="+device.getProductId()+"\nUSB_DIRECT_RESULT\n"+
            done+"\nUSB_DESCRIPTORS\n"+descriptorDetails;
        usbStatus="USB DIRECT: "+(done.contains("USB_DIRECT_ERROR=")||
            done.contains("USB_DIRECT_EXCEPTION=")||
            done.contains("ERROR_CODE=-")?"UNAVAILABLE / SEE REPORT":"STOPPED / SEE REPORT");
        try{UsbDiagnosticStore.save(this,diagnosticReport());}
        catch(IOException e){usbStatus="USB DIRECT: REPORT CACHE ERROR "+e.getMessage();}
        if(usbDirectEnabled){
          // Never silently fall back to AAudio.
          bridge.stop();
          if(appForeground && generation!=directGeneration)
            startDirectIfPossible(); // pause/resume handoff, not error retry
        }else if(!usbDirectEnabled && !isFinishing() &&
            checkSelfPermission(Manifest.permission.RECORD_AUDIO)==PackageManager.PERMISSION_GRANTED){
          restartBaselineAudio();
        }
        if(screen!=null)screen.invalidate();
      });
    },"AMPS-USB-DIRECT-PCM").start();
  }
  private void chooseUsbQueueDepth(){
    final String[] labels={
        "1 ISO URB per direction","2 ISO URBs per direction",
        "3 ISO URBs per direction","4 ISO URBs per direction"};
    new AlertDialog.Builder(this).setTitle("USB DIRECT QUEUE DEPTH")
      .setSingleChoiceItems(labels,isoQueueDepth-1,(dialog,index)->{
        dialog.dismiss();
        if(isoQueueDepth==index+1)return;
        isoQueueDepth=index+1;
        prefs.edit().putInt("usb-iso-depth",isoQueueDepth).apply();
        if(usbDirectEnabled){
          stopDirectWorker();
          usbStatus="USB DIRECT: RESTARTING DEPTH "+isoQueueDepth;
          if(!directWorkerActive)startDirectIfPossible();
        }
        screen.invalidate();
      }).setNegativeButton("CLOSE",null).show();
  }
  private void restartSelectedAudio(){
    if(usbDirectEnabled){
      stopDirectWorker();
      bridge.stop();
      usbStatus="USB DIRECT: RESTART REQUESTED";
      if(!directWorkerActive)startDirectIfPossible();
    }else restartBaselineAudio();
    screen.invalidate();
  }
  private void selectUsbProbe(){selectUsbForAction(false);}
  private void selectUsbTest(){
    final String[] queue={
      "1 ISO URB per direction",
      "2 ISO URBs per direction",
      "3 ISO URBs per direction",
      "4 ISO URBs per direction"
    };
    new AlertDialog.Builder(this).setTitle("USBFS ISO QUEUE DEPTH")
      .setSingleChoiceItems(queue,isoQueueDepth-1,(dialog,index)->{
        isoQueueDepth=index+1;
        prefs.edit().putInt("usb-iso-depth",isoQueueDepth).apply();
        dialog.dismiss();
        selectUsbForAction(true);
      }).setNegativeButton("CLOSE",null).show();
  }
  private void selectUsbForAction(boolean direct){
    UsbManager manager=(UsbManager)getSystemService(Context.USB_SERVICE);
    if(manager==null){usbReport="USB MANAGER UNAVAILABLE";usbStatus="USB SCAN: MANAGER UNAVAILABLE";showUsbReport();screen.invalidate();return;}
    ArrayList<UsbDevice> candidates=new ArrayList<>(manager.getDeviceList().values());
    candidates.sort((a,b)->a.getDeviceName().compareTo(b.getDeviceName()));
    if(candidates.isEmpty()){usbReport="NO USB DEVICE CONNECTED";usbStatus="USB SCAN: NO CONNECTED DEVICES";showUsbReport();screen.invalidate();return;}
    String[] labels=new String[candidates.size()];
    for(int i=0;i<candidates.size();i++){
      UsbDevice d=candidates.get(i);
      labels[i]=String.valueOf(d.getProductName())+" (VID "+d.getVendorId()+
          " / PID "+d.getProductId()+")";
    }
    new AlertDialog.Builder(this).setTitle("USB AUDIO / READ-ONLY PROBE")
      .setItems(labels,(dialog,index)->{
        selectedUsb=candidates.get(index);
        usbNextAction=direct?2:1;
        if(manager.hasPermission(selectedUsb)){
          if(direct)runUsbIsoTest();else probeSelectedUsb();
        }else{
          usbReport="WAITING FOR USB PERMISSION";
          usbStatus="USB SCAN: WAITING FOR PERMISSION";
          Intent action=new Intent(USB_PERMISSION).setPackage(getPackageName());
          PendingIntent pi=PendingIntent.getBroadcast(this,77,action,
              PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_MUTABLE);
          manager.requestPermission(selectedUsb,pi);
        }
        screen.invalidate();
      }).setNegativeButton("CLOSE",null).show();
  }
  private void runUsbIsoTest(){
    if(directIsoRunning)return;
    if(usbDirectEnabled||directWorkerActive){
      usbStatus="USBFS TEST: DISABLE USB DIRECT MODE FIRST";
      showUsbReport();return;
    }
    final UsbDevice device=selectedUsb;
    if(device==null){
      usbStatus="DIRECT USB ISO: NO SELECTED DEVICE";
      showUsbReport();return;
    }
    // STOP AAudio BEFORE claiming USB audio interfaces. No AAudio read/write
    // or callbacks can execute during the usbfs direct transfer test.
    bridge.stop();
    directIsoRunning=true;
    ranDirectIso=true;
    usbStatus="DIRECT USBFS ISO: RUNNING (AAUDIO OFF)";
    usbReport="Native USBFS isochronous IN+OUT in progress. OUT is SILENCE.";
    screen.invalidate();
    new Thread(()->{
      final String description=UsbAudioDeviceProbe.inspect(this,device);
      final String outcome=UsbIsoTester.execute(this,device,350,isoQueueDepth);
      runOnUiThread(()->{
        directIsoRunning=false;
        usbStatus="DIRECT USBFS ISO: FINISHED / CHECK COUNTERS";
        usbReport=description+"\n\n"+outcome;
        bridge.status="USBFS ISO TEST COMPLETED / AAUDIO STOPPED";
        if(usbDirectEnabled)startDirectIfPossible();
        screen.invalidate();
        if(!isFinishing())showUsbReport();
      });
    },"AMPS-USBFS-ISO").start();
  }
  private void probeSelectedUsb(){
    usbReport=UsbAudioDeviceProbe.inspect(this,selectedUsb);
    boolean isoInput=false,isoOutput=false;
    if(selectedUsb!=null){
      for(int i=0;i<selectedUsb.getInterfaceCount();i++){
        android.hardware.usb.UsbInterface iface=selectedUsb.getInterface(i);
        if(iface.getInterfaceClass()!=1||iface.getInterfaceSubclass()!=2)continue;
        for(int j=0;j<iface.getEndpointCount();j++){
          android.hardware.usb.UsbEndpoint ep=iface.getEndpoint(j);
          if(ep.getType()!=1)continue;
          if(ep.getDirection()==128)isoInput=true;else isoOutput=true;
        }
      }
    }
    boolean passed=usbReport.contains("RAW DESCRIPTORS HEX:");
    usbStatus=passed?
        "USB SCAN: SUCCESS / ISO IN "+(isoInput?"YES":"NOT LISTED")+
        " / OUT "+(isoOutput?"YES":"NOT LISTED"):
        "USB SCAN: FAILED / SEE REPORT";
    screen.invalidate();
    showUsbReport();
  }
  private void showUsbReport(){
    // This is the measured/decoded USB descriptor report, not a decorative
    // READ ONLY label. Dialog content is scrollable and selectable.
    TextView content=new TextView(this);
    content.setText(usbStatus+"\n\n"+usbReport);
    content.setTextSize(12f);
    content.setTypeface(Typeface.MONOSPACE);
    content.setTextIsSelectable(true);
    content.setPadding(20,12,20,12);
    ScrollView scroll=new ScrollView(this);
    scroll.setFillViewport(false);
    scroll.addView(content);
    new AlertDialog.Builder(this)
        .setTitle("USB DIAGNOSTIC RESULT")
        .setView(scroll)
        .setPositiveButton("SAVE .TXT",(dialog,which)->saveDiagnosticTxt())
        .setNeutralButton("COPY",(dialog,which)->exportReport(false))
        .setNegativeButton("CLOSE",null)
        .show();
  }
  private void saveDiagnosticTxt(){
    String report=diagnosticReport();
    if(report==null||report.trim().isEmpty()){
      new AlertDialog.Builder(this).setMessage("Diagnostic text is empty")
          .setPositiveButton("OK",null).show();
      return;
    }
    // Durable snapshot BEFORE launching picker. Saving to private AtomicFile
    // survives Activity recreation (the cause of blank 0KB exports).
    try{
      UsbDiagnosticStore.save(this,report);
      pendingDiagnosticText=report;
    }catch(IOException e){
      new AlertDialog.Builder(this).setTitle("DIAGNOSTIC CACHE FAILURE")
          .setMessage(e.toString()).setPositiveButton("OK",null).show();
      return;
    }
    String when=new SimpleDateFormat("yyyyMMdd_HHmmss",Locale.US).format(new Date());
    Intent create=new Intent(Intent.ACTION_CREATE_DOCUMENT);
    create.addCategory(Intent.CATEGORY_OPENABLE);
    create.setType("text/plain");
    create.putExtra(Intent.EXTRA_TITLE,"AMPS_Diagnostic_"+when+".txt");
    try{
      startActivityForResult(create,SAVE_DIAGNOSTIC_TXT);
    }catch(android.content.ActivityNotFoundException e){
      Toast.makeText(this,"No text document provider installed",Toast.LENGTH_LONG).show();
    }
  }
  @Override protected void onActivityResult(int request,int result,Intent data){
    super.onActivityResult(request,result,data);
    if(request!=SAVE_DIAGNOSTIC_TXT)return;
    if(result!=RESULT_OK||data==null||data.getData()==null)return;
    Uri destination=data.getData();
    String report=pendingDiagnosticText;
    pendingDiagnosticText=null;
    try{
      if(report==null||report.isEmpty())report=UsbDiagnosticStore.load(this);
      if(report==null||report.trim().isEmpty())
        throw new IOException("Diagnostic snapshot missing; refused to write 0KB");
      byte[] bytes=report.getBytes(StandardCharsets.UTF_8);
      if(bytes.length==0)throw new IOException("Cannot write an empty report");
      try(OutputStream out=getContentResolver().openOutputStream(destination,"w")){
        if(out==null)throw new IOException("Output stream unavailable");
        out.write(bytes);
        out.flush();
      }
      usbStatus="REPORT SAVED: "+bytes.length+" BYTES";
      bridge.status="DIAGNOSTIC .TXT SAVED ("+bytes.length+" BYTES)";
      Toast.makeText(this,"AMPS diagnostic saved: "+bytes.length+" bytes",Toast.LENGTH_LONG).show();
    }catch(Exception e){
      usbStatus="TXT SAVE FAILED: "+e.getClass().getSimpleName();
      bridge.status="DIAGNOSTIC SAVE FAILED";
      new AlertDialog.Builder(this).setTitle("SAVE .TXT FAILED")
          .setMessage(e.getClass().getSimpleName()+": "+e.getMessage())
          .setPositiveButton("OK",null).show();
    }
    if(screen!=null)screen.invalidate();
  }
  private String diagnosticReport(){
    bridge.poll();
    StringBuilder s=new StringBuilder();
    s.append("AMPS AN-68 ENGINE VERIFICATION DIAGNOSTIC\n");
    s.append("INSTALLED_APK_VERSION=").append(installedVersion).append('\n');
    s.append("DATE_MS=").append(System.currentTimeMillis()).append('\n');
    s.append("ANDROID_SDK=").append(Build.VERSION.SDK_INT).append('\n');
    s.append("DEVICE=").append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append('\n');
    s.append("GOAL: <=10ms ANALOG INPUT-TO-OUTPUT; NOT MEASURED\n");
    s.append("DIRECT USBFS ISO TEST RUN="+ranDirectIso+" / BUSY="+directIsoRunning+"\n");
    s.append("DIRECT USBFS ISO QUEUE DEPTH="+isoQueueDepth+"\n");
    s.append("ENGINE_CONFIG="+(usbDirectEnabled?"LIBUSB_UAC1_HOST":"AAUDIO")+"\n");
    s.append("AAUDIO_RUNTIME_RUNNING="+bridge.aaudioRunning+"\n");
    String runtime=bridge.aaudioRunning?"AAUDIO":
        (UsbDirectEngine.isRunning()?"LIBUSB_UAC1_HOST":"NO_ACTIVE_STREAM");
    s.append("ENGINE_ACTUALLY_RUNNING="+runtime+"\n");
    s.append("USB_DIRECT_STATS="+UsbDirectEngine.stats()+"\n");
    s.append("USB_STATUS="+usbStatus+"\n");
    s.append("DIRECT TRANSFER IS NOT AUDIO LOOPBACK; RTT NOT MEASURED\n");
    if(!usbDirectEnabled){
      s.append("AAUDIO DRAFT TUNING:\n").append(tuning.report());
    s.append("APPLIED OUT=").append(bridge.tuneOutFrames)
        .append(" CALLBACK=").append(bridge.tuneCallbackFrames)
        .append(" IN=").append(bridge.tuneInputFrames)
        .append(" SHARING=").append(bridge.tuneSharingPreference).append('\n');
    s.append("BURST_INDEX=").append(bridge.burstIndex)
        .append(" REQ_FRAMES=").append(bridge.requestFrames)
        .append(" ACT_OUT_FRAMES=").append(bridge.outputFrames)
        .append(" ACT_IN_FRAMES=").append(bridge.inputFrames)
        .append(" BURST_FRAMES=").append(bridge.burstFrames).append('\n');
    s.append("RATE=").append(bridge.sampleRate)
        .append(" OUT_CB_FRAMES=").append(bridge.outputCallbackFrames)
        .append(" INPUT_BURST=").append(bridge.inputBurstFrames).append('\n');
    s.append("PERF_OUT=").append(bridge.outputPerfMode)
        .append(" SHARING_OUT=").append(bridge.outputMode)
        .append(" ROUTE_IN_ID=").append(bridge.actualInId)
        .append(" ROUTE_OUT_ID=").append(bridge.actualOutId).append('\n');
    s.append("ROUTE_IN=").append(bridge.actualIn).append('\n');
    s.append("ROUTE_OUT=").append(bridge.actualOut).append('\n');
    s.append("EST_IN_MS=").append(bridge.estimatedInputLatencyMs)
        .append(" EST_OUT_MS=").append(bridge.estimatedOutputLatencyMs)
        .append(" (NOT ANALOG RTT)\n");
    s.append("XRUN=").append(bridge.xruns)
        .append(" INPUT_MISSING_FRAMES=").append(bridge.ringStarvation)
        .append(" INPUT_READ_ERRORS=").append(bridge.inputReadErrors)
        .append(" MAX_CALLBACK_US=").append(bridge.outputCbMaxUs).append('\n');
    s.append("STATUS=").append(bridge.status).append('\n');
    }else{
      s.append("AAUDIO_BUFFER_FIELDS=NOT_APPLICABLE_TO_USB_DIRECT\n");
      s.append("USB_QUEUE_DEPTH=").append(isoQueueDepth).append('\n');
      s.append("USB_DIRECT_STATUS=").append(usbStatus).append('\n');
    }
    s.append("USB_DESCRIPTOR_PROBE:\n").append(usbReport).append('\n');
    return s.toString();
  }
  private void exportReport(boolean share){
    String report=diagnosticReport();
    if(share){
      Intent intent=new Intent(Intent.ACTION_SEND);
      intent.setType("text/plain");
      intent.putExtra(Intent.EXTRA_TEXT,report);
      startActivity(Intent.createChooser(intent,"SHARE AMPS DIAGNOSTIC"));
    }else{
      ClipboardManager clip=(ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);
      if(clip!=null)clip.setPrimaryClip(ClipData.newPlainText("AMPS Diagnostic",report));
      bridge.status="DIAGNOSTIC REPORT COPIED";
      screen.invalidate();
    }
  }
  private static float bound(float v){return Math.max(0,Math.min(1,v));}
  private float getKnob(int n){
    AmpDsp a=bridge.dsp;
    switch(n){
      case 0:return a.gain; case 1:return a.volume; case 2:return a.bass;
      case 3:return a.treble; case 4:return a.cut; case 5:return a.master;
      case 6:return a.spring; case 7:return a.tremSpeed; default:return a.tremDepth;
    }
  }
  private void setKnob(int n,float value){
    AmpDsp a=bridge.dsp;float v=bound(value);
    switch(n){
      case 0:a.gain=v;break; case 1:a.volume=v;break; case 2:a.bass=v;break;
      case 3:a.treble=v;break; case 4:a.cut=v;break; case 5:a.master=v;break;
      case 6:a.spring=v;break; case 7:a.tremSpeed=v;break; case 8:a.tremDepth=v;break;
    }
    bridge.updateControls();
    if(usbDirectEnabled)UsbDirectEngine.update(bridge.dsp);
    screen.invalidate();
  }
  private void loadPreset(int selected){
    preset=(selected+5)%5;
    String data=prefs.getString("preset-"+preset,"");
    String[] items=data.split(",");
    if(items.length==11){
      try{
        for(int i=0;i<9;i++)setKnob(i,Float.parseFloat(items[i]));
        bridge.dsp.channel=Integer.parseInt(items[9]);
        bridge.dsp.cabinet=Integer.parseInt(items[10]);
      }catch(NumberFormatException e){useFactory();}
    }else useFactory();
    prefs.edit().putInt("preset",preset).apply();
    screen.invalidate();
  }
  private void useFactory(){
    float[] values=FACTORY[preset];
    for(int i=0;i<9;i++)setKnob(i,values[i]);
    bridge.dsp.channel=(int)values[9];
    bridge.dsp.cabinet=(int)values[10];
  }
  private void savePreset(){
    StringBuilder sb=new StringBuilder();
    for(int i=0;i<9;i++)sb.append(getKnob(i)).append(',');
    sb.append(bridge.dsp.channel).append(',').append(bridge.dsp.cabinet);
    prefs.edit().putString("preset-"+preset,sb.toString()).apply();
    bridge.status="PRESET "+(preset+1)+" SAVED LOCALLY";screen.invalidate();
  }
  private final class Screen extends View {
    private final Paint p=new Paint(3);
    private final RectF r=new RectF();
    private final Typeface mono=Typeface.create("monospace",Typeface.BOLD);
    private long previousRender=0;
    Screen(){super(MainActivity.this);setLayerType(View.LAYER_TYPE_SOFTWARE,null);}
    private void rect(Canvas c,float x,float y,float w,float h,int fill,int line){
      r.set(x,y,x+w,y+h);p.setStyle(Paint.Style.FILL);p.setColor(fill);
      c.drawRoundRect(r,4,4,p);
      p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(1.3f);p.setColor(line);
      c.drawRoundRect(r,4,4,p);p.setStyle(Paint.Style.FILL);
    }
    private void txt(Canvas c,String t,float x,float y,float size,int col){
      p.setTypeface(mono);p.setTextSize(size);p.setColor(col);p.setStyle(Paint.Style.FILL);
      c.drawText(t,x,y,p);
    }
    private void centered(Canvas c,String t,float x,float y,float size,int color){
      p.setTypeface(mono);p.setTextSize(size);
      txt(c,t,x-p.measureText(t)/2,y,size,color);
    }
    private void line(Canvas c,float x,float y,float xx,float yy,int color,float stroke){
      p.setColor(color);p.setStrokeWidth(stroke);c.drawLine(x,y,xx,yy,p);
    }
    private void oval(Canvas c,float cx,float cy,float radius,int color){
      p.setStyle(Paint.Style.FILL);p.setColor(color);c.drawCircle(cx,cy,radius,p);
    }
    private void knob(Canvas c,float x,float y,float value,String text){
      for(int j=0;j<=10;j++){
        double a=Math.PI*.75+j*Math.PI*1.5/10.;
        float px=x+(float)Math.cos(a)*38,py=y+(float)Math.sin(a)*38;
        oval(c,px,py,j<=Math.round(value*10)?1.9f:1.3f,j<=Math.round(value*10)?GOLD:DIM);
      }
      oval(c,x,y,29,CASE);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(2);p.setColor(DIM);
      c.drawCircle(x,y,29,p);p.setStyle(Paint.Style.FILL);
      double angle=Math.PI*.75+value*Math.PI*1.5;
      line(c,x,y,x+(float)Math.cos(angle)*23,y+(float)Math.sin(angle)*23,LIT,3);
      centered(c,text,x,y+59,text.length()>8?11:13,GOLD);
      centered(c,String.format(Locale.US,"%02d",Math.round(value*10)),x,y+76,12,DIM);
    }
    private void title(Canvas c,String t,int x,int y){txt(c,t,x,y,18,GOLD);}
    private void grill(Canvas c){
      rect(c,29,335,661,88,Color.rgb(15,14,11),DIM);
      // Cloth and old-school diamond weave, drawn rather than importing imagery.
      c.save();c.clipRect(32,338,687,420);
      for(int y=343;y<419;y+=4)for(int x=34;x<687;x+=4){
        if(((x/4)+(y/4))%2==0)oval(c,x,y,.57f,Color.rgb(70,46,20));
      }
      for(int x=-96;x<780;x+=72){
        line(c,x,336,x+118,423,DIM,1.1f);
        line(c,x+118,336,x,423,DIM,1.1f);
      }
      c.restore();
    }
    private void header(Canvas c){
      rect(c,12,13,696,78,DARK,DIM);
      txt(c,"A M P S",28,51,33,LIT);
      txt(c,"G U I T A R   A M P",31,77,10,GOLD);
      centered(c,PRESET_NAMES[preset],398,52,18,GOLD);
      txt(c,"IN",552,40,13,DIM);
      oval(c,581,35,6,bridge.inputPeak>.004?GOLD:DIM);
      txt(c,"OUT",606,40,13,DIM);
      oval(c,654,35,6,bridge.outputPeak>.004?GOLD:DIM);
      rect(c,574,56,117,26,CASE,GOLD);
      centered(c,config?"CLOSE":"CONFIG",632,74,14,LIT);
    }
    private void main(Canvas c){
      header(c);
      rect(c,12,103,696,327,CASE,GOLD);
      txt(c,"SUNBIRD",28,133,20,LIT);
      txt(c,"AC CLASS A / COMBO",30,154,11,DIM);
      rect(c,34,166,172,40,DARK,DIM);
      centered(c,bridge.dsp.channel==1?"TOP BOOST":"NORMAL",120,192,17,LIT);
      txt(c,"BRITISH TONE SERIES",32,240,12,GOLD);
      txt(c,"2x12 OPEN BACK",32,258,12,DIM);
      txt(c,bridge.dsp.cabinet==0?"ALNICO BLUE":"GREENBACK",32,279,12,LIT);
      for(int i=0;i<6;i++)knob(c,286+(i%3)*148,170+(i/3)*90,
          getKnob(i),KNOB_NAMES[i]);
      grill(c);
      rect(c,12,440,217,178,DARK,DIM);
      title(c,"CABINET",28,467);
      centered(c,"2 x 12",120,511,27,GOLD);
      centered(c,bridge.dsp.cabinet==0?"ALNICO BLUE":"GREENBACK",120,546,13,LIT);
      centered(c,"< CHANGE >",120,591,14,DIM);
      rect(c,239,440,217,178,DARK,DIM);
      title(c,"REVERB",257,467);
      knob(c,347,517,getKnob(6),"SPRING");
      rect(c,466,440,242,178,DARK,DIM);
      title(c,"TREMOLO",484,467);
      knob(c,523,519,getKnob(7),"SPEED");
      knob(c,649,519,getKnob(8),"DEPTH");
      rect(c,12,628,696,52,DARK,DIM);
      centered(c,"<",38,658,23,LIT);
      centered(c,String.format(Locale.US,"PRESET %02d  %s",preset+1,PRESET_NAMES[preset]),350,658,15,LIT);
      centered(c,">",673,658,23,LIT);
      txt(c,"HOLD PRESET TO SAVE",24,698,10,DIM);
      rect(c,555,686,146,25,bridge.dsp.bypass?GOLD:DARK,GOLD);
      centered(c,bridge.dsp.bypass?"BYPASS ON":"BYPASS OFF",628,704,12,
          bridge.dsp.bypass?DARK:GOLD);
      String state=usbDirectEnabled?usbStatus:bridge.status;
      if(state.length()>52)state=state.substring(0,52);
      txt(c,state,24,715,11,
          usbDirectEnabled&&usbStatus.contains("UNAVAILABLE")?Color.RED:DIM);
    }
    private void config(Canvas c){
      header(c);
      title(c,usbDirectEnabled?"CONFIG / USB HOST":"CONFIG / AAUDIO",25,130);
      txt(c,"APK "+shorten(installedVersion,24),440,128,10,DIM);
      if(usbDirectEnabled){
        rect(c,20,150,680,75,CASE,GOLD);
        txt(c,"ENGINE SELECTED: USB HOST",38,174,16,GOLD);
        txt(c,shorten(usbStatus,57),38,204,15,LIT);
        rect(c,20,235,680,75,CASE,GOLD);
        txt(c,"ACTIVE AUDIO ENGINE (NATIVE)",38,259,15,GOLD);
        String active=bridge.aaudioRunning?"AAUDIO (UNEXPECTED)":
          (UsbDirectEngine.isRunning()?"LIBUSB ASYNC PCM":"NONE / FAILED OR STOPPED");
        txt(c,active,38,290,18,bridge.aaudioRunning?Color.RED:LIT);
        rect(c,20,321,680,72,CASE,GOLD);
        txt(c,"USB ISO QUEUE DEPTH (TAP TO CHANGE)",38,345,15,GOLD);
        txt(c,isoQueueDepth+" URBs / DIRECTION",38,376,18,LIT);
        rect(c,20,407,320,51,DARK,GOLD);
        centered(c,"USB INFO",180,440,17,LIT);
        rect(c,355,407,345,51,DARK,GOLD);
        centered(c,"RESTART USB DIRECT",526,440,15,LIT);
      }else{
        rect(c,20,150,680,75,CASE,GOLD);
        txt(c,"AUDIO INPUT",38,174,16,GOLD);
        txt(c,shorten(labelDevice(selectedDevice(true)),51),38,204,18,LIT);
        rect(c,20,235,680,75,CASE,GOLD);
        txt(c,"AUDIO OUTPUT",38,259,16,GOLD);
        txt(c,shorten(labelDevice(selectedDevice(false)),51),38,289,18,LIT);
        rect(c,20,321,680,72,CASE,GOLD);
        txt(c,"AUDIO BUFFER / BURSTS",38,345,15,GOLD);
        String burst=bridge.burstIndex==0?"AUTO":
            AudioBridge.BURST_LABELS[bridge.burstIndex]+" BURSTS";
        txt(c,burst+"  REQ "+bridge.requestFrames+"  ACT "+bridge.outputFrames+" FR",38,376,17,LIT);
        rect(c,20,407,320,51,DARK,GOLD);
        centered(c,"RESCAN DEVICES",180,440,17,GOLD);
        rect(c,355,407,345,51,DARK,GOLD);
        centered(c,"RESTART AUDIO",526,440,17,GOLD);
      }
      rect(c,20,468,320,38,usbDirectEnabled?GOLD:DARK,GOLD);
      centered(c,usbDirectEnabled?"ENGINE: USB HOST":"ENGINE: AAUDIO",180,493,15,
          usbDirectEnabled?DARK:LIT);
      rect(c,355,468,345,38,DARK,GOLD);
      centered(c,"USB DIAGNOSTICS >",526,493,15,LIT);
      if(usbDirectEnabled){
        txt(c,shorten(usbStatus,74),24,522,12,LIT);
        txt(c,shorten(UsbDirectEngine.stats(),82),24,546,11,GOLD);
        txt(c,"AAUDIO RUNTIME: "+(bridge.aaudioRunning?"ACTIVE - ERROR":"STOPPED"),24,573,16,
            bridge.aaudioRunning?Color.RED:GOLD);
        txt(c,"USB PATH: UAC1 / LIBUSB ASYNC ISO / 48kHz",24,595,12,LIT);
        txt(c,"USB ERRORS: "+shorten(usbReport,61),24,619,11,DIM);
        txt(c,"FULL FAILURE REASON: USB DIAGNOSTICS > VIEW RESULT",24,644,12,GOLD);
      }else{
        txt(c,"IN : "+shorten(bridge.actualIn,52),24,522,14,LIT);
        txt(c,"OUT: "+shorten(bridge.actualOut,52),24,546,14,LIT);
        txt(c,String.format(Locale.US,"OUT BUFFER %d FR / %.1fms (NOT RTT)",
            bridge.outputFrames,
            bridge.outputFrames*1000f/Math.max(1,bridge.sampleRate)),24,572,13,LIT);
        txt(c,"EST INPUT "+msLabel(bridge.estimatedInputLatencyMs)+
            "  EST OUTPUT "+msLabel(bridge.estimatedOutputLatencyMs)+
            " (NOT LOOPBACK)",24,592,12,LIT);
        txt(c,String.format(Locale.US,"BURST %d  RATE %dHz  CB %d  PERF %s / %s",
            bridge.burstFrames,bridge.sampleRate,bridge.outputCallbackFrames,
            bridge.outputPerfMode==12?"LOW":"OTHER",
            bridge.outputMode==0?"EXCL":"SHARED"),24,612,12,LIT);
        txt(c,String.format(Locale.US,"OUT XRUN %d / INPUT MISS %d / READ ERR %d",
            bridge.xruns,bridge.ringStarvation,bridge.inputReadErrors),24,632,12,LIT);
        txt(c,"MAX CALLBACK "+bridge.outputCbMaxUs+"us / "+bridge.outputCbMaxFrames+
            " FR     "+shorten(bridge.status,27),24,651,11,GOLD);
      }
      rect(c,20,658,680,49,CASE,GOLD);
      centered(c,"CLOSE",360,691,18,LIT);
    }
    private void tuning(Canvas c){
      header(c);
      title(c,usbDirectEnabled?"USB HOST / DIAGNOSTICS":"USB TUNING / AAUDIO",25,128);
      txt(c,shorten(usbStatus,78),25,148,12,GOLD);
      if(usbDirectEnabled){
        rect(c,20,162,680,272,CASE,GOLD);
        txt(c,"LIBUSB UAC1 HOST - NOT AAUDIO",35,197,18,GOLD);
        txt(c,"iRig PRO DUO / USB HOST / 48000Hz",35,232,15,LIT);
        txt(c,"ACTIVE: "+(UsbDirectEngine.isRunning()?"YES":"NO / TRANSFER FAILED"),35,270,16,LIT);
        txt(c,"AAUDIO STREAM: "+(bridge.aaudioRunning?"ACTIVE - BUG":"CLOSED"),35,310,16,
            bridge.aaudioRunning?Color.RED:GOLD);
        txt(c,"QUEUE DEPTH: "+isoQueueDepth,35,350,16,LIT);
        txt(c,shorten(UsbDirectEngine.stats(),85),35,397,11,DIM);
      }else{
        for(int row=0;row<TuningProfile.OPTIONS.length;row++){
          int y=162+row*70;
          rect(c,20,y,680,62,CASE,GOLD);
          txt(c,TuningProfile.NAMES[row],35,y+20,13,DIM);
          centered(c,tuning.label(row),365,y+49,18,LIT);
          centered(c,"<",58,y+49,24,GOLD);
          centered(c,">",667,y+49,24,GOLD);
        }
      }
      rect(c,20,446,680,42,DARK,GOLD);
      centered(c,usbDirectEnabled?"RESTART USB DIRECT":"APPLY & RESTART AUDIO",360,473,17,LIT);
      rect(c,20,497,320,42,DARK,GOLD);
      centered(c,"USB INFO",180,525,16,LIT);
      rect(c,355,497,345,42,DARK,GOLD);
      centered(c,usbDirectEnabled?"USB QUEUE CONFIG":"USBFS ISO TEST",526,525,16,LIT);
      rect(c,20,548,320,47,DARK,GOLD);
      centered(c,"VIEW USB RESULT",180,578,16,LIT);
      rect(c,355,548,345,47,DARK,GOLD);
      centered(c,"SAVE REPORT .TXT",526,578,16,LIT);
      if(usbDirectEnabled){
        txt(c,"LIBUSB HOST    "+shorten(UsbDirectEngine.stats(),60),24,618,11,GOLD);
      }else{
        txt(c,"OUT FR "+bridge.outputFrames+" / REQ "+bridge.requestFrames+
            "    XRUN "+bridge.xruns+"    MISS "+bridge.ringStarvation,24,618,12,GOLD);
      }
      rect(c,20,628,680,31,DARK,DIM);
      centered(c,usbDirectEnabled?"CHANGE USB QUEUE DEPTH":"RESET DEFAULTS & APPLY",
          360,650,13,GOLD);
      rect(c,20,666,680,41,CASE,GOLD);
      centered(c,"BACK TO CONFIG",360,693,18,LIT);
    }
    private String msLabel(int value){
      return value<0?"N/A":value+"ms";
    }
    private String shorten(String text,int max){
      return text.length()<=max?text:text.substring(0,max-2)+"..";
    }
    @Override protected void onDraw(Canvas c){
      bridge.poll();
      c.drawColor(DARK);
      float sx=getWidth()/720f,sy=getHeight()/720f;
      c.save();c.scale(sx,sy);
      for(int y=2;y<720;y+=8){
        line(c,0,y,720,y,Color.rgb(16,14,10),.42f);
      }
      if(config){
        if(tuningPage)tuning(c);
        else config(c);
      }else main(c);
      c.restore();
      // Bounded animated peak display; does not require an audio callback to paint.
      long now=SystemClock.uptimeMillis();
      if(now-previousRender>70){previousRender=now;postInvalidateDelayed(170);}
    }
    private boolean within(float x,float y,float l,float t,float w,float h){
      return x>=l&&x<=l+w&&y>=t&&y<=t+h;
    }
    private int hitKnob(float x,float y){
      for(int n=0;n<6;n++){
        float cx=286+(n%3)*148,cy=170+(n/3)*90;
        if(Math.hypot(x-cx,y-cy)<=43)return n;
      }
      for(int n=6;n<9;n++){
        float cx=n==6?347:n==7?523:649;
        if(Math.hypot(x-cx,y-517)<=45)return n;
      }
      return -1;
    }
    @Override public boolean onTouchEvent(MotionEvent e){
      float x=e.getX()*720f/getWidth(),y=e.getY()*720f/getHeight();
      int action=e.getActionMasked();
      if(action==MotionEvent.ACTION_DOWN){
        downAt=SystemClock.uptimeMillis();downX=x;downY=y;
        grabbed=config?-1:hitKnob(x,y);
        if(grabbed>=0){startY=y;startValue=getKnob(grabbed);}
        return true;
      }
      if(action==MotionEvent.ACTION_MOVE){
        if(grabbed>=0)setKnob(grabbed,startValue+(startY-y)/190f);
        return true;
      }
      if(action==MotionEvent.ACTION_UP){
        if(grabbed>=0){
          if(Math.abs(y-startY)<7)setKnob(grabbed, getKnob(grabbed)+.06f);
          grabbed=-1;performClick();return true;
        }
        if(within(x,y,574,56,118,32)){
          config=!config;tuningPage=false;invalidate();return true;
        }
        if(config && tuningPage){
          if(usbDirectEnabled && y>=162 && y<438){
            // USB mode has no AAudio buffer controls: never tune an inactive engine.
          }else if(y>=162&&y<438){
            int row=(int)((y-162)/70);
            if(row>=0&&row<TuningProfile.OPTIONS.length){
              if(x<145)tuning.previous(row);
              else if(x>580)tuning.next(row);
              else selectTuneValue(row);
              invalidate();
            }
          }else if(within(x,y,20,446,680,42)){
            if(usbDirectEnabled)restartSelectedAudio();else applyTune();
          }
          else if(within(x,y,20,497,320,42))selectUsbProbe();
          else if(within(x,y,355,497,345,42)){
            if(usbDirectEnabled)chooseUsbQueueDepth();else selectUsbTest();
          }
          else if(within(x,y,20,548,320,47))showUsbReport();
          else if(within(x,y,355,548,345,47))saveDiagnosticTxt();
          else if(within(x,y,20,628,680,31)){
            if(usbDirectEnabled)chooseUsbQueueDepth();else resetTune();
          }
          else if(y>=666){tuningPage=false;invalidate();}
          performClick();return true;
        }
        if(config){
          if(within(x,y,20,468,320,38))chooseAudioEngine();
          else if(within(x,y,355,468,345,38)){tuningPage=true;invalidate();}
          else if(within(x,y,20,150,680,75)){
            if(usbDirectEnabled)showUsbReport();else chooseDevice(true);
          }
          else if(within(x,y,20,235,680,75)){
            if(usbDirectEnabled)showUsbReport();else chooseDevice(false);
          }
          else if(within(x,y,20,321,680,72)){
            if(usbDirectEnabled)chooseUsbQueueDepth();else chooseBurst();
          }
          else if(within(x,y,20,407,320,52)){
            if(usbDirectEnabled)selectUsbProbe();
            else{
              bridge.status="AUDIO DEVICE LIST REFRESHED";
              invalidate();
            }
          }
          else if(within(x,y,355,407,345,52)){
            if(usbDirectEnabled)restartSelectedAudio();
            else if(checkSelfPermission(Manifest.permission.RECORD_AUDIO)!=PackageManager.PERMISSION_GRANTED)
              requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO},PERMISSION);
            else restartBaselineAudio();
          }else if(y>=658){config=false;invalidate();}
        }else{
          if(within(x,y,34,166,173,45)){
            bridge.dsp.channel=1-bridge.dsp.channel;bridge.updateControls();invalidate();
          }else if(within(x,y,12,440,217,178)){
            bridge.dsp.cabinet=1-bridge.dsp.cabinet;bridge.updateControls();invalidate();
          }else if(within(x,y,12,628,696,52)){
            if(SystemClock.uptimeMillis()-downAt>650&&x>=100&&x<=608)savePreset();
            else loadPreset(preset+(x<100?-1:x>608?1:1));
          }else if(within(x,y,555,686,146,32)){
            bridge.dsp.bypass=!bridge.dsp.bypass;bridge.updateControls();invalidate();
          }
        }
        performClick();return true;
      }
      if(action==MotionEvent.ACTION_CANCEL){grabbed=-1;return true;}
      return true;
    }
    @Override public boolean performClick(){super.performClick();return true;}
  }
}
