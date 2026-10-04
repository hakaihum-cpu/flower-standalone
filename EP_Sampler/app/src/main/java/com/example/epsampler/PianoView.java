package com.example.epsampler;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;
import android.util.SparseIntArray;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;

public final class PianoView extends View {
    static final int EFFECT_BOOST = 0;
    static final int EFFECT_SPACE = 1;
    static final int EFFECT_TAPE = 2;
    static final int EFFECT_DREAMY = 3;

    interface ActionListener {
        void onCycleBooster();
        void onCycleSpace();
        void onToggleTape();
        void onToggleDreamy();
        void onChooseBank();
        void onEditEffect(int effect);
        void onOpenConfig();
        void onRecorderRecord();
        void onRecorderClear();
        void onRecorderRandom();
        void onRecorderClock();
        void onRecorderBpm(int bpm);
        void onRecorderTile(int slot);
    }

    private ActionListener actionListener;
    private PerformanceVideoLayer performanceVideoLayer;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final boolean[] held = new boolean[128];
    private final int[] velocities = new int[128];
    private final int[] polyPressure = new int[128];
    private int channelPressure = 0;
    private int cc1 = 14, cc7 = 127, cc10 = 74, cc11 = 74, cc64 = 0, cc74 = 42, cc103 = 36, cc104 = 36;
    private int pitchBend = 8192;
    private int midiConnections = 0;
    private boolean dreamy = false;
    private boolean tape = false;
    private int boosterStep = 0;
    private int boostDb = 0;
    private int spaceMode = 0;
    private String bankStatus = "VIOLIN READY CH1";
    private String instrumentButtonLabel = "VIOLIN";
    private int instrumentMidiChannel = 1;
    private int downButton = -1;
    private long downTimeMs = 0L;

    private boolean recorderOpen = false;
    private float recorderSlide = 0f;
    private boolean recorderBpmDragging = false;
    private long recorderLastPollMs = 0L;
    private boolean recRecording = false, recRandom = false, recMidiClock = false;
    private int recRecordingSlot = -1, recBpm = 120;
    private float recInputLevel = 0f;
    private final int[] recValid = new int[4];
    private final boolean[] recPlaying = new boolean[4];
    private final float[] recProgress = new float[4];
    private final float[][] recPeaks = new float[4][64];

    // Temporary audition keyboard for the physical-model branch.
    // Intentionally contained in this View so it can be removed cleanly later.
    private boolean keyOverlayVisible = false;
    private final SparseIntArray touchNotes = new SparseIntArray();
    private static final int AUDITION_LOW_NOTE = 55;   // G3
    private static final int AUDITION_HIGH_NOTE = 96;  // C7

    private final Finger[] left = new Finger[5];
    private final Finger[] right = new Finger[5];
    private RectF imageRect = new RectF();

    // Coordinates measured from the supplied 1191 x 896 reference image.
    private static final float SRC_W = 1191f, SRC_H = 896f;
    private static final RectF KEYBOARD = new RectF(151f, 472f, 1078f, 606f);

    PianoView(Context context) {
        super(context);
        setKeepScreenOn(true);
        setBackgroundColor(Color.TRANSPARENT);
        text.setTypeface(android.graphics.Typeface.create("sans", android.graphics.Typeface.NORMAL));
        for (int i = 0; i < 5; i++) { left[i] = new Finger(); right[i] = new Finger(); }
    }

    void setActionListener(ActionListener l) { actionListener = l; }
    void setPerformanceVideoLayer(PerformanceVideoLayer layer) { performanceVideoLayer = layer; }
    void setDreamy(boolean on) {
        dreamy = on;
        if (performanceVideoLayer != null) performanceVideoLayer.setDreamy(on);
        invalidate();
    }
    void setTape(boolean on) { tape = on; invalidate(); }
    void setBoosterStep(int step) {
        boosterStep = Math.max(0, Math.min(3, step));
        boostDb = boosterStep * 2;
        invalidate();
    }
    void setBoostDb(int db) {
        boostDb = Math.max(0, Math.min(6, db));
        boosterStep = Math.min(3, Math.round(boostDb / 2f));
        invalidate();
    }
    void setSpaceMode(int mode) { spaceMode = Math.max(0, Math.min(3, mode)); invalidate(); }
    void setBankStatus(String s) { bankStatus = s; invalidate(); }
    void toggleRecorderDrawer() { setRecorderOpen(!recorderOpen); }
    void setRecorderOpen(boolean open) {
        recorderOpen = open;
        recorderBpmDragging = false;
        postInvalidateOnAnimation();
    }
    boolean isRecorderOpen() { return recorderOpen; }
    void setInstrumentName(String fullName, String buttonLabel) {
        instrumentButtonLabel = buttonLabel == null ? "MODEL" : buttonLabel;
        String channel = instrumentMidiChannel <= 0 ? "OFF" : "CH" + instrumentMidiChannel;
        bankStatus = (fullName == null ? "MODEL" : fullName) + " READY " + channel;
        invalidate();
    }

