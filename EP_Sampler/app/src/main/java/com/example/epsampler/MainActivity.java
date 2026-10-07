package com.example.epsampler;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.*;
import android.graphics.Color;
import android.hardware.usb.*;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.*;

import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * DaVinci Resolve Speed Editor diagnostic probe.
 *
 * The HID key/report mapping and the mutual-authentication algorithm are based on
 * Sylvain Munaut's Apache-2.0-licensed blackmagic-misc/bmd.py implementation.
 * This app is a clean Android/UsbHost implementation that validates the
 * already-known Speed Editor protocol on Android before EP-SAMPLE integration.
 * It is intentionally not a blind HID discovery/calibration workflow.
 */
public class MainActivity extends Activity {
    private static final int USB_VID = 0x1edb;
    private static final int USB_PID = 0xda0e;

    // Known Search Dial mode mapping from speed-editor-demo.py.
    private static final int KEY_SHTL = 0x1c;
    private static final int KEY_JOG  = 0x1d;
    private static final int KEY_SCRL = 0x1e;
    private static final int JOG_ABSOLUTE_CONTINUOUS = 1;
    private static final int JOG_RELATIVE_2 = 2;
    private static final int JOG_ABSOLUTE_DEADZERO = 3;

    private static final String ACTION_USB_PERMISSION = "com.example.epmodel.speededitorprobe.USB_PERMISSION";
    private static final int REQ_EXPORT = 701;

    private UsbManager usbManager;
    private UsbDevice device;
    private UsbDeviceConnection connection;
    private UsbInterface hidInterface;
    private UsbEndpoint inEndpoint;
    private Thread readerThread;
    private final AtomicBoolean running = new AtomicBoolean(false);

    private TextView statusView, stepView, lastView, progressView;
    private Button nextButton, backButton, skipButton, exportButton, restartButton;

    private final Object logLock = new Object();
    private final StringBuilder rawLog = new StringBuilder(128 * 1024);
    private final ArrayList<StepResult> results = new ArrayList<>();
    private int stepIndex = 0;
    private long stepStartMs;
    private int dialEventsThisStep = 0;
    private Set<Integer> lastHeld = new HashSet<>();
    private long authAtMs = 0;
    private int authTimeoutSec = 0;

    private static class KeyDef {
        final int code; final String label;
        KeyDef(int c, String l) { code=c; label=l; }
    }
    private static class Step {
        final String prompt;
        final Integer expectedKey;
        final boolean dial;
        Step(String p, Integer k, boolean d) { prompt=p; expectedKey=k; dial=d; }
    }
    private static class StepResult {
        int index; String prompt; String result; long elapsedMs; String detail="";
    }

    private static final KeyDef[] KEYS = new KeyDef[] {
        new KeyDef(0x01,"SMART INSRT [CLIP]"), new KeyDef(0x02,"APPND [CLIP]"),
        new KeyDef(0x03,"RIPL O/WR"), new KeyDef(0x04,"CLOSE UP [YPOS]"),
        new KeyDef(0x05,"PLACE ON TOP"), new KeyDef(0x06,"SRC O/WR"),
        new KeyDef(0x07,"IN [CLR]"), new KeyDef(0x08,"OUT [CLR]"),
        new KeyDef(0x09,"TRIM IN"), new KeyDef(0x0a,"TRIM OUT"),
        new KeyDef(0x0b,"ROLL [SLIDE]"), new KeyDef(0x0c,"SLIP SRC"),
        new KeyDef(0x0d,"SLIP DEST"), new KeyDef(0x0e,"TRANS DUR [SET]"),
        new KeyDef(0x0f,"CUT"), new KeyDef(0x10,"DIS"), new KeyDef(0x11,"SMTH CUT"),
        new KeyDef(0x1a,"SOURCE"), new KeyDef(0x1b,"TIMELINE"),
        new KeyDef(0x1c,"SHTL"), new KeyDef(0x1d,"JOG"), new KeyDef(0x1e,"SCRL"),
        new KeyDef(0x31,"ESC [UNDO]"), new KeyDef(0x1f,"SYNC BIN"),
        new KeyDef(0x2c,"AUDIO LEVEL [MARK]"), new KeyDef(0x2d,"FULL VIEW [RVW]"),
        new KeyDef(0x22,"TRANS [TITLE]"), new KeyDef(0x2f,"SPLIT [MOVE]"),
        new KeyDef(0x2e,"SNAP [=]"), new KeyDef(0x2b,"RIPL DEL"),
        new KeyDef(0x33,"CAM 1"), new KeyDef(0x34,"CAM 2"), new KeyDef(0x35,"CAM 3"),
        new KeyDef(0x36,"CAM 4"), new KeyDef(0x37,"CAM 5"), new KeyDef(0x38,"CAM 6"),
        new KeyDef(0x39,"CAM 7"), new KeyDef(0x3a,"CAM 8"), new KeyDef(0x3b,"CAM 9"),
        new KeyDef(0x30,"LIVE O/WR [RND]"), new KeyDef(0x25,"VIDEO ONLY"),
        new KeyDef(0x26,"AUDIO ONLY"), new KeyDef(0x3c,"STOP/PLAY")
    };

