package com.example.epsampler;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

final class SequencerView extends View {
    interface Listener {
        void onSequencerClose();
    }

    private static final int TRACKS = 8;
    private static final int BARS = 8;
    private static final int PPQN = 96;
    private static final int TICKS_PER_BAR = PPQN * 4;
    private static final int LOOP_TICKS = TICKS_PER_BAR * BARS;
    private static final int STEPS_PER_BAR = 16;
    private static final int STEP_TICKS = PPQN / 4;
    private static final int STEPS_PER_PAGE = 32;
    private static final int VISIBLE_NOTES = 24;

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Listener listener;

    private String[] partNames = new String[]{
            "VIOLIN","FLUTE","SAXOPHONE","FELT PIANO",
            "PIANICA / ACCORDION","XYLOPHONE","WOOD BASS","DRUMS",
            "EP-SAMPLE","SAMPLE 2","SAMPLE 3","SAMPLE 4",
            "SAMPLE 5","SAMPLE 6","SAMPLE 7","SAMPLE 8"
    };

    private int selectedTrack = 0;
    private int bpm = 120;
    private int playheadTick = 0;
    private boolean playing = false;
    private boolean recording = false;
    private boolean click = false;
    private int page = 0;
    private int lowNote = 48; // C3
    private final int[] trackParts = new int[]{0,1,2,3,4,5,6,7};
    private int[] noteData = new int[0];
    private long lastPollMs = 0L;
    private int downTarget = -1;

    private static final String[] CHORD_NAMES =
            new String[]{"MAJ","MIN","7","m7","SUS2","SUS4"};
    private boolean chordMode = false;
    private int chordType = 0;

    private float touchDownX = 0f;
    private float touchDownY = 0f;
    private boolean touchMoved = false;
    private int pressedNoteIndex = -1;
    private boolean resizeMode = false;
    private int resizeNoteIndex = -1;
    private int resizeStartTick = 0;
    private int resizeNote = 60;
    private int resizeOriginalDuration = STEP_TICKS;
    private int resizePreviewDuration = STEP_TICKS;

    private final Runnable noteLongPress = () -> {
        if (pressedNoteIndex >= 0 && !recording) {
            beginResize(pressedNoteIndex);
        }
    };

    SequencerView(Context context) {
        super(context);
        setBackgroundColor(Color.rgb(8,8,8));
        setVisibility(GONE);
        setClickable(true);
        setFocusable(true);
        text.setTypeface(android.graphics.Typeface.create(
                "sans", android.graphics.Typeface.NORMAL));
    }

    void setListener(Listener listener) { this.listener = listener; }

    void setPartNames(String[] names) {
        if (names == null || names.length < 16) return;
        partNames = java.util.Arrays.copyOf(names, 16);
        invalidate();
    }

    void open() {
        setVisibility(VISIBLE);
        bringToFront();
        refreshState(true);
        invalidate();
    }

    private float u() {
        return Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
    }

    private void refreshState(boolean force) {
        long now = android.os.SystemClock.uptimeMillis();
        if (!force && now - lastPollMs < 55L) return;
        lastPollMs = now;
        selectedTrack = clamp(NativeEngine.sequencerSelectedTrack(),0,TRACKS-1);
        bpm = clamp(NativeEngine.sequencerBpm(),40,240);
        playheadTick = clamp(NativeEngine.sequencerPlayheadTick(),0,LOOP_TICKS-1);
        playing = NativeEngine.sequencerIsPlaying();
        recording = NativeEngine.sequencerIsRecording();
        click = NativeEngine.sequencerIsClickOn();
        for (int i=0; i<TRACKS; i++) {
            trackParts[i] = clamp(NativeEngine.sequencerTrackPart(i),0,15);
        }
        int[] notes = NativeEngine.sequencerNotes(selectedTrack);
        noteData = notes == null ? new int[0] : notes;
    }

    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        if (getVisibility() != VISIBLE) return;
        refreshState(false);

        final float u = u();
        c.drawColor(Color.rgb(8,8,8));
        drawHeader(c,u);
        drawTransport(c,u);
        drawTracks(c,u);
        drawTarget(c,u);
        drawPianoRoll(c,u);
        drawFooter(c,u);
        if (resizeMode) drawResizeOverlay(c,u);