    void setInstrumentMidiChannel(int channel) {
        instrumentMidiChannel = Math.max(0, Math.min(16, channel));
        String prefix = bankStatus;
        int ready = prefix.indexOf(" READY");
        if (ready >= 0) prefix = prefix.substring(0, ready);
        String ch = instrumentMidiChannel <= 0 ? "OFF" : "CH" + instrumentMidiChannel;
        bankStatus = prefix + " READY " + ch;
        invalidate();
    }
    void setMidiConnections(int count) { midiConnections = count; invalidate(); }
    void setChannelPressure(int v) { channelPressure = clamp7(v); invalidate(); }
    void setPitchBend(int v) { pitchBend = Math.max(0, Math.min(16383, v)); invalidate(); }

    void noteOn(int note, int velocity) {
        if (note < 0 || note > 127) return;
        held[note] = true; velocities[note] = clamp7(velocity);
        if (performanceVideoLayer != null) performanceVideoLayer.noteOn(note, velocity);
        invalidate();
    }
    void noteOff(int note) {
        if (note < 0 || note > 127) return;
        held[note] = false; polyPressure[note] = 0;
        if (performanceVideoLayer != null) performanceVideoLayer.noteOff(note);
        invalidate();
    }
    void polyPressure(int note, int value) {
        if (note >= 0 && note < 128) polyPressure[note] = clamp7(value);
        invalidate();
    }
    void controlChange(int cc, int value) {
        if (cc == 1) cc1 = clamp7(value);
        else if (cc == 7) cc7 = clamp7(value);
        else if (cc == 10) cc10 = clamp7(value);
        else if (cc == 11) cc11 = clamp7(value);
        else if (cc == 64) cc64 = clamp7(value);
        else if (cc == 74) cc74 = clamp7(value);
        else if (cc == 103) cc103 = clamp7(value);
        else if (cc == 104) cc104 = clamp7(value);
        if ((cc == 103 || cc == 104) && performanceVideoLayer != null) {
            performanceVideoLayer.setDreamyXY(cc103, cc104);
        }
        invalidate();
    }