    private final ArrayList<Step> steps = new ArrayList<>();

    @Override public void onCreate(Bundle b) {
        super.onCreate(b);
        buildSteps();
        buildUi();
        usbManager = (UsbManager)getSystemService(Context.USB_SERVICE);
        IntentFilter f = new IntentFilter();
        f.addAction(ACTION_USB_PERMISSION);
        f.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        f.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(usbReceiver, f, Context.RECEIVER_NOT_EXPORTED);
        else registerReceiver(usbReceiver, f);
        resetSession();
    }

    @Override protected void onResume() { super.onResume(); scanAndConnect(); }
    @Override protected void onDestroy() {
        running.set(false);
        try { unregisterReceiver(usbReceiver); } catch (Exception ignored) {}
        closeUsb();
        super.onDestroy();
    }

    private void buildSteps() {
        steps.clear();
        for (KeyDef k: KEYS) steps.add(new Step("PRESS: " + k.label, k.code, false));
        steps.add(new Step("DIAL / JOG: press JOG, then rotate CLOCKWISE several clicks", null, true));
        steps.add(new Step("DIAL / JOG: rotate COUNTER-CLOCKWISE several clicks", null, true));
        steps.add(new Step("DIAL / SCRL: press SCRL, then rotate CLOCKWISE", null, true));
        steps.add(new Step("DIAL / SCRL: rotate COUNTER-CLOCKWISE", null, true));
        steps.add(new Step("DIAL / SHTL: press SHTL, move from center CLOCKWISE, then return center", null, true));
        steps.add(new Step("DIAL / SHTL: move from center COUNTER-CLOCKWISE, then return center", null, true));
    }

    private TextView tv(int sp, int color) {
        TextView v=new TextView(this); v.setTextSize(sp); v.setTextColor(color); v.setPadding(16,8,16,8); return v;
    }
    private void buildUi() {
        LinearLayout root=new LinearLayout(this); root.setOrientation(LinearLayout.VERTICAL); root.setPadding(14,14,14,14); root.setBackgroundColor(Color.rgb(16,16,16));
        ScrollView sv=new ScrollView(this); LinearLayout body=new LinearLayout(this); body.setOrientation(LinearLayout.VERTICAL); sv.addView(body);
        TextView title=tv(20,Color.WHITE); title.setText("SPEED EDITOR HID CHECK / EP-SAMPLE"); body.addView(title);
        statusView=tv(14,Color.LTGRAY); body.addView(statusView);
        progressView=tv(13,Color.GRAY); body.addView(progressView);
        stepView=tv(24,Color.WHITE); stepView.setMinHeight(140); body.addView(stepView);
        lastView=tv(12,Color.LTGRAY); lastView.setMovementMethod(new ScrollingMovementMethod()); lastView.setMinHeight(170); body.addView(lastView);
        LinearLayout row1=new LinearLayout(this); row1.setOrientation(LinearLayout.HORIZONTAL);
        backButton=new Button(this); backButton.setText("SHTL MODE"); backButton.setOnClickListener(v->selectJogMode(KEY_SHTL)); row1.addView(backButton,new LinearLayout.LayoutParams(0,-2,1));
        nextButton=new Button(this); nextButton.setText("JOG MODE"); nextButton.setOnClickListener(v->selectJogMode(KEY_JOG)); row1.addView(nextButton,new LinearLayout.LayoutParams(0,-2,1));
        skipButton=new Button(this); skipButton.setText("SCRL MODE"); skipButton.setOnClickListener(v->selectJogMode(KEY_SCRL)); row1.addView(skipButton,new LinearLayout.LayoutParams(0,-2,1));
        body.addView(row1);
        LinearLayout row2=new LinearLayout(this); row2.setOrientation(LinearLayout.HORIZONTAL);
        exportButton=new Button(this); exportButton.setText("EXPORT TXT"); exportButton.setOnClickListener(v->exportTxt()); row2.addView(exportButton,new LinearLayout.LayoutParams(0,-2,1));
        restartButton=new Button(this); restartButton.setText("CLEAR LOG"); restartButton.setOnClickListener(v->resetSession()); row2.addView(restartButton,new LinearLayout.LayoutParams(0,-2,1));
        body.addView(row2);
        TextView note=tv(12,Color.GRAY); note.setText("Known HID protocol is already loaded. After AUTH OK, press any representative keys: their known names/codes appear live. Release SHTL/JOG/SCRL or use the mode buttons above, then turn the Search Dial. Export TXT only if a mismatch appears or after a short representative check."); body.addView(note);
        root.addView(sv,new LinearLayout.LayoutParams(-1,0,1)); setContentView(root);
    }