        postInvalidateDelayed(50L);
    }

    private void drawHeader(Canvas c, float u) {
        text.setColor(Color.rgb(241,238,229));
        text.setTextSize(19f*u);
        c.drawText("SEQUENCER", 14f*u, 31f*u, text);

        text.setTextSize(10.5f*u);
        text.setColor(Color.argb(150,241,238,229));
        c.drawText("8 TRACK  /  8 BAR  /  MIDI", 14f*u, 48f*u, text);

        int bar = playheadTick / TICKS_PER_BAR + 1;
        int beat = (playheadTick % TICKS_PER_BAR) / PPQN + 1;
        String pos = String.format(java.util.Locale.US, "%02d.%d", bar, beat);
        text.setTextSize(16f*u);
        text.setColor(Color.rgb(241,238,229));
        c.drawText(pos, 520f*u, 31f*u, text);

        drawButton(c, closeRect(u), "CLOSE", false, u);
    }

    private void drawTransport(Canvas c, float u) {
        drawButton(c, playRect(u), "PLAY", playing && !recording, u);
        drawButton(c, stopRect(u), "STOP", !playing, u);
        drawButton(c, recRect(u), recording ? "● REC" : "REC", recording, u);
        drawButton(c, clickRect(u), click ? "CLICK ON" : "CLICK OFF", click, u);
        drawButton(c, bpmMinusRect(u), "−", false, u);
        drawButton(c, bpmRect(u), "BPM " + bpm, false, u);
        drawButton(c, bpmPlusRect(u), "+", false, u);
    }

    private void drawTracks(Canvas c, float u) {
        float left = 12f*u, gap = 5f*u;
        float w = (getWidth() - 24f*u - gap*7f) / 8f;
        for (int i=0; i<TRACKS; i++) {
            RectF r = new RectF(
                    left + i*(w+gap), 126f*u,
                    left + i*(w+gap)+w, 179f*u);
            boolean selected = i == selectedTrack;
            drawPanel(c,r,selected,u);
            text.setTextSize(12f*u);
            text.setColor(selected ? Color.rgb(20,20,19) : Color.rgb(241,238,229));
            String t = "T" + (i+1);
            float tw = text.measureText(t);
            c.drawText(t,r.centerX()-tw/2f,r.top+20f*u,text);
            text.setTextSize(8.5f*u);
            String name = shortName(partNames[trackParts[i]],7);
            tw = text.measureText(name);
            c.drawText(name,r.centerX()-tw/2f,r.top+39f*u,text);
        }
    }

    private void drawTarget(Canvas c, float u) {
        RectF area = new RectF(12f*u,188f*u,getWidth()-12f*u,232f*u);
        drawPanel(c,area,false,u);

        text.setColor(Color.rgb(241,238,229));
        text.setTextSize(11f*u);
        c.drawText("T" + (selectedTrack+1) + " TARGET", 22f*u, 215f*u, text);

        drawButton(c, partMinusRect(u), "−", false, u);
        RectF nameRect = partNameRect(u);
        drawPanel(c,nameRect,true,u);
        text.setTextSize(11.5f*u);
        text.setColor(Color.rgb(20,20,19));
        String name = shortName(partNames[trackParts[selectedTrack]],18);
        float tw = text.measureText(name);
        c.drawText(name,nameRect.centerX()-tw/2f,nameRect.centerY()+4f*u,text);
        drawButton(c, partPlusRect(u), "+", false, u);
        drawButton(c, clearRect(u), "CLEAR T" + (selectedTrack+1), false, u);
    }

    private void drawPianoRoll(Canvas c, float u) {
        RectF roll = rollRect(u);
        float keyW = 58f*u;
        float gridLeft = roll.left + keyW;
        float gridW = roll.width() - keyW;
        float rowH = roll.height() / VISIBLE_NOTES;
        float stepW = gridW / STEPS_PER_PAGE;
        int highNote = lowNote + VISIBLE_NOTES - 1;

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.rgb(13,13,13));
        c.drawRect(roll,paint);

        for (int row=0; row<VISIBLE_NOTES; row++) {
            int note = highNote - row;
            float y0 = roll.top + row*rowH;
            boolean black = isBlack(note);
            paint.setColor(black ? Color.rgb(18,18,18) : Color.rgb(27,27,26));
            c.drawRect(roll.left,y0,gridLeft,y0+rowH,paint);

            if (note % 12 == 0) {
                text.setTextSize(8.5f*u);
                text.setColor(Color.argb(185,241,238,229));
                c.drawText(noteName(note),roll.left+5f*u,y0+rowH-3f*u,text);
            }

            paint.setColor(Color.argb(28,241,238,229));
            c.drawRect(gridLeft,y0,roll.right,y0+1f,paint);
        }

        for (int step=0; step<=STEPS_PER_PAGE; step++) {
            float x = gridLeft + step*stepW;
            boolean barLine = step % STEPS_PER_BAR == 0;
            boolean beatLine = step % 4 == 0;
            paint.setColor(Color.argb(barLine ? 150 : beatLine ? 78 : 30,
                    241,238,229));
            c.drawRect(x,roll.top,x+Math.max(1f,barLine ? 2f*u : 1f),
                    roll.bottom,paint);
        }

        final int pageStartTick = page * 2 * TICKS_PER_BAR;
        final int pageEndTick = pageStartTick + 2 * TICKS_PER_BAR;

        for (int i=0; i+3<noteData.length; i+=4) {
            int start = noteData[i];
            int dur = Math.max(1,noteData[i+1]);
            int note = noteData[i+2];
            int vel = clamp(noteData[i+3],1,127);
            if (note < lowNote || note > highNote) continue;

            int visualStart = start;
            int visualEnd = start + dur;
            if (visualEnd > LOOP_TICKS) {
                if (pageStartTick == 0) {
                    visualStart = 0;
                    visualEnd -= LOOP_TICKS;
                } else if (start >= pageStartTick && start < pageEndTick) {
                    visualEnd = pageEndTick;
                }
            }
            if (visualEnd <= pageStartTick || visualStart >= pageEndTick) continue;

            float x0 = gridLeft + (Math.max(visualStart,pageStartTick)-pageStartTick)
                    / (float)(2*TICKS_PER_BAR) * gridW;
            float x1 = gridLeft + (Math.min(visualEnd,pageEndTick)-pageStartTick)
                    / (float)(2*TICKS_PER_BAR) * gridW;
            int row = highNote - note;
            float y0 = roll.top + row*rowH + 1.5f*u;
            float y1 = y0 + rowH - 3f*u;
            x1 = Math.max(x0+2.5f*u,x1);

            int alpha = 120 + Math.round(vel / 127f * 115f);
            paint.setColor(Color.argb(alpha,238,229,207));
            c.drawRoundRect(new RectF(x0+1f*u,y0,x1-1f*u,y1),
                    2.5f*u,2.5f*u,paint);
        }

        if (playing && playheadTick >= pageStartTick && playheadTick < pageEndTick) {
            float x = gridLeft + (playheadTick-pageStartTick)
                    / (float)(2*TICKS_PER_BAR) * gridW;
            paint.setColor(Color.rgb(238,238,235));
            c.drawRect(x,roll.top,x+1.5f*u,roll.bottom,paint);
        }

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(1f,u));
        paint.setColor(Color.argb(100,241,238,229));
        c.drawRect(roll,paint);
        paint.setStyle(Paint.Style.FILL);
    }

    private void drawFooter(Canvas c, float u) {
        drawButton(c,pageMinusRect(u),"PAGE −",false,u);
        RectF p = pageRect(u);
        drawPanel(c,p,true,u);
        text.setTextSize(11f*u);
        text.setColor(Color.rgb(20,20,19));
        String bars = "BAR " + (page*2+1) + "–" + (page*2+2);
        float tw = text.measureText(bars);
        c.drawText(bars,p.centerX()-tw/2f,p.centerY()+4f*u,text);
        drawButton(c,pagePlusRect(u),"PAGE +",false,u);

        drawButton(c,octMinusRect(u),"OCT −",false,u);
        RectF o = octRect(u);
        drawPanel(c,o,false,u);
        text.setTextSize(10.5f*u);
        text.setColor(Color.rgb(241,238,229));
        String range = noteName(lowNote) + "–" + noteName(lowNote+VISIBLE_NOTES-1);
        tw = text.measureText(range);
        c.drawText(range,o.centerX()-tw/2f,o.centerY()+4f*u,text);
        drawButton(c,octPlusRect(u),"OCT +",false,u);
    }

    private void drawButton(Canvas c, RectF r, String label, boolean active, float u) {
        drawPanel(c,r,active,u);
        text.setTextSize(10.5f*u);
        float max = r.width()-8f*u;
        while (text.measureText(label) > max && text.getTextSize() > 7.2f*u)
            text.setTextSize(text.getTextSize()-0.5f*u);
        text.setColor(active ? Color.rgb(20,20,19) : Color.rgb(241,238,229));
        float tw = text.measureText(label);
        c.drawText(label,r.centerX()-tw/2f,r.centerY()+4f*u,text);
    }

    private void drawPanel(Canvas c, RectF r, boolean active, float u) {
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(active ? Color.rgb(238,229,207) : Color.rgb(18,18,18));
        c.drawRoundRect(r,4f*u,4f*u,paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(1f,u));
        paint.setColor(Color.argb(active ? 210 : 90,241,238,229));
        c.drawRoundRect(r,4f*u,4f*u,paint);
        paint.setStyle(Paint.Style.FILL);
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        if (getVisibility() != VISIBLE) return false;
        int action = e.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            downTarget = targetAt(e.getX(),e.getY());
            return downTarget >= 0 || rollRect(u()).contains(e.getX(),e.getY());
        }
        if (action == MotionEvent.ACTION_CANCEL) {
            downTarget = -1;
            return true;
        }
        if (action != MotionEvent.ACTION_UP) return true;

        int target = targetAt(e.getX(),e.getY());
        if (target >= 0 && target == downTarget) {
            activateTarget(target);
            downTarget = -1;
            performClick();
            return true;
        }

        RectF roll = rollRect(u());
        if (downTarget < 0 && roll.contains(e.getX(),e.getY())) {
            toggleGridAt(e.getX(),e.getY());
            performClick();
            return true;
        }
        downTarget = -1;
        return true;
    }

    @Override public boolean performClick() {
        super.performClick();
        return true;
    }

    private void activateTarget(int target) {
        if (target == 1) {
            NativeEngine.sequencerPlay();
        } else if (target == 2) {
            NativeEngine.sequencerStop();
        } else if (target == 3) {
            NativeEngine.sequencerToggleRecord();
        } else if (target == 4) {
            NativeEngine.sequencerSetClick(!click);
        } else if (target == 5) {
            NativeEngine.sequencerSetBpm(Math.max(40,bpm-2));
        } else if (target == 6) {
            NativeEngine.sequencerSetBpm(Math.min(240,bpm+2));
        } else if (target >= 20 && target < 28) {
            NativeEngine.sequencerSetSelectedTrack(target-20);
        } else if (target == 30) {
            int part = (trackParts[selectedTrack] + 15) % 16;
            NativeEngine.sequencerSetTrackPart(selectedTrack,part);
        } else if (target == 31) {
            int part = (trackParts[selectedTrack] + 1) % 16;
            NativeEngine.sequencerSetTrackPart(selectedTrack,part);
        } else if (target == 32) {
            NativeEngine.sequencerClearTrack(selectedTrack);
        } else if (target == 40) {
            page = Math.max(0,page-1);
        } else if (target == 41) {
            page = Math.min(3,page+1);
        } else if (target == 42) {
            lowNote = Math.max(0,lowNote-12);
        } else if (target == 43) {
            lowNote = Math.min(104,lowNote+12);
        } else if (target == 90) {
            setVisibility(GONE);
            if (listener != null) listener.onSequencerClose();
        }
        refreshState(true);
        invalidate();
    }

    private void toggleGridAt(float x, float y) {
        float u = u();
        RectF roll = rollRect(u);
        float keyW = 58f*u;
        float gridLeft = roll.left + keyW;
        if (x < gridLeft) return;

        float gridW = roll.width()-keyW;
        float stepW = gridW/STEPS_PER_PAGE;
        int col = clamp((int)((x-gridLeft)/stepW),0,STEPS_PER_PAGE-1);
        float rowH = roll.height()/VISIBLE_NOTES;
        int row = clamp((int)((y-roll.top)/rowH),0,VISIBLE_NOTES-1);
        int note = lowNote + (VISIBLE_NOTES-1-row);
        int globalStep = page*STEPS_PER_PAGE+col;
        NativeEngine.sequencerToggleGridNote(selectedTrack,globalStep,note,100);
        refreshState(true);
        invalidate();
    }

    private int targetAt(float x, float y) {
        float u = u();
        if (closeRect(u).contains(x,y)) return 90;
        if (playRect(u).contains(x,y)) return 1;
        if (stopRect(u).contains(x,y)) return 2;
        if (recRect(u).contains(x,y)) return 3;
        if (clickRect(u).contains(x,y)) return 4;
        if (bpmMinusRect(u).contains(x,y)) return 5;
        if (bpmPlusRect(u).contains(x,y)) return 6;

        float left = 12f*u, gap = 5f*u;
        float w = (getWidth()-24f*u-gap*7f)/8f;
        for (int i=0; i<TRACKS; i++) {
            RectF r = new RectF(left+i*(w+gap),126f*u,
                    left+i*(w+gap)+w,179f*u);
            if (r.contains(x,y)) return 20+i;
        }

        if (partMinusRect(u).contains(x,y)) return 30;
        if (partPlusRect(u).contains(x,y)) return 31;
        if (clearRect(u).contains(x,y)) return 32;
        if (pageMinusRect(u).contains(x,y)) return 40;
        if (pagePlusRect(u).contains(x,y)) return 41;
        if (octMinusRect(u).contains(x,y)) return 42;
        if (octPlusRect(u).contains(x,y)) return 43;
        return -1;
    }

    private RectF closeRect(float u){ return new RectF(628f*u,10f*u,708f*u,45f*u); }
    private RectF playRect(float u){ return new RectF(12f*u,65f*u,88f*u,111f*u); }
    private RectF stopRect(float u){ return new RectF(94f*u,65f*u,170f*u,111f*u); }
    private RectF recRect(float u){ return new RectF(176f*u,65f*u,252f*u,111f*u); }
    private RectF clickRect(float u){ return new RectF(258f*u,65f*u,368f*u,111f*u); }
    private RectF bpmMinusRect(float u){ return new RectF(374f*u,65f*u,422f*u,111f*u); }
    private RectF bpmRect(float u){ return new RectF(428f*u,65f*u,570f*u,111f*u); }
    private RectF bpmPlusRect(float u){ return new RectF(576f*u,65f*u,624f*u,111f*u); }
    private RectF partMinusRect(float u){ return new RectF(132f*u,194f*u,176f*u,226f*u); }
    private RectF partNameRect(float u){ return new RectF(182f*u,194f*u,456f*u,226f*u); }
    private RectF partPlusRect(float u){ return new RectF(462f*u,194f*u,506f*u,226f*u); }
    private RectF clearRect(float u){ return new RectF(514f*u,194f*u,696f*u,226f*u); }
    private RectF rollRect(float u){ return new RectF(12f*u,242f*u,708f*u,630f*u); }
    private RectF pageMinusRect(float u){ return new RectF(12f*u,642f*u,100f*u,704f*u); }
    private RectF pageRect(float u){ return new RectF(106f*u,642f*u,210f*u,704f*u); }
    private RectF pagePlusRect(float u){ return new RectF(216f*u,642f*u,304f*u,704f*u); }
    private RectF octMinusRect(float u){ return new RectF(330f*u,642f*u,418f*u,704f*u); }
    private RectF octRect(float u){ return new RectF(424f*u,642f*u,608f*u,704f*u); }
    private RectF octPlusRect(float u){ return new RectF(614f*u,642f*u,702f*u,704f*u); }

    private static boolean isBlack(int note) {
        int pc = Math.floorMod(note,12);
        return pc==1 || pc==3 || pc==6 || pc==8 || pc==10;
    }

    private static String noteName(int note) {
        String[] names = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        return names[Math.floorMod(note,12)] + (note/12-1);
    }

    private static String shortName(String value, int max) {
        if (value == null) return "PART";
        return value.length() <= max ? value : value.substring(0,max);
    }

    private static int clamp(int value, int lo, int hi) {
        return Math.max(lo,Math.min(hi,value));
    }
}