    private static int clamp7(int v) { return Math.max(0, Math.min(127, v)); }

    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        // The original MP4 is rendered by PerformanceVideoLayer underneath.
        // Keep this view transparent so only the controls / audition keyboard
        // sit above the full-quality video.
        drawIndicators(c);
        if (keyOverlayVisible) drawAuditionKeyboard(c);
        drawRecorderDrawer(c);
    }

    private void drawPressedKeys(Canvas c, float s, float ox, float oy) {
        for (int note = 21; note <= 108; note++) {
            if (!held[note]) continue;
            RectF r = keyRect(note);
            if (r == null) continue;
            r = new RectF(ox + r.left*s, oy + r.top*s, ox + r.right*s, oy + r.bottom*s);
            int a = 34 + Math.round(velocities[note] / 127f * 60f);
            paint.setColor(Color.argb(a, 255, 245, 215));
            c.drawRoundRect(r, 2f*s, 2f*s, paint);
        }
    }

    private RectF keyRect(int note) {
        if (note < 21 || note > 108) return null;
        boolean black = isBlack(note);
        int whiteIndex = whiteIndex(note);
        float whiteW = KEYBOARD.width() / 52f;
        if (!black) {
            float x = KEYBOARD.left + whiteIndex * whiteW;
            return new RectF(x + 1, KEYBOARD.top, x + whiteW - 1, KEYBOARD.bottom);
        }
        int prevWhite = whiteIndexBeforeBlack(note);
        float center = KEYBOARD.left + (prevWhite + 1) * whiteW;
        float bw = whiteW * 0.58f;
        return new RectF(center - bw/2, KEYBOARD.top, center + bw/2, KEYBOARD.top + KEYBOARD.height()*0.57f);
    }

    private static boolean isBlack(int note) {
        int pc = note % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    private static int whiteIndex(int note) {
        int idx = 0;
        for (int n = 21; n < note; n++) if (!isBlack(n)) idx++;
        return idx;
    }

    private static int whiteIndexBeforeBlack(int note) {
        int idx = -1;
        for (int n = 21; n < note; n++) if (!isBlack(n)) idx++;
        return Math.max(0, idx);
    }

    private float keyCenterX(int note) {
        RectF r = keyRect(note);
        return r == null ? KEYBOARD.centerX() : r.centerX();
    }

    private void assignHands() {
        List<Integer> active = new ArrayList<>();
        for (int n = 21; n <= 108; n++) if (held[n]) active.add(n);
        active.sort(Comparator.naturalOrder());

        List<Integer> l = new ArrayList<>(), r = new ArrayList<>();
        for (int n : active) {
            if (n <= 60) l.add(n); else r.add(n);
        }
        // If one side has too many notes, keep the five nearest to its natural range.
        if (l.size() > 5) l = new ArrayList<>(l.subList(Math.max(0, l.size()-5), l.size()));
        if (r.size() > 5) r = new ArrayList<>(r.subList(0, 5));
        targetHand(left, l, true);
        targetHand(right, r, false);
    }

    private void targetHand(Finger[] fingers, List<Integer> notes, boolean isLeft) {
        float neutralX = isLeft ? 474f : 720f;
        float neutralY = 568f;
        for (int i = 0; i < fingers.length; i++) {
            Finger f = fingers[i];
            float spread = (i - 2) * 14f;
            f.targetX = neutralX + spread;
            f.targetY = neutralY - (i == 0 || i == 4 ? 5 : 14);
            f.active = false;
        }
        int[][] slots = { {}, {2}, {1,3}, {1,2,3}, {0,1,3,4}, {0,1,2,3,4} };
        int count = Math.min(5, notes.size());
        if (count > 0) {
            int[] use = slots[count];
            for (int j = 0; j < count; j++) {
                int fingerIndex = use[j];
                int note = notes.get(j);
                Finger f = fingers[fingerIndex];
                f.targetX = keyCenterX(note);
                RectF kr = keyRect(note);
                f.targetY = (kr == null) ? neutralY : (isBlack(note) ? kr.bottom - 5 : kr.top + kr.height()*0.68f);
                f.active = held[note];
            }
        }
        for (Finger f : fingers) {
            if (!f.initialized) { f.x = f.targetX; f.y = f.targetY; f.initialized = true; }
        }
    }

    private void drawHands(Canvas c, float s, float ox, float oy) {
        boolean animating = false;
        animating |= drawHand(c, left, true, s, ox, oy);
        animating |= drawHand(c, right, false, s, ox, oy);
        if (animating) postInvalidateOnAnimation();
    }

    private boolean drawHand(Canvas c, Finger[] fingers, boolean isLeft, float s, float ox, float oy) {
        float palmX = 0f, palmY = 0f;
        boolean moving = false;
        for (Finger f : fingers) {
            float dx = f.targetX - f.x, dy = f.targetY - f.y;
            f.x += dx * 0.16f; f.y += dy * 0.16f;
            if (Math.abs(dx) + Math.abs(dy) > 0.8f) moving = true;
            palmX += f.x; palmY += f.y;
        }
        palmX /= 5f;
        palmY = palmY / 5f + 35f;

        paint.setStrokeWidth(Math.max(1.4f, 2.0f*s));
        paint.setStyle(Paint.Style.STROKE);
        paint.setColor(Color.argb(80, 248, 236, 210));
        for (int i = 0; i < 5; i++) {
            Finger f = fingers[i];
            float fx = ox + f.x*s, fy = oy + f.y*s;
            float px = ox + palmX*s, py = oy + palmY*s;
            float knuckleX = px + (fx-px)*0.50f + (i-2)*2.5f*s;
            float knuckleY = py + (fy-py)*0.48f;
            c.drawLine(px, py, knuckleX, knuckleY, paint);
            c.drawLine(knuckleX, knuckleY, fx, fy, paint);
            if (f.active) {
                paint.setStyle(Paint.Style.FILL);
                paint.setColor(Color.argb(115, 255, 244, 215));
                c.drawCircle(fx, fy, Math.max(2.5f, 4.0f*s), paint);
                paint.setStyle(Paint.Style.STROKE);
                paint.setColor(Color.argb(80, 248, 236, 210));
            }
        }
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(25, 248, 236, 210));
        c.drawOval(new RectF(ox+(palmX-25)*s, oy+(palmY-14)*s, ox+(palmX+25)*s, oy+(palmY+14)*s), paint);
        return moving;
    }

    private void drawIndicators(Canvas c) {
        float u = Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
        float pad = 12f*u;
        float panelTop = 7f*u;
        float panelBottom = 112f*u;

        paint.setColor(Color.argb(150, 0, 0, 0));
        c.drawRoundRect(new RectF(pad*0.45f, panelTop, getWidth()-pad*0.45f, panelBottom),
                10f*u, 10f*u, paint);

        text.setColor(Color.argb(235, 244, 237, 224));

        // Row 1: connection / bank / pitch. Roughly 2x the old text size.
        text.setTextSize(18f*u);
        float row1 = 30f*u;
        c.drawText(midiConnections > 0 ? "MIDI ●" : "MIDI ○", pad, row1, text);
        String modelShort = bankStatus == null ? "MODEL READY" : bankStatus;
        c.drawText(modelShort, pad + 95f*u, row1, text);
        float cents = (pitchBend - 8192) / 8192f * 200f;
        String pb = String.format(java.util.Locale.US, "PB %+3.0fc", cents);
        float pw = text.measureText(pb);
        c.drawText(pb, getWidth()-pad-pw, row1, text);

        // Row 2: performance meters, enlarged from the previous micro display.
        text.setTextSize(15.5f*u);
        float row2 = 57f*u;
        float barY = 61f*u;
        c.drawText("VEL", pad, row2, text);
        drawMicroBar(c, pad + 34f*u, barY-5f*u, maxHeldVelocity()/127f, 58f*u, 6f*u);
        c.drawText("AT", pad + 105f*u, row2, text);
        drawMicroBar(c, pad + 130f*u, barY-5f*u, Math.max(channelPressure,maxPolyPressure())/127f, 48f*u, 6f*u);
        c.drawText("VOL", pad + 191f*u, row2, text);
        drawMicroBar(c, pad + 229f*u, barY-5f*u, cc7/127f, 48f*u, 6f*u);
        c.drawText("BOW", pad + 290f*u, row2, text);
        drawMicroBar(c, pad + 329f*u, barY-5f*u, cc11/127f, 48f*u, 6f*u);
        c.drawText(cc64 >= 64 ? "SUS ●" : "SUS ○", pad + 390f*u, row2, text);

        // Row 3: direct-touch effect controls.
        String[] labels = new String[] {
                boostDb == 0 ? "BOOST OFF" : "BOOST +" + boostDb + "dB",
                "SPACE " + new String[]{"NONE","ROOM","HALL","SPACE"}[spaceMode],
                tape ? "TAPE ON" : "TAPE OFF",
                dreamy ? "DREAMY ON" : "DREAMY OFF",
                "RECORDER",
                instrumentButtonLabel,
                "CONFIG",
                keyOverlayVisible ? "KEY CLOSE" : "KEY"
        };
        float gap = 5f*u;
        float bx0 = pad;
        float by0 = 72f*u;
        float bh = 30f*u;
        float bw = (getWidth() - pad*2f - gap*7f) / 8f;
        text.setTextSize(12.3f*u);
        for (int i=0;i<8;i++) {
            float l = bx0 + i*(bw+gap);
            float rr = l + bw;
            boolean active = (i==0 && boostDb>0) || (i==1 && spaceMode>0) ||
                    (i==2 && tape) || (i==3 && dreamy) || (i==4 && recorderOpen) ||
                    (i==7 && keyOverlayVisible);
            paint.setColor(active ? Color.argb(120, 238, 229, 207) : Color.argb(72, 238, 229, 207));
            c.drawRoundRect(new RectF(l, by0, rr, by0+bh), 6f*u, 6f*u, paint);
            text.setColor(active ? Color.rgb(24,24,22) : Color.argb(235,244,237,224));
            float tw = text.measureText(labels[i]);
            c.drawText(labels[i], l + (bw-tw)*0.5f, by0 + 20f*u, text);
        }
        text.setColor(Color.argb(235, 244, 237, 224));
    }

    private void refreshRecorderState() {
        long now = android.os.SystemClock.uptimeMillis();
        if (now - recorderLastPollMs < 50L) return;
        recorderLastPollMs = now;
        recRecording = NativeEngine.recorderIsRecording();
        recRandom = NativeEngine.recorderIsRandom();
        recMidiClock = NativeEngine.recorderIsMidiClock();
        recBpm = NativeEngine.recorderBpm();
        recRecordingSlot = NativeEngine.recorderRecordingSlot();
        recInputLevel = NativeEngine.recorderInputLevel();
        for (int i=0;i<4;i++) {
            recValid[i] = NativeEngine.recorderValidSamples(i);
            recPlaying[i] = NativeEngine.recorderSlotPlaying(i);
            recProgress[i] = NativeEngine.recorderSlotProgress(i);
            float[] p = NativeEngine.recorderPeaks(i);
            if (p != null) System.arraycopy(p, 0, recPeaks[i], 0, Math.min(64, p.length));
        }
    }

    private float recorderDrawerHeight(float u) { return 270f*u; }
    private float recorderDrawerTop(float u) { return getHeight() - recorderDrawerHeight(u)*recorderSlide; }

    private RectF recorderTileRect(int slot, float u, float top) {
        float pad = 14f*u, gap = 8f*u;
        float w = (getWidth() - pad*2f - gap*3f) / 4f;
        float y = top + 48f*u;
        return new RectF(pad + slot*(w+gap), y, pad + slot*(w+gap) + w, y + 124f*u);
    }

    private RectF recorderControlRect(int index, float u, float top) {
        float pad = 14f*u, gap = 7f*u;
        float w = (getWidth() - pad*2f - gap*4f) / 5f;
        float y = top + 190f*u;
        return new RectF(pad + index*(w+gap), y, pad + index*(w+gap) + w, y + 34f*u);
    }

    private RectF recorderCloseRect(float u, float top) {
        return new RectF(getWidth()-94f*u, top+10f*u, getWidth()-14f*u, top+40f*u);
    }

    private void drawRecorderDrawer(Canvas c) {
        float target = recorderOpen ? 1f : 0f;
        if (Math.abs(target - recorderSlide) > 0.006f) {
            recorderSlide += (target - recorderSlide) * 0.22f;
            postInvalidateOnAnimation();
        } else {
            recorderSlide = target;
        }
        if (recorderSlide <= 0.001f) return;

        refreshRecorderState();
        float u = Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
        float top = recorderDrawerTop(u);

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(232, 8, 8, 8));
        c.drawRoundRect(new RectF(0, top, getWidth(), getHeight()+12f*u), 16f*u, 16f*u, paint);

        text.setColor(Color.argb(242, 244, 237, 224));
        text.setTextSize(18f*u);
        c.drawText("RECORDER", 14f*u, top+30f*u, text);

        RectF close = recorderCloseRect(u, top);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(1f, 1.5f*u));
        paint.setColor(Color.argb(150, 244, 237, 224));
        c.drawRoundRect(close, 5f*u, 5f*u, paint);
        text.setTextSize(12.5f*u);
        float ctw = text.measureText("CLOSE");
        c.drawText("CLOSE", close.centerX()-ctw/2f, close.centerY()+4.5f*u, text);

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(58, 244, 237, 224));
        c.drawRect(130f*u, top+22f*u, getWidth()-110f*u, top+25f*u, paint);
        paint.setColor(Color.argb(190, 244, 237, 224));
        c.drawRect(130f*u, top+21f*u,
                130f*u + (getWidth()-240f*u)*Math.max(0f, Math.min(1f, recInputLevel*1.8f)),
                top+26f*u, paint);

        for (int slot=0; slot<4; slot++) {
            RectF tr = recorderTileRect(slot, u, top);
            boolean recording = recRecordingSlot == slot;
            boolean playing = recPlaying[slot];
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(Color.argb(32, 244, 237, 224));
            c.drawRoundRect(tr, 6f*u, 6f*u, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth((recording || playing) ? 2.2f*u : 1f*u);
            paint.setColor(recording ? Color.rgb(225,72,64)
                    : Color.argb(playing ? 225 : 95, 244, 237, 224));
            c.drawRoundRect(tr, 6f*u, 6f*u, paint);

            text.setTextSize(12f*u);
            text.setColor(Color.argb(185, 244, 237, 224));
            c.drawText(String.valueOf(slot+1), tr.left+8f*u, tr.top+17f*u, text);

            if (recValid[slot] > 0) {
                float waveL=tr.left+7f*u, waveR=tr.right-7f*u;
                float cy=tr.centerY()+7f*u, half=tr.height()*0.31f;
                paint.setStrokeWidth(Math.max(1f, 1.15f*u));
                paint.setColor(Color.argb(205, 244, 237, 224));
                for (int i=0;i<64;i++) {
                    float x=waveL+(waveR-waveL)*i/63f;
                    float a=Math.max(0f,Math.min(1f,recPeaks[slot][i]*1.7f));
                    c.drawLine(x,cy-a*half,x,cy+a*half,paint);
                }
                if (playing) {
                    float px=waveL+(waveR-waveL)*Math.max(0f,Math.min(1f,recProgress[slot]));
                    paint.setColor(Color.WHITE);
                    paint.setStrokeWidth(2f*u);
                    c.drawLine(px,tr.top+27f*u,px,tr.bottom-8f*u,paint);
                }
            }
        }

        String[] controls = new String[] {
                recRecording ? "● REC" : "REC",
                "CLEAR",
                recRandom ? "RANDOM ●" : "RANDOM",
                recMidiClock ? "MIDI" : "INTERNAL",
                recMidiClock ? "BPM MIDI" : "BPM " + recBpm
        };
        for (int i=0;i<5;i++) {
            RectF br=recorderControlRect(i,u,top);
            boolean active=(i==0&&recRecording)||(i==2&&recRandom)||(i==3&&recMidiClock);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(active ? 2f*u : 1f*u);
            paint.setColor(active ? Color.argb(235,244,237,224) : Color.argb(100,244,237,224));
            c.drawRoundRect(br,5f*u,5f*u,paint);
            text.setTextSize(11.5f*u);
            text.setColor(Color.argb(active?245:205,244,237,224));
            float tw=text.measureText(controls[i]);
            c.drawText(controls[i],br.centerX()-tw/2f,br.centerY()+4f*u,text);
        }
        paint.setStyle(Paint.Style.FILL);

        if (recorderOpen || recRecording || recRandom) postInvalidateDelayed(50L);
    }

    private int recorderTargetAt(float x, float y) {
        if (recorderSlide < 0.55f) return -1;
        float u = Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
        float top = recorderDrawerTop(u);
        if (y < top) return -1;
        if (recorderCloseRect(u,top).contains(x,y)) return 100;
        for (int i=0;i<4;i++) if (recorderTileRect(i,u,top).contains(x,y)) return 110+i;
        for (int i=0;i<5;i++) if (recorderControlRect(i,u,top).contains(x,y)) return 120+i;
        return 109;
    }

    private void updateRecorderBpmFromX(float x) {
        if (actionListener == null || recMidiClock) return;
        float u = Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
        RectF r = recorderControlRect(4,u,recorderDrawerTop(u));
        float norm = Math.max(0f, Math.min(1f, (x-r.left)/Math.max(1f,r.width())));
        actionListener.onRecorderBpm(30 + Math.round(norm*270f));
    }

    private void drawMicroBar(Canvas c, float x, float y, float value, float w, float h) {
        value = Math.max(0f, Math.min(1f, value));
        paint.setColor(Color.argb(65, 235, 228, 215));
        c.drawRect(x, y, x+w, y+h, paint);
        paint.setColor(Color.argb(175, 245, 236, 217));
        c.drawRect(x, y, x+w*value, y+h, paint);
    }

    private int maxHeldVelocity() {
        int m = 0; for (int i=0;i<128;i++) if (held[i]) m = Math.max(m, velocities[i]); return m;
    }
    private int maxPolyPressure() {
        int m = 0; for (int v : polyPressure) m = Math.max(m, v); return m;
    }

    private void drawAuditionKeyboard(Canvas c) {
        float u = Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
        float top = Math.max(150f*u, getHeight() - 255f*u);
        float bottom = getHeight() - 8f*u;
        float left = 8f*u;
        float right = getWidth() - 8f*u;

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(218, 18, 18, 18));
        c.drawRoundRect(new RectF(left-4f*u, top-36f*u, right+4f*u, bottom+4f*u), 8f*u, 8f*u, paint);

        text.setTextSize(14f*u);
        text.setColor(Color.argb(240, 244, 237, 224));
        c.drawText("TEMP KEY  G3–C7", left, top-12f*u, text);

        int whiteCount = 0;
        for (int n=AUDITION_LOW_NOTE; n<=AUDITION_HIGH_NOTE; n++) if (!isBlack(n)) whiteCount++;
        float whiteW = (right-left) / Math.max(1, whiteCount);

        int wi = 0;
        for (int n=AUDITION_LOW_NOTE; n<=AUDITION_HIGH_NOTE; n++) {
            if (isBlack(n)) continue;
            float x0 = left + wi * whiteW;
            float x1 = x0 + whiteW;
            boolean active = held[n];
            paint.setColor(active ? Color.rgb(214, 204, 181) : Color.rgb(238, 234, 224));
            c.drawRect(x0, top, x1-1f*u, bottom, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(Math.max(1f, u));
            paint.setColor(Color.rgb(48,48,45));
            c.drawRect(x0, top, x1-1f*u, bottom, paint);
            paint.setStyle(Paint.Style.FILL);
            if (n % 12 == 0) {
                text.setTextSize(11f*u);
                text.setColor(Color.rgb(50,50,48));
                c.drawText("C" + ((n / 12) - 1), x0 + 4f*u, bottom - 8f*u, text);
            }
            wi++;
        }

        wi = 0;
        for (int n=AUDITION_LOW_NOTE; n<=AUDITION_HIGH_NOTE; n++) {
            if (!isBlack(n)) {
                wi++;
                continue;
            }
            float center = left + wi * whiteW;
            float bw = whiteW * 0.60f;
            RectF r = new RectF(center-bw/2f, top, center+bw/2f, top+(bottom-top)*0.60f);
            paint.setColor(held[n] ? Color.rgb(110,105,96) : Color.rgb(30,30,29));
            c.drawRoundRect(r, 2f*u, 2f*u, paint);
        }
    }

    private RectF auditionKeyboardRect() {
        float u = Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
        float top = Math.max(150f*u, getHeight() - 255f*u);
        return new RectF(8f*u, top, getWidth()-8f*u, getHeight()-8f*u);
    }

    private int auditionNoteAt(float x, float y) {
        if (!keyOverlayVisible) return -1;
        RectF area = auditionKeyboardRect();
        if (!area.contains(x,y)) return -1;

        int whiteCount = 0;
        for (int n=AUDITION_LOW_NOTE; n<=AUDITION_HIGH_NOTE; n++) if (!isBlack(n)) whiteCount++;
        float whiteW = area.width() / Math.max(1, whiteCount);
        float blackBottom = area.top + area.height()*0.60f;

        if (y <= blackBottom) {
            int wi = 0;
            for (int n=AUDITION_LOW_NOTE; n<=AUDITION_HIGH_NOTE; n++) {
                if (!isBlack(n)) {
                    wi++;
                    continue;
                }
                float center = area.left + wi * whiteW;
                float bw = whiteW * 0.60f;
                if (x >= center-bw/2f && x <= center+bw/2f) return n;
            }
        }

        int whiteIndex = Math.max(0, Math.min(whiteCount-1, (int)((x-area.left)/whiteW)));
        int wi = 0;
        for (int n=AUDITION_LOW_NOTE; n<=AUDITION_HIGH_NOTE; n++) {
            if (isBlack(n)) continue;
            if (wi == whiteIndex) return n;
            wi++;
        }
        return -1;
    }

    private void auditionNoteOn(int pointerId, int note) {
        if (note < 0) return;
        int old = touchNotes.get(pointerId, -1);
        if (old == note) return;
        if (old >= 0) auditionNoteOff(pointerId, old);
        touchNotes.put(pointerId, note);
        NativeEngine.noteOn(note, 104);
        noteOn(note, 104);
    }

    private void auditionNoteOff(int pointerId, int note) {
        if (note >= 0) {
            NativeEngine.noteOff(note, 0);
            noteOff(note);
        }
        touchNotes.delete(pointerId);
    }

    private void releaseAllAuditionNotes() {
        for (int i=touchNotes.size()-1; i>=0; i--) {
            int note = touchNotes.valueAt(i);
            NativeEngine.noteOff(note, 0);
            noteOff(note);
        }
        touchNotes.clear();
    }

    private void toggleAuditionKeyboard() {
        if (keyOverlayVisible) releaseAllAuditionNotes();
        keyOverlayVisible = !keyOverlayVisible;
        invalidate();
    }

    void clearForegroundForInstrumentSwitch() {
        // Release only touchscreen audition notes. External MIDI parts must
        // continue sounding when the foreground instrument is changed.
        releaseAllAuditionNotes();
        java.util.Arrays.fill(held, false);
        java.util.Arrays.fill(polyPressure, 0);
        if (performanceVideoLayer != null) performanceVideoLayer.allNotesOff();
        invalidate();
    }

    void panicAuditionKeyboard() {
        clearForegroundForInstrumentSwitch();
        NativeEngine.controlChange(123, 0);
    }

    @Override protected void onDetachedFromWindow() {
        panicAuditionKeyboard();
        super.onDetachedFromWindow();
    }

    boolean blocksPerformanceXY(float x, float y) {
        float u = Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
        if (y <= 112f*u) return true;
        if (recorderSlide > 0.05f && y >= recorderDrawerTop(u)) return true;
        if (keyOverlayVisible) {
            float keyTop = Math.max(150f*u, getHeight() - 255f*u) - 36f*u;
            if (y >= keyTop) return true;
        }
        return false;
    }

    private int effectButtonAt(float x, float y) {
        float u = Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
        float pad = 12f*u;
        float gap = 5f*u;
        float by0 = 72f*u;
        float bh = 30f*u;
        if (y < by0 || y > by0 + bh) return -1;
        float bw = (getWidth() - pad*2f - gap*7f) / 8f;
        for (int i=0;i<8;i++) {
            float l = pad + i*(bw+gap);
            if (x >= l && x <= l+bw) return i;
        }
        return -1;
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        final int action = e.getActionMasked();
        final int actionIndex = e.getActionIndex();

        int recorderTarget = recorderTargetAt(e.getX(actionIndex), e.getY(actionIndex));
        if (action == MotionEvent.ACTION_DOWN && recorderTarget >= 0) {
            downButton = recorderTarget;
            downTimeMs = android.os.SystemClock.uptimeMillis();
            recorderBpmDragging = (recorderTarget == 124 && !recMidiClock);
            if (recorderBpmDragging) updateRecorderBpmFromX(e.getX(actionIndex));
            return true;
        }
        if (action == MotionEvent.ACTION_MOVE && recorderBpmDragging) {
            updateRecorderBpmFromX(e.getX(actionIndex));
            return true;
        }
        if (action == MotionEvent.ACTION_UP && downButton >= 100) {
            int down = downButton;
            downButton = -1;
            recorderBpmDragging = false;
            if (actionListener == null) return true;
            if (down == 100) setRecorderOpen(false);
            else if (down >= 110 && down <= 113) actionListener.onRecorderTile(down-110);
            else if (down == 120) actionListener.onRecorderRecord();
            else if (down == 121) actionListener.onRecorderClear();
            else if (down == 122) actionListener.onRecorderRandom();
            else if (down == 123) actionListener.onRecorderClock();
            else if (down == 124) updateRecorderBpmFromX(e.getX(actionIndex));
            postInvalidateOnAnimation();
            return true;
        }

        if (keyOverlayVisible) {
            if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
                int pointerId = e.getPointerId(actionIndex);
                int note = auditionNoteAt(e.getX(actionIndex), e.getY(actionIndex));
                if (note >= 0) {
                    auditionNoteOn(pointerId, note);
                    return true;
                }
            } else if (action == MotionEvent.ACTION_MOVE) {
                for (int i=0; i<e.getPointerCount(); i++) {
                    int pointerId = e.getPointerId(i);
                    int old = touchNotes.get(pointerId, -1);
                    if (old < 0) continue;
                    int note = auditionNoteAt(e.getX(i), e.getY(i));
                    if (note != old) {
                        auditionNoteOff(pointerId, old);
                        if (note >= 0) auditionNoteOn(pointerId, note);
                    }
                }
                return true;
            } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
                int pointerId = e.getPointerId(actionIndex);
                int old = touchNotes.get(pointerId, -1);
                if (old >= 0) {
                    auditionNoteOff(pointerId, old);
                    return true;
                }
            } else if (action == MotionEvent.ACTION_CANCEL) {
                releaseAllAuditionNotes();
                downButton = -1;
                recorderBpmDragging = false;
                return true;
            }
        }

        int index = effectButtonAt(e.getX(actionIndex), e.getY(actionIndex));
        if (action == MotionEvent.ACTION_DOWN) {
            downButton = index;
            downTimeMs = android.os.SystemClock.uptimeMillis();
            return true;
        }
        if (action == MotionEvent.ACTION_CANCEL) {
            releaseAllAuditionNotes();
            downButton = -1;
            recorderBpmDragging = false;
            return true;
        }
        if (action != MotionEvent.ACTION_UP) return true;
        if (index < 0 || index != downButton || actionListener == null) {
            downButton = -1;
            return true;
        }
        long heldMs = android.os.SystemClock.uptimeMillis() - downTimeMs;
        downButton = -1;
        if (heldMs >= 550 && index <= EFFECT_DREAMY) {
            actionListener.onEditEffect(index);
            return true;
        }
        if (index==0) actionListener.onCycleBooster();
        else if (index==1) actionListener.onCycleSpace();
        else if (index==2) actionListener.onToggleTape();
        else if (index==3) actionListener.onToggleDreamy();
        else if (index==4) toggleRecorderDrawer();
        else if (index==5) actionListener.onChooseBank();
        else if (index==6) actionListener.onOpenConfig();
        else if (index==7) toggleAuditionKeyboard();
        return true;
    }

    private static final class Finger {
        float x, y, targetX, targetY;
        boolean initialized = false;
        boolean active = false;
    }
}
