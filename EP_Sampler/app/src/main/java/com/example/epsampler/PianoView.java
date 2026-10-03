package com.example.epsampler;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

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
    }

    private ActionListener actionListener;
    private final Bitmap background;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final boolean[] held = new boolean[128];
    private final int[] velocities = new int[128];
    private final int[] polyPressure = new int[128];
    private int channelPressure = 0;
    private int cc1 = 14, cc7 = 127, cc10 = 74, cc11 = 74, cc64 = 0, cc74 = 42, cc103 = 36, cc104 = 36;
    private int pitchBend = 8192;
    private int midiConnections = 0;
    private boolean dreamy = true;
    private boolean tape = false;
    private int boosterStep = 0;
    private int boostDb = 0;
    private int spaceMode = 0;
    private String bankStatus = "MODEL READY";
    private int downButton = -1;
    private long downTimeMs = 0L;

    private final Finger[] left = new Finger[5];
    private final Finger[] right = new Finger[5];
    private RectF imageRect = new RectF();

    // Coordinates measured from the supplied 1191 x 896 reference image.
    private static final float SRC_W = 1191f, SRC_H = 896f;
    private static final RectF KEYBOARD = new RectF(151f, 472f, 1078f, 606f);

    PianoView(Context context) {
        super(context);
        setKeepScreenOn(true);
        setBackgroundColor(Color.BLACK);
        background = BitmapFactory.decodeResource(getResources(), R.drawable.piano_reference);
        text.setTypeface(android.graphics.Typeface.create("sans", android.graphics.Typeface.NORMAL));
        for (int i = 0; i < 5; i++) { left[i] = new Finger(); right[i] = new Finger(); }
    }

    void setActionListener(ActionListener l) { actionListener = l; }
    void setDreamy(boolean on) { dreamy = on; invalidate(); }
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
    void setMidiConnections(int count) { midiConnections = count; invalidate(); }
    void setChannelPressure(int v) { channelPressure = clamp7(v); invalidate(); }
    void setPitchBend(int v) { pitchBend = Math.max(0, Math.min(16383, v)); invalidate(); }

    void noteOn(int note, int velocity) {
        if (note < 0 || note > 127) return;
        held[note] = true; velocities[note] = clamp7(velocity);
        invalidate();
    }
    void noteOff(int note) {
        if (note < 0 || note > 127) return;
        held[note] = false; polyPressure[note] = 0;
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
        invalidate();
    }

    private static int clamp7(int v) { return Math.max(0, Math.min(127, v)); }

    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        float scale = Math.min(getWidth() / SRC_W, getHeight() / SRC_H);
        float dw = SRC_W * scale, dh = SRC_H * scale;
        float leftPad = (getWidth() - dw) * 0.5f;
        float topPad = (getHeight() - dh) * 0.5f;
        imageRect.set(leftPad, topPad, leftPad + dw, topPad + dh);
        // Reset shared Paint before drawing the background. Its alpha/color are reused by
        // overlays later in the frame; without this reset the next MIDI redraw darkens it.
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.WHITE);
        paint.setAlpha(255);
        c.drawBitmap(background, null, imageRect, paint);

        drawPressedKeys(c, scale, leftPad, topPad);
        drawIndicators(c);
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
                "MODEL",
                "CONFIG"
        };
        float gap = 6f*u;
        float bx0 = pad;
        float by0 = 72f*u;
        float bh = 30f*u;
        float bw = (getWidth() - pad*2f - gap*5f) / 6f;
        text.setTextSize(13.5f*u);
        for (int i=0;i<6;i++) {
            float l = bx0 + i*(bw+gap);
            float rr = l + bw;
            boolean active = (i==0 && boostDb>0) || (i==1 && spaceMode>0) ||
                    (i==2 && tape) || (i==3 && dreamy);
            paint.setColor(active ? Color.argb(120, 238, 229, 207) : Color.argb(72, 238, 229, 207));
            c.drawRoundRect(new RectF(l, by0, rr, by0+bh), 6f*u, 6f*u, paint);
            text.setColor(active ? Color.rgb(24,24,22) : Color.argb(235,244,237,224));
            float tw = text.measureText(labels[i]);
            c.drawText(labels[i], l + (bw-tw)*0.5f, by0 + 20f*u, text);
        }
        text.setColor(Color.argb(235, 244, 237, 224));
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

    private int effectButtonAt(float x, float y) {
        float u = Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
        float pad = 12f*u;
        float gap = 6f*u;
        float by0 = 72f*u;
        float bh = 30f*u;
        if (y < by0 || y > by0 + bh) return -1;
        float bw = (getWidth() - pad*2f - gap*5f) / 6f;
        for (int i=0;i<6;i++) {
            float l = pad + i*(bw+gap);
            if (x >= l && x <= l+bw) return i;
        }
        return -1;
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        int index = effectButtonAt(e.getX(), e.getY());
        if (e.getAction() == MotionEvent.ACTION_DOWN) {
            downButton = index;
            downTimeMs = android.os.SystemClock.uptimeMillis();
            return true;
        }
        if (e.getAction() == MotionEvent.ACTION_CANCEL) {
            downButton = -1;
            return true;
        }
        if (e.getAction() != MotionEvent.ACTION_UP) return true;
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
        else if (index==4) actionListener.onChooseBank();
        else actionListener.onOpenConfig();
        return true;
    }

    private static final class Finger {
        float x, y, targetX, targetY;
        boolean initialized = false;
        boolean active = false;
    }
}
