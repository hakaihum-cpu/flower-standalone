package com.analoglav.hydrasynthcontroller;

import android.Manifest;
import android.app.Activity;
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
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Entire patch editor, dropdowns and CONFIG run in one View. No Activity navigation. */
public final class MainActivity extends Activity {
  private MidiManager midi;
  private MidiDevice txDevice,rxDevice;
  private MidiInputPort txPort;
  private MidiOutputPort rxPort;
  private final ArrayList<MidiDeviceInfo> outs=new ArrayList<>(),ins=new ArrayList<>();
  private int outId=-1,inId=-1,channel=1,rxBytes;
  private Editor view;
  private final Handler main=new Handler(Looper.getMainLooper());
  private final MidiReceiver receiver=new MidiReceiver(){
    @Override public void onSend(byte[] msg,int offset,int count,long timestamp){
      rxBytes+=count;main.post(()->{if(view!=null)view.invalidate();});
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
    view=new Editor();setContentView(view);
    if(midi!=null)midi.registerDeviceCallback(callback,main);
    refresh();
  }
  @Override protected void onDestroy(){
    if(midi!=null)midi.unregisterDeviceCallback(callback);
    closeTx();closeRx();super.onDestroy();
  }
  private void closeTx(){
    if(txPort!=null){try{txPort.close();}catch(IOException ignored){}txPort=null;}
    if(txDevice!=null){try{txDevice.close();}catch(IOException ignored){}txDevice=null;}
    outId=-1;
  }
  private void closeRx(){
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
      for(int i=0;i<msg.length;i+=3)txPort.send(msg,i,3);
      view.status="TX SENT  CH"+channel+"  "+p.name;
    }catch(IOException|IllegalArgumentException ex){
      view.status="TX FAILED: "+ex.getClass().getSimpleName();
    }
    view.invalidate();
  }
  private void updateBluetoothPermission(){
    if(Build.VERSION.SDK_INT>=31 &&
       checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)!=PackageManager.PERMISSION_GRANTED)
      requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT},11);
  }

  /** Coordinates always originate on a square 720x720 canvas, letterboxed without touch drift. */
  private final class Editor extends View {
    private final int BLACK=0xff100e08,DIM=0xff80671e,YELLOW=0xffffdb46,BRIGHT=0xffffe57a;
    private final int[] BAYER={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    private final Paint pen=new Paint();
    private final Map<String,Integer> pending=new HashMap<>();
    private int moduleIndex,parameterIndex,dropOffset,dropKind; // 0=none, 1=module, 2=parameter, 3=value, 4=out, 5=in, 6=channel
    private boolean config;
    private float startY;
    private int frame;
    private String status="CONFIG > SELECT MIDI OUT";
    private String currentGroup(){return ParameterCatalog.modules().get(moduleIndex);}
    private List<ParameterCatalog.Param> available(){return ParameterCatalog.params(currentGroup());}
    private ParameterCatalog.Param current(){
      List<ParameterCatalog.Param> p=available();
      return p.isEmpty()?null:p.get(Math.min(parameterIndex,p.size()-1));
    }
    private int val(){
      ParameterCatalog.Param p=current();
      return p==null?0:pending.containsKey(p.key())?pending.get(p.key()):p.min;
    }
    private void change(int next){
      ParameterCatalog.Param p=current();
      if(p==null)return;
      pending.put(p.key(),Math.max(p.min,Math.min(p.max,next)));
      status="PENDING / PRESS SEND";invalidate();
    }
    private Editor(){
      super(MainActivity.this);
      pen.setTypeface(Typeface.create(Typeface.MONOSPACE,Typeface.BOLD));
      pen.setAntiAlias(false);setLayerType(View.LAYER_TYPE_SOFTWARE,null);
    }
    private void fill(Canvas c,int color,float x,float y,float w,float h){
      pen.setColor(color);pen.setStyle(Paint.Style.FILL);
      c.drawRect(x,y,x+w,y+h,pen);
    }
    private void outline(Canvas c,int color,float x,float y,float w,float h){
      pen.setColor(color);pen.setStyle(Paint.Style.STROKE);pen.setStrokeWidth(2);
      c.drawRect(x,y,x+w,y+h,pen);pen.setStyle(Paint.Style.FILL);
    }
    private void txt(Canvas c,String text,int color,float x,float y,int size){
      pen.setColor(color);pen.setTextSize(size);
      c.drawText(text,x,y,pen);
    }
    private String fit(String s,int limit){
      if(s==null)return "";
      return s.length()>limit?s.substring(0,Math.max(0,limit-2))+"..":s;
    }
    private void field(Canvas c,String label,String value,int y){
      txt(c,label,DIM,55,y,23);
      fill(c,0xff241f0c,48,y+11,624,78);
      outline(c,DIM,48,y+11,624,78);
      txt(c,fit(value,28),BRIGHT,70,y+66,31);
      txt(c,"v",YELLOW,636,y+62,27);
    }
    private void dotBackground(Canvas c){
      c.drawColor(BLACK);
      frame++;
      for(int y=0;y<720;y+=8)for(int x=0;x<720;x+=8){
        int v=BAYER[((x/8)&3)+((((y/8)&3))<<2)];
        if(v==0||v==7||v==14){
          pen.setColor(v==0?0xff31290e:0xff231d0c);
          c.drawRect(x,y,x+2,y+2,pen);
        }
      }
    }
    @Override protected void onDraw(Canvas actual){
      super.onDraw(actual);
      float scale=Math.min(getWidth()/720f,getHeight()/720f);
      if(scale<=0)return;
      float ox=(getWidth()-720f*scale)/2f,oy=(getHeight()-720f*scale)/2f;
      actual.drawColor(Color.BLACK);
      actual.save();actual.translate(ox,oy);actual.scale(scale,scale);
      Canvas c=actual;dotBackground(c);
      outline(c,DIM,13,13,694,694);
      txt(c,"HYDRA / EDITOR",YELLOW,40,53,35);
      txt(c,txPort==null?"OUT:--":"OUT:OK",txPort==null?DIM:BRIGHT,521,52,22);
      txt(c,"MIDI NRPN  /  DOT  /  AN-62",DIM,41,79,18);
      if(config)drawConfig(c);
      else drawEditor(c);
      if(dropKind!=0)drawDropdown(c);
      actual.restore();
    }
    private void drawEditor(Canvas c){
      field(c,"01  MODULE",currentGroup(),107);
      ParameterCatalog.Param p=current();
      field(c,"02  PARAMETER",p==null?"NO VERIFIED MIDI MAP":p.name,228);
      field(c,"03  VALUE",p==null?"NOT AVAILABLE":pending.containsKey(p.key())?p.display(val()):"SELECT VALUE (UNKNOWN)",349);
      fill(c,0xff241f0c,48,466,195,75);outline(c,DIM,48,466,195,75);
      txt(c,"-  1",YELLOW,107,516,29);
      fill(c,0xff241f0c,260,466,195,75);outline(c,DIM,260,466,195,75);
      txt(c,"+  1",YELLOW,319,516,29);
      fill(c,0xff241f0c,474,466,198,75);outline(c,DIM,474,466,198,75);
      txt(c,"+10",YELLOW,541,516,29);
      boolean ready=p!=null && pending.containsKey(p.key());
      fill(c,ready?YELLOW:DIM,48,562,415,76);
      txt(c,"SEND  >",ready?BLACK:BRIGHT,176,612,36);
      fill(c,0xff241f0c,480,562,192,76);outline(c,YELLOW,480,562,192,76);
      txt(c,"CONFIG",BRIGHT,507,610,27);
      txt(c,fit(status,43),BRIGHT,48,669,21);
      txt(c,"SELECT  >  ADJUST  >  SEND",DIM,48,694,15);
    }
    private void drawConfig(Canvas c){
      fill(c,BLACK,28,93,664,594);outline(c,YELLOW,28,93,664,594);
      txt(c,"CONFIG / MIDI",YELLOW,48,130,30);
      String out=chosenDeviceIndex(true)>0?deviceNames(true).get(chosenDeviceIndex(true)):"Not connected";
      String in=chosenDeviceIndex(false)>0?deviceNames(false).get(chosenDeviceIndex(false)):"Not connected";
      field(c,"OUTPUT DEVICE",out,149);
      field(c,"INPUT DEVICE",in,260);
      field(c,"MIDI CHANNEL","Channel "+channel,371);
      fill(c,0xff241f0c,48,488,282,73);outline(c,DIM,48,488,282,73);
      txt(c,"REFRESH MIDI",BRIGHT,68,537,26);
      fill(c,YELLOW,349,488,323,73);
      txt(c,"CLOSE",BLACK,450,538,31);
      txt(c,"Hydrasynth: PARAM RX = NRPN",DIM,48,604,18);
      txt(c,"MIDI IN monitors bytes only. No sync yet.",DIM,48,631,16);
      txt(c,fit(status,48),BRIGHT,48,667,18);
    }
    private List<String> dropdownItems(){
      ArrayList<String> a=new ArrayList<>();
      if(dropKind==1)return ParameterCatalog.modules();
      if(dropKind==2){for(ParameterCatalog.Param p:available())a.add(p.name);return a;}
      if(dropKind==3){ParameterCatalog.Param p=current();
        if(p!=null)for(int v=p.min;v<=p.max;v++)a.add(p.display(v));
        return a;
      }
      if(dropKind==4)return deviceNames(true);
      if(dropKind==5)return deviceNames(false);
      if(dropKind==6){for(int v=1;v<=16;v++)a.add("Channel "+v);return a;}
      return a;
    }
    private int dropdownSelected(){
      if(dropKind==1)return moduleIndex;
      if(dropKind==2)return parameterIndex;
      if(dropKind==3){ParameterCatalog.Param p=current();return p==null?0:val()-p.min;}
      if(dropKind==4)return chosenDeviceIndex(true);
      if(dropKind==5)return chosenDeviceIndex(false);
      if(dropKind==6)return channel-1;
      return 0;
    }
    private void openDrop(int kind){
      dropKind=kind;dropOffset=Math.max(0,dropdownSelected()-3);invalidate();
    }
    private void drawDropdown(Canvas c){
      fill(c,BLACK,25,101,670,551);
      outline(c,YELLOW,25,101,670,551);
      txt(c,"SELECT  /  TAP ITEM",YELLOW,50,145,27);
      fill(c,0xff241f0c,583,111,95,44);
      txt(c,"CLOSE",BRIGHT,591,141,17);
      List<String> items=dropdownItems();
      int max=Math.max(0,items.size()-8);dropOffset=Math.min(max,Math.max(0,dropOffset));
      for(int i=0;i<8;i++) {
        int index=dropOffset+i;
        if(index>=items.size())break;
        int y=160+i*53;
        boolean selected=index==dropdownSelected();
        fill(c,selected?DIM:0xff211c0c,47,y,626,48);
        txt(c,fit((index+1)+"  "+items.get(index),37),
            selected?BRIGHT:YELLOW,62,y+34,23);
      }
      txt(c,(dropOffset+1)+" - "+Math.min(dropOffset+8,items.size())
          +" / "+items.size(),DIM,50,628,19);
      txt(c,"SWIPE OR TAP RIGHT EDGE TO SCROLL",DIM,50,648,14);
    }
    @Override public boolean onTouchEvent(MotionEvent ev){
      float s=Math.min(getWidth()/720f,getHeight()/720f);
      if(s<=0)return true;
      float x=(ev.getX()-(getWidth()-720f*s)/2f)/s;
      float y=(ev.getY()-(getHeight()-720f*s)/2f)/s;
      if(ev.getActionMasked()==MotionEvent.ACTION_DOWN){startY=y;return true;}
      if(ev.getActionMasked()!=MotionEvent.ACTION_UP)return true;
      if(dropKind!=0){
        List<String> a=dropdownItems();
        float diff=startY-y;
        if(Math.abs(diff)>22){
          dropOffset=Math.min(Math.max(0,a.size()-8),
                    Math.max(0,dropOffset+Math.round(diff/45f)));invalidate();return true;
        }
        if(y>=111&&y<158&&x>=575){dropKind=0;invalidate();return true;}
        if(x>590&&y>=160&&y<590&&a.size()>8){
          dropOffset=Math.min(Math.max(0,a.size()-8),
                 Math.max(0,Math.round((y-160)/430f*(a.size()-8))));
          invalidate();return true;
        }
        if(y>=160&&y<584){
          int ix=dropOffset+(int)(y-160)/53;
          if(ix>=0&&ix<a.size()){
            if(dropKind==1){moduleIndex=ix;parameterIndex=0;status="MODULE SELECTED";}
            if(dropKind==2){parameterIndex=ix;status="PARAMETER SELECTED";}
            if(dropKind==3){ParameterCatalog.Param p=current();if(p!=null)change(p.min+ix);}
            if(dropKind==4)openTx(ix);
            if(dropKind==5)openRx(ix);
            if(dropKind==6){
              channel=ix+1;
              getPreferences(MODE_PRIVATE).edit().putInt("midi_channel",channel).apply();
              status="MIDI CHANNEL "+channel;
            }
          }
          dropKind=0;invalidate();return true;
        }
        return true;
      }
      if(config){
        if(y>=160&&y<253){openDrop(4);return true;}
        if(y>=270&&y<365){openDrop(5);return true;}
        if(y>=380&&y<477){openDrop(6);return true;}
        if(y>=488&&y<561){
          if(x<335){updateBluetoothPermission();refresh();status="DEVICE LIST REFRESHED";}
          else config=false;
          invalidate();return true;
        }
        return true;
      }
      if(y>=118&&y<210){openDrop(1);return true;}
      if(y>=240&&y<332){if(current()!=null)openDrop(2);return true;}
      if(y>=360&&y<450){if(current()!=null)openDrop(3);return true;}
      if(y>=466&&y<541){
        if(x<243)change(val()-1);
        else if(x<463)change(val()+1);
        else change(val()+10);
        return true;
      }
      if(y>=562&&y<639){
        if(x<466){
          if(current()==null){status="NO VERIFIED NRPN - SEND DISABLED";invalidate();}
          else if(!pending.containsKey(current().key())){
            status="SELECT A VALUE BEFORE SEND";invalidate();
          }else send(current(),val());
        } else {config=true;invalidate();}
      }
      return true;
    }
  }
}