    private final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) {
            String a=i.getAction();
            if (ACTION_USB_PERMISSION.equals(a)) {
                UsbDevice d = i.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                boolean ok = i.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED,false);
                appendMeta("USB_PERMISSION result="+ok+" device="+describeDevice(d));
                if(ok && d!=null) openUsb(d); else setStatus("USB permission denied. Reconnect or reopen app.");
            } else if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(a)) scanAndConnect();
            else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(a)) {
                UsbDevice d=i.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                appendMeta("USB_DETACHED "+describeDevice(d));
                if(device!=null && d!=null && device.getDeviceId()==d.getDeviceId()) { closeUsb(); setStatus("Speed Editor disconnected."); }
            }
        }
    };

    private void scanAndConnect() {
        if (connection != null) return;
        for(UsbDevice d: usbManager.getDeviceList().values()) {
            appendMeta("USB_SEEN "+describeDevice(d));
            if(d.getVendorId()==USB_VID && d.getProductId()==USB_PID) {
                device=d;
                if(usbManager.hasPermission(d)) openUsb(d); else requestUsbPermission(d);
                return;
            }
        }
        setStatus("Waiting for DaVinci Resolve Speed Editor USB (VID 1EDB / PID DA0E)...");
    }

    private void requestUsbPermission(UsbDevice d) {
        int flags=PendingIntent.FLAG_UPDATE_CURRENT;
        if(Build.VERSION.SDK_INT>=31) flags|=PendingIntent.FLAG_MUTABLE;
        PendingIntent pi=PendingIntent.getBroadcast(this,0,new Intent(ACTION_USB_PERMISSION).setPackage(getPackageName()),flags);
        appendMeta("USB_PERMISSION requested");
        usbManager.requestPermission(d,pi);
        setStatus("Approve the Android USB permission dialog.");
    }

    private synchronized void openUsb(UsbDevice d) {
        if(connection!=null) return;
        UsbDeviceConnection c=usbManager.openDevice(d);
        if(c==null){ setStatus("openDevice() failed"); appendMeta("ERROR openDevice failed"); return; }
        UsbInterface best=null; UsbEndpoint in=null;
        appendMeta("OPEN "+describeDevice(d));
        for(int i=0;i<d.getInterfaceCount();i++) {
            UsbInterface it=d.getInterface(i);
            appendMeta(String.format(Locale.US,"IFACE index=%d id=%d class=%d subclass=%d protocol=%d endpoints=%d",i,it.getId(),it.getInterfaceClass(),it.getInterfaceSubclass(),it.getInterfaceProtocol(),it.getEndpointCount()));
            for(int e=0;e<it.getEndpointCount();e++) {
                UsbEndpoint ep=it.getEndpoint(e);
                appendMeta(String.format(Locale.US,"ENDPOINT iface=%d n=%d addr=0x%02X dir=%s type=%d maxPacket=%d interval=%d",it.getId(),e,ep.getAddress(),ep.getDirection()==UsbConstants.USB_DIR_IN?"IN":"OUT",ep.getType(),ep.getMaxPacketSize(),ep.getInterval()));
                if(ep.getDirection()==UsbConstants.USB_DIR_IN && ep.getType()==UsbConstants.USB_ENDPOINT_XFER_INT && in==null) { best=it; in=ep; }
            }
        }
        if(best==null || in==null){ c.close(); setStatus("No interrupt-IN HID endpoint found. Export TXT."); appendMeta("ERROR no interrupt IN endpoint"); return; }
        if(!c.claimInterface(best,true)){ c.close(); setStatus("claimInterface() failed. Export TXT."); appendMeta("ERROR claimInterface failed id="+best.getId()); return; }
        connection=c; hidInterface=best; inEndpoint=in; device=d;
        setStatus("USB connected. Authenticating Speed Editor...");
        new Thread(() -> {
            boolean ok=authenticate();
            if(!ok){ setStatus("Authentication failed. Raw descriptor data is still exportable."); return; }
            setStatus("AUTH OK. Known HID map loaded; no full calibration is required.");
            selectJogMode(KEY_SHTL);
            startReader();
        },"SpeedEditorAuth").start();
    }

    private boolean authenticate() {
        try {
            byte[] x=new byte[10]; x[0]=6;
            if(setFeature(new byte[]{6,0,0,0,0,0,0,0,0,0})<0) throw new Exception("reset SET_FEATURE failed");
            byte[] ch=getFeature();
            if(ch==null || ch[0]!=6 || ch[1]!=0) throw new Exception("challenge header="+hex(ch));
            long challenge=le64(ch,2); appendMeta(String.format(Locale.US,"AUTH challenge=0x%016X",challenge));
            if(setFeature(new byte[]{6,1,0,0,0,0,0,0,0,0})<0) throw new Exception("app challenge failed");
            byte[] respKbd=getFeature();
            if(respKbd==null || respKbd[0]!=6 || respKbd[1]!=2) throw new Exception("kbd response header="+hex(respKbd));
            long response=bmdAuth(challenge);
            byte[] out=new byte[10]; out[0]=6; out[1]=3; putLe64(out,2,response);
            if(setFeature(out)<0) throw new Exception("response SET_FEATURE failed");
            byte[] st=getFeature();
            if(st==null || st[0]!=6 || st[1]!=4) throw new Exception("status header="+hex(st));
            authTimeoutSec=(st[2]&255)|((st[3]&255)<<8); authAtMs=SystemClock.elapsedRealtime();
            appendMeta("AUTH OK timeoutSec="+authTimeoutSec+" status="+hex(st));
            return true;
        } catch(Exception ex) { appendMeta("AUTH ERROR "+ex); return false; }
    }
    private int setFeature(byte[] b) { return connection.controlTransfer(0x21,0x09,0x0306,hidInterface.getId(),b,b.length,1200); }
    private byte[] getFeature() {
        byte[] b=new byte[10]; b[0]=6;
        int n=connection.controlTransfer(0xA1,0x01,0x0306,hidInterface.getId(),b,b.length,1200);
        appendMeta("AUTH GET n="+n+" data="+hex(b));
        return n>0?b:null;
    }

    private void selectJogMode(int key) {
        if(connection==null || hidInterface==null) return;
        final int mode;
        final int ledBits;
        final String label;
        if(key==KEY_SHTL) {
            mode=JOG_RELATIVE_2; ledBits=(1<<1); label="SHTL / RELATIVE_2";
        } else if(key==KEY_JOG) {
            mode=JOG_ABSOLUTE_CONTINUOUS; ledBits=(1<<0); label="JOG / ABSOLUTE_CONTINUOUS";
        } else if(key==KEY_SCRL) {
            mode=JOG_ABSOLUTE_DEADZERO; ledBits=(1<<2); label="SCRL / ABSOLUTE_DEADZERO";
        } else return;

        boolean ledOk=writeOutputReport(new byte[]{4,(byte)ledBits});
        boolean modeOk=writeOutputReport(new byte[]{3,(byte)mode,0,0,0,0,(byte)0xff});
        appendMeta("SET_JOG "+label+" ledOk="+ledOk+" modeOk="+modeOk);
        runOnUiThread(() -> lastView.setText("MODE SELECTED: "+label+"\nLED write="+ledOk+" / MODE write="+modeOk));
    }

    private boolean writeOutputReport(byte[] report) {
        if(connection==null || hidInterface==null || report==null || report.length==0) return false;
        int reportId=report[0]&255;
        int n=connection.controlTransfer(0x21,0x09,0x0200|reportId,hidInterface.getId(),report,report.length,1000);
        appendMeta("OUT SET_REPORT id="+reportId+" n="+n+" data="+hex(report));
        return n==report.length;
    }

    private static String jogModeName(int mode) {
        switch(mode) {
            case 0: return "RELATIVE_0";
            case 1: return "ABSOLUTE_CONTINUOUS";
            case 2: return "RELATIVE_2";
            case 3: return "ABSOLUTE_DEADZERO";
            default: return "UNKNOWN_MODE";
        }
    }

    private void startReader() {
        if(running.getAndSet(true)) return;
        readerThread=new Thread(() -> {
            UsbRequest req=new UsbRequest();
            if(!req.initialize(connection,inEndpoint)){ appendMeta("ERROR UsbRequest.initialize=false"); setStatus("UsbRequest initialization failed."); running.set(false); return; }
            int cap=Math.max(64,inEndpoint.getMaxPacketSize());
            ByteBuffer buf=ByteBuffer.allocateDirect(cap);
            try {
                while(running.get() && connection!=null) {
                    if(authTimeoutSec>30 && SystemClock.elapsedRealtime()-authAtMs > (authTimeoutSec-20L)*1000L) {
                        appendMeta("AUTH refresh"); authenticate();
                    }
                    buf.clear();
                    if(!req.queue(buf,cap)) { appendMeta("ERROR UsbRequest.queue=false"); break; }
                    UsbRequest done=connection.requestWait();
                    if(done==null) { appendMeta("ERROR requestWait=null"); break; }
                    int n=buf.position();
                    if(n<=0) n=Math.min(cap,64);
                    byte[] data=new byte[n]; buf.rewind(); buf.get(data,0,n);
                    int actual=expectedReportLength(data);
                    if(actual>0 && actual<data.length) data=Arrays.copyOf(data,actual);
                    handleReport(data);
                }
            } catch(Throwable t) { appendMeta("READER ERROR "+t); }
            try{req.close();}catch(Exception ignored){}
            running.set(false);
        },"SpeedEditorReader"); readerThread.start();
    }

    private int expectedReportLength(byte[] d) {
        if(d==null || d.length==0) return 0;
        switch(d[0]&255){ case 3:return 7; case 4:return 13; case 7:return 3; default:return 0; }
    }

    private void handleReport(byte[] d) {
        long t=SystemClock.elapsedRealtime();
        if(d==null || d.length==0) return;
        int id=d[0]&255;
        String parsed="";
        if(id==4 && d.length>=13) {
            Set<Integer> held=new LinkedHashSet<>();
            for(int i=0;i<6;i++){ int p=1+i*2; int k=(d[p]&255)|((d[p+1]&255)<<8); if(k!=0) held.add(k); }
            parsed="KEYS "+keySetText(held);
            Set<Integer> newly=new LinkedHashSet<>(held); newly.removeAll(lastHeld);
            for(Integer k:newly) onKeyPressed(k);
            Set<Integer> released=new LinkedHashSet<>(lastHeld); released.removeAll(held);
            for(Integer k:released) {
                if(k==KEY_SHTL || k==KEY_JOG || k==KEY_SCRL) selectJogMode(k);
            }
            lastHeld=held;
        } else if(id==3 && d.length>=7) {
            int mode=d[1]&255;
            int value=ByteBuffer.wrap(d,2,4).order(ByteOrder.LITTLE_ENDIAN).getInt();
            parsed="DIAL mode="+mode+" value="+value+" unknown="+(d[6]&255);
            onDialEvent(mode,value);
        } else if(id==7 && d.length>=3) {
            parsed="BATTERY charging="+(d[1]&255)+" level="+(d[2]&255);
        } else parsed="UNKNOWN reportId="+id;
        appendRaw(t,d,parsed);
        String p=parsed+"\nRAW "+hex(d);
        runOnUiThread(()-> lastView.setText(p));
    }

    private void onKeyPressed(int code) {
        String name=keyLabel(code);
        appendMeta("KNOWN_KEY_DOWN code=0x"+String.format(Locale.US,"%02X",code)+" name="+name);
        runOnUiThread(() -> stepView.setText((name.equals("UNKNOWN")?"UNKNOWN KEY":"KEY: "+name)+"\n0x"+String.format(Locale.US,"%02X",code)));
    }
    private void onDialEvent(int mode,int value) {
        dialEventsThisStep++;
        runOnUiThread(() -> {
            String modeName=jogModeName(mode);
            stepView.setText("SEARCH DIAL\n"+modeName+" ("+mode+")  value="+value);
            progressView.setText(progressText()+"  dialReports="+dialEventsThisStep);
        });
    }

    private synchronized Step currentStep(){ return stepIndex>=0 && stepIndex<steps.size()?steps.get(stepIndex):null; }
    private synchronized void completeCurrent(String result,String detail) {
        if(stepIndex>=steps.size()) return;
        StepResult r=new StepResult(); r.index=stepIndex+1; r.prompt=steps.get(stepIndex).prompt; r.result=result; r.elapsedMs=SystemClock.elapsedRealtime()-stepStartMs; r.detail=detail;
        results.add(r); appendMeta("STEP "+r.index+" "+result+" "+r.prompt+" "+detail+" elapsedMs="+r.elapsedMs);
        stepIndex++; stepStartMs=SystemClock.elapsedRealtime(); dialEventsThisStep=0;
        if(stepIndex>=steps.size()) { appendMeta("GUIDED TEST COMPLETE"); setStatus("TEST COMPLETE. Export TXT and attach it to ChatGPT."); }
    }
    private void manualNext(){ Step s=currentStep(); if(s!=null && s.dial && dialEventsThisStep>0){ completeCurrent("PASS","dialReports="+dialEventsThisStep); refreshStepUi(); } }
    private void skipStep(){ if(currentStep()!=null){ completeCurrent("SKIP","user skipped"); refreshStepUi(); } }
    private void backStep(){
        synchronized(this){ if(stepIndex<=0)return; stepIndex--; if(!results.isEmpty() && results.get(results.size()-1).index==stepIndex+1) results.remove(results.size()-1); stepStartMs=SystemClock.elapsedRealtime(); dialEventsThisStep=0; }
        appendMeta("STEP BACK to="+(stepIndex+1)); refreshStepUi();
    }
    private void resetSession(){
        synchronized(this){ stepIndex=0; results.clear(); stepStartMs=SystemClock.elapsedRealtime(); dialEventsThisStep=0; lastHeld.clear(); }
        synchronized(logLock){ rawLog.setLength(0); }
        appendMeta("SESSION START app=0.2-known-hid sdk="+Build.VERSION.SDK_INT+" model="+Build.MANUFACTURER+" "+Build.MODEL+" androidId="+Settings.Secure.getString(getContentResolver(),Settings.Secure.ANDROID_ID));
        refreshStepUi();
    }
    private String progressText(){ return "KNOWN MAP: 43 KEYS / REPORTS 3,4,7"; }
    private void refreshStepUi(){
        runOnUiThread(()->{
            progressView.setText(progressText());
            stepView.setText("PRESS ANY SPEED EDITOR KEY\nor turn Search Dial after selecting a mode");
            nextButton.setEnabled(true);
            backButton.setEnabled(true); skipButton.setEnabled(true); exportButton.setEnabled(true);
        });
    }

    private String buildReport() {
        StringBuilder o=new StringBuilder(256*1024);
        o.append("SPEED_EDITOR_KNOWN_HID_CHECK v0.2\n");
        o.append("Generated: ").append(new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS Z",Locale.US).format(new Date())).append('\n');
        o.append(String.format(Locale.US,"Expected device: VID=0x%04X PID=0x%04X\n",USB_VID,USB_PID));
        o.append("Android: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append(" SDK ").append(Build.VERSION.SDK_INT).append('\n');
        o.append("Auth timeout sec: ").append(authTimeoutSec).append("\n\n");
        o.append("=== VALIDATION MODE ===\n");
        o.append("No 49-step calibration is required. Known mappings are decoded live.\n");
        o.append("Expected dial mapping: SHTL=2 RELATIVE_2, JOG=1 ABSOLUTE_CONTINUOUS, SCRL=3 ABSOLUTE_DEADZERO\n");
        o.append("\n=== EXPECTED KEY MAP ===\n");
        for(KeyDef k:KEYS) o.append(String.format(Locale.US,"0x%02X\t%s\n",k.code,k.label));
        o.append("\n=== RAW / META LOG ===\n");
        synchronized(logLock){o.append(rawLog);}
        return o.toString();
    }

    private void exportTxt(){
        Intent i=new Intent(Intent.ACTION_CREATE_DOCUMENT); i.addCategory(Intent.CATEGORY_OPENABLE); i.setType("text/plain");
        String fn="SpeedEditorProbe_"+new SimpleDateFormat("yyyyMMdd_HHmmss",Locale.US).format(new Date())+".txt"; i.putExtra(Intent.EXTRA_TITLE,fn);
        startActivityForResult(i,REQ_EXPORT);
    }
    @Override protected void onActivityResult(int req,int res,Intent data){
        super.onActivityResult(req,res,data);
        if(req==REQ_EXPORT && res==RESULT_OK && data!=null && data.getData()!=null){
            Uri u=data.getData(); try(OutputStream os=getContentResolver().openOutputStream(u)){ os.write(buildReport().getBytes(java.nio.charset.StandardCharsets.UTF_8)); os.flush(); setStatus("TXT exported. Attach that file to ChatGPT."); appendMeta("EXPORT OK uri="+u); }
            catch(Exception e){ setStatus("Export failed: "+e); appendMeta("EXPORT ERROR "+e); }
        }
    }

    private void closeUsb(){ running.set(false); UsbDeviceConnection c=connection; connection=null; if(c!=null){ try{ if(hidInterface!=null)c.releaseInterface(hidInterface);}catch(Exception ignored){} try{c.close();}catch(Exception ignored){} } hidInterface=null; inEndpoint=null; }
    private void setStatus(String s){ runOnUiThread(()->statusView.setText(s)); }
    private void appendMeta(String s){ synchronized(logLock){ rawLog.append(String.format(Locale.US,"%d\tMETA\t%s\n",SystemClock.elapsedRealtime(),s)); } }
    private void appendRaw(long t,byte[] d,String parsed){ synchronized(logLock){ rawLog.append(t).append("\tRAW\t").append(hex(d)).append("\t").append(parsed).append('\n'); } }
    private static String describeDevice(UsbDevice d){ if(d==null)return"null"; return String.format(Locale.US,"name=%s id=%d vid=0x%04X pid=0x%04X class=%d subclass=%d protocol=%d interfaces=%d",d.getDeviceName(),d.getDeviceId(),d.getVendorId(),d.getProductId(),d.getDeviceClass(),d.getDeviceSubclass(),d.getDeviceProtocol(),d.getInterfaceCount()); }
    private static String hex(byte[] b){ if(b==null)return"null"; StringBuilder s=new StringBuilder(); for(byte x:b)s.append(String.format(Locale.US,"%02X",x&255)); return s.toString(); }
    private static String keySetText(Set<Integer> ks){ StringBuilder s=new StringBuilder("["); boolean f=true; for(int k:ks){if(!f)s.append(", "); f=false; s.append(String.format(Locale.US,"0x%02X:%s",k,keyLabel(k)));} return s.append(']').toString(); }
    private static String keyLabel(int c){ for(KeyDef k:KEYS)if(k.code==c)return k.label; return "UNKNOWN"; }
    private static long le64(byte[] b,int off){ long v=0; for(int i=0;i<8;i++)v|=((long)b[off+i]&255L)<<(8*i); return v; }
    private static void putLe64(byte[] b,int off,long v){ for(int i=0;i<8;i++)b[off+i]=(byte)(v>>>(8*i)); }
    private static long rol8(long v){ return (v<<56)|(v>>>8); }
    private static long bmdAuth(long challenge){
        final long[] even={0x3ae1206f97c10bc8L,0x2a9ab32bebf244c6L,0x20a6f8b8df9adf0aL,0xaf80ece52cfc1719L,0xec2ee2f7414fd151L,0xb055adfd73344a15L,0xa63d2e3059001187L,0x751bf623f42e0ddeL};
        final long[] odd={0x3e22b34f502e7fdeL,0x24656b981875ab1cL,0xa17f3456df7bf8c3L,0x6df72e1941aef698L,0x72226f011e66ab94L,0x3831a3c606296b42L,0xfd7ff81881332c89L,0x61a3f6474ff236c6L};
        final long mask=0xa79a63f585d37bf0L; int n=(int)(challenge&7L); long v=challenge; for(int i=0;i<n;i++)v=rol8(v); long k;
        if((v&1L)==((0x78L>>>n)&1L)) k=even[n]; else {v^=rol8(v); k=odd[n];} return v^(rol8(v)&mask)^k;
    }
}