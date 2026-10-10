package com.analoglav.hydrasynthcontroller;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.widget.EditText;
import android.text.InputType;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Entire patch editor, dropdowns and CONFIG run in one View. No Activity navigation. */
public final class MainActivity extends Activity {
  private MidiManager midi;
  private PatchLibrary library;
  private int sendGeneration=0;
  private HydraDumpProtocol.Reader dumpReader;
  private HydraDumpProtocol.Assembler dumpAssembler;
  private final Runnable dumpTimeout=new Runnable(){
    @Override public void run(){if(dumpReader!=null&&dumpReader.active())dumpReader.cancel("SLOT READ TIMEOUT");}
  };
  private void resetDumpTimeout(){
    main.removeCallbacks(dumpTimeout);
    if(dumpReader!=null&&dumpReader.active())main.postDelayed(dumpTimeout,2500);
  }
  private MidiDevice txDevice,rxDevice;
  private MidiInputPort txPort;
  private MidiOutputPort rxPort;
  private final ArrayList<MidiDeviceInfo> outs=new ArrayList<>(),ins=new ArrayList<>();
  private int outId=-1,inId=-1,channel=1,rxBytes,txBytes;
  private Editor view;
  private final Handler main=new Handler(Looper.getMainLooper());
  private final MidiReceiver receiver=new MidiReceiver(){
    @Override public void onSend(byte[] msg,int offset,int count,long timestamp){
      // MidiReceiver callbacks can be split and can run off the UI thread.
      byte[] copy=Arrays.copyOfRange(msg,offset,offset+count);
      main.post(()->{
        rxBytes+=copy.length;
        if(dumpAssembler!=null)dumpAssembler.feed(copy,0,copy.length);
        if(view!=null)view.invalidate();
      });
    }
  };
  private final MidiManager.DeviceCallback callback=new MidiManager.DeviceCallback(){
    @Override public void onDeviceAdded(MidiDeviceInfo d){refresh();}
    @Override public void onDeviceRemoved(MidiDeviceInfo d){
      if(d.getId()==outId)closeTx();
      if(d.getId()==inId)closeRx();
      refresh();
    }
  };
  @Override public void onCreate(Bundle b){
    super.onCreate(b);
    getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
        WindowManager.LayoutParams.FLAG_FULLSCREEN);
    channel=Math.max(1,Math.min(16,getPreferences(MODE_PRIVATE).getInt("midi_channel",1)));
    midi=(MidiManager)getSystemService(Context.MIDI_SERVICE);
    library=new PatchLibrary(this);
    view=new Editor();setContentView(view);
    dumpReader=new HydraDumpProtocol.Reader(new HydraDumpProtocol.Listener(){
      @Override public void send(byte[] data){
        if(txPort==null)throw new IllegalStateException("MIDI OUT DISCONNECTED");
        try{txPort.send(data,0,data.length);txBytes+=data.length;}
        catch(IOException e){throw new IllegalStateException("SYSEX MIDI SEND FAILED",e);}
      }
      @Override public void status(String message){
        view.status=message;view.invalidate();resetDumpTimeout();
      }
      @Override public void complete(byte[] data){
        main.removeCallbacks(dumpTimeout);
        try{view.acceptSlot(new HydraPatchSnapshot(data));}
        catch(RuntimeException e){view.status="PATCH DECODE FAILED / "+e.getMessage();view.invalidate();}
      }
      @Override public void failed(String reason){
        main.removeCallbacks(dumpTimeout);
        view.status="CURRENT LOAD FAILED / "+reason;view.invalidate();
      }
    });
    dumpAssembler=new HydraDumpProtocol.Assembler(frame->{
      if(dumpReader==null||!dumpReader.active())return;
      try{dumpReader.accept(frame);}
      catch(RuntimeException ex){
        try{dumpReader.cancel(ex.getMessage());}
        catch(RuntimeException ignored){view.status="SYSEX IO FAILED";view.invalidate();}
      }
    });
    if(midi!=null)midi.registerDeviceCallback(callback,main);
    refresh();
  }
  @Override protected void onDestroy(){
    if(midi!=null)midi.unregisterDeviceCallback(callback);
    main.removeCallbacks(dumpTimeout);
    if(dumpReader!=null&&dumpReader.active()){
      try{dumpReader.cancel("APP CLOSED");}catch(RuntimeException ignored){}
    }
    closeTx();closeRx();super.onDestroy();
  }
  private void closeTx(){
    if(dumpReader!=null&&dumpReader.active()){
      try{dumpReader.cancel("MIDI OUT CLOSED");}catch(RuntimeException ignored){}
    }
    sendGeneration++;
    if(txPort!=null){try{txPort.close();}catch(IOException ignored){}txPort=null;}
    if(txDevice!=null){try{txDevice.close();}catch(IOException ignored){}txDevice=null;}
    outId=-1;
  }
  private void closeRx(){
    if(dumpReader!=null&&dumpReader.active()){
      try{dumpReader.cancel("MIDI IN CLOSED");}catch(RuntimeException ignored){}
    }
    if(dumpAssembler!=null)dumpAssembler.reset();
    if(rxPort!=null){try{rxPort.close();}catch(IOException ignored){}rxPort=null;}
    if(rxDevice!=null){try{rxDevice.close();}catch(IOException ignored){}rxDevice=null;}
    inId=-1;
  }
  private void refresh(){
    outs.clear();ins.clear();
    if(midi==null){if(view!=null)view.status="MIDI SERVICE UNAVAILABLE";return;}
    try{
      for(MidiDeviceInfo d:midi.getDevices()){
        if(d.getInputPortCount()>0)outs.add(d);   // sends TO a device input port
        if(d.getOutputPortCount()>0)ins.add(d);   // receives FROM device output
      }
    }catch(SecurityException e){view.status="MIDI PERMISSION REQUIRED";}
    if(view!=null)view.invalidate();
  }
  private static String name(MidiDeviceInfo d){
    Object n=d.getProperties().get(MidiDeviceInfo.PROPERTY_NAME);
    return (n==null?"MIDI DEVICE":n.toString())+" ["+d.getId()+"]";
  }
  private List<String> deviceNames(boolean output){
    ArrayList<String> a=new ArrayList<>();a.add("Not connected");
    for(MidiDeviceInfo d:(output?outs:ins))a.add(name(d));
    return a;
  }
  private int chosenDeviceIndex(boolean output){
    ArrayList<MidiDeviceInfo> list=output?outs:ins;
    int id=output?outId:inId;
    for(int i=0;i<list.size();i++)if(list.get(i).getId()==id)return i+1;
    return 0;
  }
  private void openTx(int index){
    closeTx();
    if(index<1||index>outs.size()){view.status="MIDI OUT DISCONNECTED";view.invalidate();return;}
    MidiDeviceInfo info=outs.get(index-1);
    final int id=info.getId();outId=id;view.status="OPENING MIDI OUT...";
    midi.openDevice(info,d->{
      if(outId!=id){if(d!=null)try{d.close();}catch(IOException ignored){}return;}
      if(d==null){outId=-1;view.status="OUT CONNECTION FAILED";view.invalidate();return;}
      MidiInputPort p=d.openInputPort(0);
      if(p==null){try{d.close();}catch(IOException ignored){}outId=-1;
        view.status="NO MIDI INPUT PORT";view.invalidate();return;}
      txDevice=d;txPort=p;view.status="MIDI OUT READY";view.invalidate();
    },main);
  }
  private void openRx(int index){
    closeRx();
    if(index<1||index>ins.size()){view.status="MIDI IN DISCONNECTED";view.invalidate();return;}
    MidiDeviceInfo info=ins.get(index-1);
    final int id=info.getId();inId=id;view.status="OPENING MIDI IN...";
    midi.openDevice(info,d->{
      if(inId!=id){if(d!=null)try{d.close();}catch(IOException ignored){}return;}
      if(d==null){inId=-1;view.status="IN CONNECTION FAILED";view.invalidate();return;}
      MidiOutputPort p=d.openOutputPort(0);
      if(p==null){try{d.close();}catch(IOException ignored){}inId=-1;
        view.status="NO MIDI OUTPUT PORT";view.invalidate();return;}
      rxDevice=d;rxPort=p;p.connect(receiver);view.status="MIDI IN READY";view.invalidate();
    },main);
  }
  private void send(ParameterCatalog.Param p,int value){
    if(txPort==null){view.status="CONNECT MIDI OUT IN CONFIG";view.invalidate();return;}
    try {
      byte[] msg=NrpnEncoder.encode(channel,p.msb,p.lsb,p.encode(value));
      // Preserve NRPN CC 99/98/6/38 together in one ordered output write.
      txPort.send(msg,0,msg.length);
      txBytes+=msg.length;
      view.status="TX TO PORT CH"+channel+"  "+p.name+" / SYNTH NOT VERIFIED";
    }catch(IOException|IllegalArgumentException ex){
      view.status="TX FAILED: "+ex.getClass().getSimpleName();
    }
    view.invalidate();
  }

  /** Send all explicitly staged editor assignments, with throttling and cancellation. */
  private void applyDocument(Map<String,Integer> staged) {
    if(txPort==null){view.status="MIDI OUT NOT CONNECTED";view.invalidate();return;}
    ArrayList<ParameterCatalog.Param> list=new ArrayList<>();
    for(ParameterCatalog.Param p:ParameterCatalog.all()) if(staged.containsKey(p.key()))
      list.add(p);
    if(list.isEmpty()){view.status="NO PATCH VALUES SET";view.invalidate();return;}
    final int job=++sendGeneration;
    final int count=list.size();
    view.status="APPLY "+count+" FIELDS / NOT A SYSEX PATCH";view.invalidate();
    for(int i=0;i<count;i++){
      ParameterCatalog.Param p=list.get(i);
      int value=staged.get(p.key());
      final boolean last=(i==count-1);
      main.postDelayed(()->{
        if(sendGeneration!=job||txPort==null)return;
        try{
          byte[] m=NrpnEncoder.encode(channel,p.msb,p.lsb,p.encode(value));
          txPort.send(m,0,m.length);
          txBytes+=m.length;
          if(last){view.status="TX TO PORT "+count+" FIELDS / SYNTH NOT VERIFIED";view.invalidate();}
        }catch(IOException|IllegalArgumentException err){
          sendGeneration++;
          view.status="APPLY ABORTED: "+err.getClass().getSimpleName();view.invalidate();
        }
      },i*22L);
    }
  }


  private void beginSlotRead(int bank,int slot){
    if(dumpReader==null||dumpReader.active()){view.status="READ ALREADY ACTIVE";view.invalidate();return;}
    if(txPort==null||rxPort==null){
      view.status="CURRENT LOAD NEEDS MIDI OUT + MIDI IN";view.invalidate();return;
    }
    if(dumpAssembler!=null)dumpAssembler.reset();
    try{dumpReader.start(bank,slot);resetDumpTimeout();}
    catch(RuntimeException ex){
      if(dumpReader.active()){
        try{dumpReader.cancel(ex.getMessage());}catch(RuntimeException ignored){}
      }
      view.status="CURRENT LOAD ERROR / "+ex.getMessage();view.invalidate();
    }
  }

  /** Explicit routing check. No patch data is changed by this test. */
  private void sendDiagnosticNote(){
    final MidiInputPort destination=txPort;
    if(destination==null){
      view.status="TEST: SELECT MIDI OUTPUT FIRST";view.invalidate();return;
    }
    final int midiCh=(channel-1)&15;
    try{
      destination.send(new byte[]{(byte)(0x90|midiCh),60,100},0,3);
      view.status="TEST CH"+channel+": NOTE ON WRITTEN TO PORT";
      view.invalidate();
      main.postDelayed(()->{
        try{
          destination.send(new byte[]{(byte)(0x80|midiCh),60,0},0,3);
          view.status="TEST NOTE OFF / NO HYDRASYNTH ACK";
        }catch(IOException e){
          view.status="TEST NOTE OFF PORT ERROR";
        }
        view.invalidate();
      },220L);
    }catch(IOException e){
      view.status="TEST NOTE PORT ERROR";view.invalidate();
    }
  }

  private boolean outputIsDigitakt(){
    for(MidiDeviceInfo d:outs)
      if(d.getId()==outId && name(d).toLowerCase(java.util.Locale.ROOT).contains("digitakt"))
        return true;
    return false;
  }
  private void updateBluetoothPermission(){
    if(Build.VERSION.SDK_INT>=31 &&
       checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)!=PackageManager.PERMISSION_GRANTED)
      requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT},11);
  }

  /**
   * One square view: a permanent patch workbench rather than a MIDI-value list.
   * Signal blocks represent the Explorer sound-design path. The editor area
   * changes IN PLACE when a block is selected; there are no separate screens.
   */
  private final class Editor extends View {
    private final int BG=0xff100e08,PANEL=0xff201c0d,DIM=0xff80671e;
    private final int YELLOW=0xffffdb46,LIGHT=0xffffe57a,GREY=0xff534720;
    private final int[] BAYER={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    private final String[][] FLOW={
      {"OSC 1","OSC 2","OSC 3","RING / NOISE","MUTANT 1","MUTANT 2"},
      {"MUTANT 3","MUTANT 4","MIXER","FILTER 1","FILTER 2","AMP"},
      {"PRE-FX","DELAY","REVERB","POST-FX","VOICE","ARPEGGIATOR"},
      {"ENV 1-5","LFO 1-5","MOD MATRIX","MACRO 1-8","SYSTEM","PATCH"}
    };
    private final Paint brush=new Paint();
    private final java.util.LinkedHashMap<String,Integer> draft=new java.util.LinkedHashMap<>();
    private String patchName="UNTITLED",module="OSC 1",status="SELECT MODULE > EDIT > SEND";
    private boolean dirty, loadedFromHardware;
    private HydraPatchSnapshot sourceSnapshot;
    // Imported UI fields are for viewing; only subsequent deliberate edits are re-sent.
    private final java.util.LinkedHashMap<String,Integer> changedSinceRead=new java.util.LinkedHashMap<>();
    private int chosenBank=0,chosenSlot=0;
    private int sectionOffset;
    private ParameterCatalog.Param selectedParam;
    // panelMode: 0 editor, 1 values, 2 number, 3 family, 4 library, 5 config, 6 CurrentLoad
    private int panelMode,scrollIndex,familyType;
    private int numCurrent;
    private float touchStartY;
    private final float[] area={0,0,720,720};

    Editor() {
      super(MainActivity.this);
      brush.setTypeface(Typeface.create(Typeface.MONOSPACE,Typeface.BOLD));
      brush.setAntiAlias(false);
      setLayerType(View.LAYER_TYPE_SOFTWARE,null);
      setFocusable(true);
    }
    private void rect(Canvas c,int color,float x,float y,float w,float h){
      brush.setColor(color);brush.setStyle(Paint.Style.FILL);
      c.drawRect(x,y,x+w,y+h,brush);
    }
    private void stroke(Canvas c,int color,float x,float y,float w,float h){
      brush.setColor(color);brush.setStyle(Paint.Style.STROKE);brush.setStrokeWidth(2);
      c.drawRect(x,y,x+w,y+h,brush);brush.setStyle(Paint.Style.FILL);
    }
    private void text(Canvas c,String v,int color,float x,float y,int size){
      brush.setColor(color);brush.setTextSize(size);
      c.drawText(v,x,y,brush);
    }
    private String trim(String value,int n){
      if(value==null)return "";
      return value.length()>n?value.substring(0,Math.max(0,n-2))+"..":value;
    }
    private void tile(Canvas c,String label,float x,float y,float w,float h,boolean enabled,boolean active){
      rect(c,active?YELLOW:PANEL,x,y,w,h);
      stroke(c,active?LIGHT:(enabled?DIM:GREY),x,y,w,h);
      text(c,trim(label,13),active?BG:(enabled?YELLOW:GREY),x+8,y+h/2+6,17);
    }
    private void dot(Canvas c){
      c.drawColor(BG);
      for(int y=0;y<720;y+=8)for(int x=0;x<720;x+=8){
        if(BAYER[((x/8)&3)+(((y/8)&3)<<2)]<2)
          rect(c,0xff29230f,x,y,2,2);
      }
      stroke(c,DIM,10,10,700,700);
    }
    private List<ParameterCatalog.Param> fields(){return ParameterCatalog.params(module);}
    private void chooseModule(String name){
      module=name;sectionOffset=0;selectedParam=null;panelMode=0;
      status="EDIT / "+name;invalidate();
    }
    private void stage(ParameterCatalog.Param p,int value){
      if(p==null)return;
      value=Math.max(p.min,Math.min(p.max,value));
      draft.put(p.key(),value);selectedParam=p;dirty=true;
      if(loadedFromHardware)changedSinceRead.put(p.key(),value);
      status="EDITED "+p.name+"  /  SEND WHEN READY";invalidate();
    }
    private int stagedValue(ParameterCatalog.Param p){
      Integer i=draft.get(p.key());return i==null?p.min:i;
    }
    private String stagedText(ParameterCatalog.Param p){
      Integer i=draft.get(p.key());return i==null?"-- SELECT --":p.display(i);
    }
    @Override protected void onDraw(Canvas screen){
      super.onDraw(screen);
      float scale=Math.min(getWidth()/720f,getHeight()/720f);
      if(scale<=0)return;
      float dx=(getWidth()-720f*scale)/2f,dy=(getHeight()-720f*scale)/2f;
      screen.drawColor(0xff000000);
      screen.save();screen.translate(dx,dy);screen.scale(scale,scale);
      dot(screen);
      drawHeader(screen);
      drawRouting(screen);
      if(panelMode==5)drawConfig(screen);
      else if(panelMode==6)drawCurrentLoad(screen);
      else if(panelMode==1||panelMode==3||panelMode==4)drawList(screen);
      else if(panelMode==2)drawNumber(screen);
      else drawModuleEditor(screen);
      screen.restore();
    }
    private void drawHeader(Canvas c){
      text(c,"HYDRA / DOT",YELLOW,28,41,31);
      text(c,trim(patchName+(dirty?" *":""),25),LIGHT,30,71,21);
      text(c,txPort==null?"PORT OFF":"PORT OPEN",txPort==null?DIM:LIGHT,559,42,20);
      text(c,draft.size()+" SET",DIM,593,70,16);
      if(txPort!=null)text(c,trim(deviceLabel(outs,outId),21),DIM,332,70,13);
    }
    private void drawRouting(Canvas c){
      text(c,"SOURCES  >  MUTATORS  >  MIX / FILTER  >  FX",DIM,27,95,15);
      for(int row=0;row<4;row++)for(int col=0;col<6;col++){
        String id=FLOW[row][col];float x=28+col*111,y=104+row*37;
        boolean family=id.equals("ENV 1-5")||id.equals("LFO 1-5")||id.equals("MACRO 1-8");
        boolean active=id.equals(module)||(family&&(
          id.startsWith("ENV")&&module.startsWith("ENV ")||
          id.startsWith("LFO")&&module.startsWith("LFO ")||
          id.startsWith("MACRO")&&module.startsWith("MACRO ")));
        boolean enabled=family||id.equals("PATCH")||!ParameterCatalog.params(id).isEmpty();
        tile(c,id.equals("RING / NOISE")?"RING/NOISE":id,x,y,105,32,enabled,active);
      }
      text(c,"MODULATION        ENV / LFO / MATRIX / MACROS",DIM,27,260,15);
    }
    private void drawModuleEditor(Canvas c){
      rect(c,PANEL,27,274,666,255);
      stroke(c,YELLOW,27,274,666,255);
      text(c,"EDIT / "+module,YELLOW,43,304,25);
      List<ParameterCatalog.Param> ps=fields();
      if(ps.size()>4) {
        tile(c,"<",549,277,38,36,sectionOffset>0,false);
        tile(c,">",649,277,38,36,sectionOffset+4<ps.size(),false);
        text(c,(sectionOffset/4+1)+"/"+((ps.size()+3)/4),DIM,594,304,18);
      }
      if(ps.isEmpty()){
        text(c,"NOT MAPPED FOR SAFE MIDI EDIT",LIGHT,51,367,23);
        text(c,"This module remains in the patch path.",DIM,51,402,18);
        text(c,"Do not invent parameter addresses.",DIM,51,429,18);
        text(c,"PATCH SAVE stores assigned values only.",DIM,51,472,17);
      }else {
        for(int i=0;i<4;i++){
          int ix=sectionOffset+i;
          if(ix>=ps.size())break;
          ParameterCatalog.Param p=ps.get(ix);
          int col=i%2,row=i/2;
          int x=41+col*330,y=322+row*99;
          boolean active=p==selectedParam;
          rect(c,active?0xff393017:BG,x,y,310,90);
          stroke(c,active?YELLOW:DIM,x,y,310,90);
          text(c,trim(p.name,23),active?LIGHT:YELLOW,x+12,y+25,20);
          text(c,trim(stagedText(p),22),
            draft.containsKey(p.key())?LIGHT:GREY,x+12,y+62,24);
          text(c,p.options==null?"+ -":">",DIM,x+280,y+66,18);
        }
      }
      text(c,trim(status,65),LIGHT,38,557,17);
      tile(c,"SEND FIELD",28,570,323,43,
        selectedParam!=null && draft.containsKey(selectedParam.key()),false);
      tile(c,loadedFromHardware?"APPLY EDITS":"APPLY PATCH",365,570,328,43,
        loadedFromHardware?!changedSinceRead.isEmpty():!draft.isEmpty(),false);
      drawBottom(c);
    }
    private void drawBottom(Canvas c){
      tile(c,"NEW",28,626,125,49,true,false);
      tile(c,"SAVE",163,626,125,49,true,false);
      tile(c,"LOAD",298,626,125,49,true,false);
      tile(c,dumpReader!=null&&dumpReader.active()?"CANCEL READ":"CURRENTLOAD",433,626,125,49,true,false);
      tile(c,"CONFIG",568,626,125,49,true,false);
      text(c,sourceSnapshot==null?
        "LOCAL PRESET / HARDWARE FLASH WRITE NOT AVAILABLE":
        "SOURCE SLOT "+(char)('A'+sourceSnapshot.bank)+"-"+(sourceSnapshot.slot+1)+
        " / RAW 2790B / UI "+draft.size()+" FIELDS",DIM,29,694,15);
    }
    private List<String> listItems(){
      ArrayList<String> items=new ArrayList<>();
      if(panelMode==1 && selectedParam!=null&&selectedParam.options!=null){
        for(String s:selectedParam.options)items.add(s);
      }else if(panelMode==3){
        String family=familyType==1?"ENV ":familyType==2?"LFO ":"MACRO ";
        int n=familyType==3?8:5;
        for(int i=1;i<=n;i++)items.add(family+i);
      }else if(panelMode==4){
        try{items.addAll(library.names());}catch(Exception e){status="PRESET READ ERROR";}
      }
      return items;
    }
    private void drawList(Canvas c){
      rect(c,BG,27,274,666,340);stroke(c,YELLOW,27,274,666,340);
      String title=panelMode==1?selectedParam.name:panelMode==4?"LOAD LOCAL PRESET":"SELECT MODULE";
      text(c,trim(title,29),YELLOW,44,311,25);
      tile(c,"CLOSE",570,280,110,37,true,false);
      List<String> list=listItems();
      scrollIndex=Math.max(0,Math.min(scrollIndex,Math.max(0,list.size()-6)));
      for(int i=0;i<6;i++){
        int index=scrollIndex+i;
        if(index>=list.size())break;
        tile(c,trim((index+1)+"  "+list.get(index),37),
          44,330+i*42,634,38,true,false);
      }
      if(list.size()>6) {
        // Direct scrollbar allows reaching waveform 219 without dozens of swipes.
        rect(c,DIM,671,330,6,248);
        rect(c,YELLOW,665,330+(int)(220f*scrollIndex/(list.size()-6)),18,25);
      }
      text(c,(list.isEmpty()?"NO STORED PRESETS":(scrollIndex+1)+" - "
          +Math.min(scrollIndex+6,list.size())+" / "+list.size()),
          DIM,49,604,17);
      drawBottom(c);
    }
    private void drawNumber(Canvas c){
      rect(c,BG,27,274,666,340);stroke(c,YELLOW,27,274,666,340);
      if(selectedParam==null){panelMode=0;return;}
      text(c,trim(selectedParam.name,29),YELLOW,45,310,25);
      tile(c,"CLOSE",570,280,110,37,true,false);
      text(c,selectedParam.display(numCurrent),LIGHT,55,389,41);
      text(c,"RANGE "+selectedParam.min+" ... "+selectedParam.max,DIM,55,413,17);
      tile(c,"-10",43,442,149,64,true,false);
      tile(c,"-1",206,442,149,64,true,false);
      tile(c,"+1",369,442,149,64,true,false);
      tile(c,"+10",532,442,146,64,true,false);
      tile(c,"USE VALUE",43,528,635,57,true,false);
      drawBottom(c);
    }
    private void drawCurrentLoad(Canvas c){
      rect(c,BG,27,274,666,340);stroke(c,YELLOW,27,274,666,340);
      text(c,"CURRENT LOAD / SLOT READ",YELLOW,42,310,25);
      tile(c,"CLOSE",570,280,110,37,true,false);
      text(c,"SAVED SLOT ONLY - NOT UNSAVED EDIT BUFFER",DIM,43,329,16);
      for(int i=0;i<8;i++){
        int col=i%4,row=i/4;
        tile(c,"BANK "+(char)('A'+i),43+159*col,339+45*row,148,39,true,chosenBank==i);
      }
      text(c,"SLOT",DIM,45,450,18);
      text(c,""+(chosenSlot+1)+" / 128",LIGHT,181,454,30);
      tile(c,"-10",43,469,148,52,true,false);
      tile(c,"-1",205,469,148,52,true,false);
      tile(c,"+1",367,469,148,52,true,false);
      tile(c,"+10",529,469,149,52,true,false);
      tile(c,dumpReader.active()?"READING ...":"READ SAVED SLOT",43,538,635,51,
           !dumpReader.active()&&txPort!=null&&rxPort!=null,false);
      text(c,dumpReader.active()?trim(status,55):
        "Requires MIDI IN + OUT. Does not modify synth.",DIM,43,611,16);
      drawBottom(c);
    }
    private void acceptSlot(HydraPatchSnapshot snapshot){
      // Nothing in the editor changes before the complete 22 chunk dump,
      // CRC, expected slot, version and footer checks have all succeeded.
      sourceSnapshot=snapshot;
      chosenBank=snapshot.bank;chosenSlot=snapshot.slot;
      draft.clear();draft.putAll(snapshot.mapped);
      changedSinceRead.clear();loadedFromHardware=true;
      patchName=snapshot.name;dirty=false;
      selectedParam=null;sectionOffset=0;panelMode=0;module="OSC 1";
      status="READ "+(char)('A'+snapshot.bank)+"-"+(snapshot.slot+1)+
        " / FULL RAW / UI "+draft.size()+" OF "+ParameterCatalog.mappedCount();
      invalidate();
    }
    private void drawConfig(Canvas c){
      rect(c,BG,27,274,666,340);stroke(c,YELLOW,27,274,666,340);
      text(c,"CONFIG / MIDI",YELLOW,45,310,25);
      tile(c,"CLOSE",570,280,110,37,true,false);
      drawConfigRow(c,"MIDI OUTPUT",deviceLabel(outs,outId),337);
      drawConfigRow(c,"MIDI INPUT",deviceLabel(ins,inId),410);
      drawConfigRow(c,"MIDI CHANNEL","CHANNEL "+channel,483);
      tile(c,"REFRESH",43,563,308,41,true,false);
      tile(c,"TEST NOTE",364,563,314,41,txPort!=null,false);
      text(c,outputIsDigitakt()?"DIGITAKT USB: VERIFY DIN ROUTING":
           "HYDRA: SET SYSTEM PARAM RX = NRPN",DIM,43,612,16);
      text(c,"TX="+txBytes+" BYTES   RX="+rxBytes+" BYTES (NOT ACK)",DIM,43,632,15);
      drawBottom(c);
    }
    private void drawConfigRow(Canvas c,String label,String value,int y){
      text(c,label,DIM,47,y,18);
      rect(c,PANEL,43,y+7,635,42);stroke(c,DIM,43,y+7,635,42);
      text(c,trim(value,42),LIGHT,54,y+36,20);
    }
    private String deviceLabel(List<MidiDeviceInfo> list,int id){
      for(MidiDeviceInfo d:list)if(d.getId()==id)return name(d);
      return "NOT CONNECTED  > SELECT";
    }
    private void popupValues(ParameterCatalog.Param p){
      selectedParam=p;scrollIndex=0;
      if(p.options!=null){
        panelMode=1;Integer existing=draft.get(p.key());
        if(existing!=null)scrollIndex=Math.max(0,existing-3);
      }else{
        panelMode=2;numCurrent=stagedValue(p);
      }
      invalidate();
    }
    private void triggerSave(){
      EditText e=new EditText(MainActivity.this);
      e.setSingleLine(true);e.setText(patchName);
      e.setTextColor(YELLOW);e.setHintTextColor(DIM);e.setSelectAllOnFocus(true);
      e.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);
      new AlertDialog.Builder(MainActivity.this)
        .setTitle("SAVE LOCAL PRESET")
        .setMessage("Saves assigned editor values locally. Not a synth patch dump.")
        .setView(e)
        .setNegativeButton("CANCEL",null)
        .setPositiveButton("SAVE",(d,w)->{
          String name=e.getText().toString().trim();
          try{
            if(library.names().contains(name)){
              new AlertDialog.Builder(MainActivity.this)
              .setTitle("REPLACE PRESET?")
              .setMessage(name)
              .setNegativeButton("CANCEL",null)
              .setPositiveButton("REPLACE",(a,b)->store(name)).show();
            } else store(name);
          }catch(Exception ex){status="SAVE FAILED: "+ex.getClass().getSimpleName();invalidate();}
        }).show();
    }
    private void store(String name){
      try{
        library.save(name,draft,sourceSnapshot==null?null:sourceSnapshot.raw);
        patchName=name;dirty=false;
        status="SAVED LOCALLY / "+draft.size()+" UI VALUES"+
          (sourceSnapshot!=null?" + ORIGINAL RAW":"");
      }catch(Exception ex){status="SAVE FAILED: "+ex.getClass().getSimpleName();}
      invalidate();
    }
    private void openLibrary(){panelMode=4;scrollIndex=0;invalidate();}
    private void load(String name){
      try{
        PatchLibrary.Document doc=library.load(name);
        // Validate before altering the current patch document.
        HydraPatchSnapshot baseline=doc.sourcePatch==null?null:new HydraPatchSnapshot(doc.sourcePatch);
        sourceSnapshot=baseline;loadedFromHardware=baseline!=null;
        changedSinceRead.clear();
        draft.clear();draft.putAll(doc.values);patchName=doc.name;
        dirty=false;selectedParam=null;sectionOffset=0;panelMode=0;
        status="LOADED LOCAL / "+draft.size()+" UI VALUES"+
          (baseline==null?"":" + ORIGINAL RAW");
      }catch(Exception ex){status="LOAD FAILED: "+ex.getClass().getSimpleName();}
      invalidate();
    }
    private void newPatch(){
      draft.clear();patchName="UNTITLED";selectedParam=null;dirty=false;
      sourceSnapshot=null;loadedFromHardware=false;changedSinceRead.clear();
      sectionOffset=0;panelMode=0;status="NEW PROJECT / NO SYNTH INIT SENT";
      invalidate();
    }
    private void requireClean(Runnable next){
      if(!dirty){next.run();return;}
      new AlertDialog.Builder(MainActivity.this)
        .setTitle("DISCARD UNSAVED EDITS?")
        .setMessage("Your local patch has unsaved parameter edits.")
        .setNegativeButton("CANCEL",null)
        .setPositiveButton("DISCARD",(d,w)->next.run()).show();
    }
    private void processSelection(int i){
      if(panelMode==1) {
        if(selectedParam!=null&&i>=0&&i<selectedParam.options.length)
          stage(selectedParam,selectedParam.min+i);
        panelMode=0;invalidate();return;
      }
      if(panelMode==3){
        List<String> items=listItems();
        if(i>=0&&i<items.size())chooseModule(items.get(i));
        else panelMode=0;
        invalidate();return;
      }
      if(panelMode==4){
        List<String> names=listItems();
        if(i>=0&&i<names.size()){
          String target=names.get(i);
          requireClean(()->load(target));
        }
        panelMode=0;invalidate();
      }
    }
    private void routingTap(float x,float y){
      int col=(int)((x-28)/111f),row=(int)((y-104)/37f);
      if(row<0||row>=4||col<0||col>=6)return;
      String id=FLOW[row][col];
      if(id.equals("PATCH")){openLibrary();return;}
      if(id.equals("ENV 1-5")||id.equals("LFO 1-5")||id.equals("MACRO 1-8")){
        familyType=id.startsWith("ENV")?1:id.startsWith("LFO")?2:3;
        scrollIndex=0;panelMode=3;invalidate();return;
      }
      chooseModule(id);
    }
    @Override public boolean onTouchEvent(MotionEvent event){
      float scale=Math.min(getWidth()/720f,getHeight()/720f);
      if(scale<=0)return true;
      float x=(event.getX()-(getWidth()-720f*scale)/2f)/scale;
      float y=(event.getY()-(getHeight()-720f*scale)/2f)/scale;
      if(event.getActionMasked()==MotionEvent.ACTION_DOWN){
        touchStartY=y;return true;
      }
      if(event.getActionMasked()!=MotionEvent.ACTION_UP)return true;
      if(y>=626&&y<679){
        if(x<158)requireClean(this::newPatch);
        else if(x<293)triggerSave();
        else if(x<428)openLibrary();
        else if(x<563){
          if(dumpReader!=null&&dumpReader.active()){
            dumpReader.cancel("USER CANCELLED");
          }else{panelMode=6;invalidate();}
        }else{panelMode=5;invalidate();}
        return true;
      }
      if(panelMode!=0){
        if(y>=280&&y<324&&x>564){if(!dumpReader.active())panelMode=0;invalidate();return true;}
        if(panelMode==6){
          if(dumpReader.active())return true;
          if(y>=339&&y<424&&x>=43&&x<681){
            int col=(int)((x-43)/159f),row=(int)((y-339)/45f);
            int index=row*4+col;
            if(index>=0&&index<8)chosenBank=index;
            invalidate();return true;
          }
          if(y>=469&&y<522){
            int delta=x<198?-10:x<361?-1:x<523?1:10;
            chosenSlot=Math.max(0,Math.min(127,chosenSlot+delta));
            invalidate();return true;
          }
          if(y>=538&&y<592){
            requireClean(()->beginSlotRead(chosenBank,chosenSlot));
            return true;
          }
          return true;
        }
        if(panelMode==5){
          if(y>=340&&y<391){chooseMidi(false);return true;}
          if(y>=414&&y<464){chooseMidi(true);return true;}
          if(y>=489&&y<540){chooseChannel();return true;}
          if(y>=563&&y<611){
            if(x<359){
              updateBluetoothPermission();refresh();status="MIDI DEVICE LIST REFRESHED";
            } else sendDiagnosticNote();
            invalidate();
          }
          return true;
        }
        if(panelMode==2){
          if(y>=442&&y<512){
            int step=x<198?-10:x<361?-1:x<525?1:10;
            numCurrent=Math.max(selectedParam.min,Math.min(selectedParam.max,numCurrent+step));
            invalidate();return true;
          }
          if(y>=525&&y<590){stage(selectedParam,numCurrent);panelMode=0;invalidate();}
          return true;
        }
        if(panelMode==1||panelMode==3||panelMode==4){
          List<String> items=listItems();
          float diff=touchStartY-y;
          if(Math.abs(diff)>20){
            scrollIndex=Math.max(0,Math.min(Math.max(0,items.size()-6),
                 scrollIndex+Math.round(diff/34f)));invalidate();return true;
          }
          if(x>=655&&y>=330&&y<582&&items.size()>6){
            scrollIndex=Math.max(0,Math.min(items.size()-6,
              Math.round((y-330)/252f*(items.size()-6))));
            invalidate();return true;
          }
          if(y>=330&&y<582){
            int chosen=scrollIndex+(int)((y-330)/42f);
            if(chosen>=0&&chosen<items.size())processSelection(chosen);
          }
          return true;
        }
      }
      if(y>=104&&y<252){routingTap(x,y);return true;}
      List<ParameterCatalog.Param> fields=fields();
      if(y>=274&&y<315&&fields.size()>4){
        if(x>=547&&x<592)sectionOffset=Math.max(0,sectionOffset-4);
        if(x>=637)sectionOffset=Math.min(((fields.size()-1)/4)*4,sectionOffset+4);
        invalidate();return true;
      }
      if(y>=322&&y<513){
        int row=y<414?0:1,col=x<362?0:1;
        int ix=sectionOffset+row*2+col;
        if(ix>=0&&ix<fields.size())popupValues(fields.get(ix));
        return true;
      }
      if(y>=570&&y<613){
        if(x<358){
          if(selectedParam==null||!draft.containsKey(selectedParam.key())){
            status="SELECT A PARAMETER VALUE FIRST";invalidate();
          } else send(selectedParam,draft.get(selectedParam.key()));
        } else {
          applyDocument(new java.util.LinkedHashMap<>(
            loadedFromHardware?changedSinceRead:draft));
        }
      }
      return true;
    }
    private void chooseMidi(boolean input){
      final ArrayList<MidiDeviceInfo> l=input?ins:outs;
      String[] options=new String[l.size()+1];
      options[0]="Not connected";
      for(int i=0;i<l.size();i++)options[i+1]=name(l.get(i));
      // A small selection dialog is an overlay for device setup, not a navigation screen.
      new AlertDialog.Builder(MainActivity.this)
        .setTitle(input?"MIDI INPUT":"MIDI OUTPUT")
        .setItems(options,(d,index)->{if(input)openRx(index);else openTx(index);invalidate();})
        .show();
    }
    private void chooseChannel(){
      String[] channels=new String[16];
      for(int i=0;i<16;i++)channels[i]="MIDI CH "+(i+1);
      new AlertDialog.Builder(MainActivity.this)
        .setTitle("MIDI CHANNEL")
        .setSingleChoiceItems(channels,channel-1,(d,which)->{
          channel=which+1;
          getPreferences(MODE_PRIVATE).edit().putInt("midi_channel",channel).apply();
          status="MIDI CH "+channel;d.dismiss();invalidate();
        }).setNegativeButton("CANCEL",null).show();
    }
  }
}
