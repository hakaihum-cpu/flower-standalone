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
    private static final int MAX_BARS = 8;
    private static final int PPQN = 96;
    private static final int TICKS_PER_BAR = PPQN * 4;
    private static final int MAX_LOOP_TICKS = TICKS_PER_BAR * MAX_BARS;
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
    private int loopBars = 2;
    private int viewMode = 0; // 0 ROLL, 1 ARRANGE, 2 MIXER
    private boolean euclid = false;
    private int euclidRoot = 60;
    private int euclidScale = 0;
    private int euclidPulses = 5;
    private int euclidSteps = 16;
    private int lastLoopTick = -1;
    private final int[] sendRev = new int[TRACKS];
    private final int[] sendDelay = new int[TRACKS];
    private static final String[] SCALE_NAMES = {"MAJ","MIN","DOR","PENTA"};
    private int lowNote = 48; // C3
    private final int[] trackParts = new int[]{0,1,2,3,4,5,6,7};
    private int[] noteData = new int[0];
    private long lastPollMs = 0L;
    private long lastVisualTimingPollMs = 0L;
    private float visualLatencyTicks = 0f;
    private int downTarget = -1;

    private static final String[] CHORD_NAMES =
            new String[]{"MAJ","MIN","MAJ7","7","m7","SUS2","SUS4"};
    private boolean chordMode = false;
    private int chordType = 0;
    private boolean eraserMode = false;
    private boolean eraserGestureActive = false;
    private final java.util.HashSet<Integer> pendingEraseIndices =
            new java.util.HashSet<>();
    private float eraseLastX = 0f;
    private float eraseLastY = 0f;

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
        playing = NativeEngine.sequencerIsPlaying();
        recording = NativeEngine.sequencerIsRecording();
        click = NativeEngine.sequencerIsClickOn();
        loopBars = clamp(NativeEngine.sequencerLoopBars(),1,8);
        for (int i=0; i<TRACKS; i++) {
            trackParts[i] = clamp(NativeEngine.sequencerTrackPart(i),0,15);
        }
        int[] notes = NativeEngine.sequencerNotes(selectedTrack);
        noteData = notes == null ? new int[0] : notes;
    }

    private void refreshVisualPlayhead() {
        if (!playing) {
            playheadTick = clamp(NativeEngine.sequencerPlayheadTick(),0,loopTicks()-1);
            return;
        }

        long now = android.os.SystemClock.uptimeMillis();
        if (now - lastVisualTimingPollMs >= 250L) {
            lastVisualTimingPollMs = now;
            int sampleRate = Math.max(1, NativeEngine.audioSampleRate());
            int bufferFrames = Math.max(0, NativeEngine.audioBufferSizeFrames());
            visualLatencyTicks = (float)bufferFrames
                    * (float)bpm * (float)PPQN
                    / ((float)sampleRate * 60f);
        }

        float rawTick = NativeEngine.sequencerPlayheadTick();
        float visualTick = rawTick - visualLatencyTicks;
        while (visualTick < 0f) visualTick += loopTicks();
        while (visualTick >= loopTicks()) visualTick -= loopTicks();
        playheadTick = clamp(Math.round(visualTick),0,loopTicks()-1);
    }

    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        if (getVisibility() != VISIBLE) return;
        refreshState(false);
        refreshVisualPlayhead();

        final float u = u();
        c.drawColor(Color.rgb(8,8,8));
        drawHeader(c,u);
        drawTransport(c,u);
        drawViewTabs(c,u);
        if(viewMode==0){ drawTracks(c,u); drawTarget(c,u); drawPianoRoll(c,u); drawFooter(c,u); }
        else if(viewMode==1) drawArrange(c,u);
        else drawSequenceMixer(c,u);
        if (resizeMode) drawResizeOverlay(c,u);

        if (playing && euclid) {
            if (lastLoopTick >= 0 && playheadTick < lastLoopTick) generateEuclid();
            lastLoopTick = playheadTick;
        } else if (!playing) lastLoopTick=-1;
        if (playing) postInvalidateOnAnimation();
        else postInvalidateDelayed(80L);
    }

    private void drawHeader(Canvas c, float u) {
        text.setColor(Color.rgb(241,238,229));
        text.setTextSize(19f*u);
        c.drawText("SEQUENCER", 14f*u, 31f*u, text);

        text.setTextSize(10.5f*u);
        text.setColor(Color.argb(150,241,238,229));
        c.drawText("8 TRACK  /  " + loopBars + " BAR  /  MIDI", 14f*u, 48f*u, text);

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
        text.setTextSize(10.8f*u);
        text.setColor(Color.rgb(20,20,19));
        String name = shortName(partNames[trackParts[selectedTrack]],14);
        float tw = text.measureText(name);
        c.drawText(name,nameRect.centerX()-tw/2f,nameRect.centerY()+4f*u,text);
        drawButton(c, partPlusRect(u), "+", false, u);
        drawButton(c, chordRect(u), chordMode ? "CHORD ON" : "CHORD", chordMode, u);
        drawButton(c, chordTypeRect(u), CHORD_NAMES[chordType], chordMode, u);
        drawButton(c, clearRect(u), "CLEAR", false, u);
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
            if (visualEnd > loopTicks()) {
                if (pageStartTick == 0) {
                    visualStart = 0;
                    visualEnd -= loopTicks();
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

            boolean pendingErase = pendingEraseIndices.contains(i/4);
            int alpha = pendingErase
                    ? 42
                    : 120 + Math.round(vel / 127f * 115f);
            RectF noteRect = new RectF(x0+1f*u,y0,x1-1f*u,y1);
            paint.setColor(Color.argb(alpha,238,229,207));
            c.drawRoundRect(noteRect,2.5f*u,2.5f*u,paint);
            if (pendingErase) {
                paint.setStyle(Paint.Style.STROKE);
                paint.setStrokeWidth(Math.max(1f,1.2f*u));
                paint.setColor(Color.argb(165,241,238,229));
                c.drawLine(noteRect.left,noteRect.top,noteRect.right,noteRect.bottom,paint);
                c.drawLine(noteRect.left,noteRect.bottom,noteRect.right,noteRect.top,paint);
                paint.setStyle(Paint.Style.FILL);
            }
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
        drawButton(c,eraserRect(u),eraserMode ? "ERASER ON" : "ERASER",eraserMode,u);

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


    private void drawViewTabs(Canvas c,float u) {
        drawButton(c,rollTabRect(u),"ROLL",viewMode==0,u);
        drawButton(c,arrTabRect(u),"ARRANGE",viewMode==1,u);
        drawButton(c,mixTabRect(u),"MIX",viewMode==2,u);
    }

    private void drawArrange(Canvas c,float u) {
        float top=72f*u,left=12f*u,labelW=116f*u,right=getWidth()-12f*u,rowH=72f*u;
        for(int t=0;t<TRACKS;t++){
            float y=top+t*rowH;
            RectF label=new RectF(left,y,left+labelW-6f*u,y+rowH-6f*u);
            drawPanel(c,label,t==selectedTrack,u);
            text.setTextSize(10.5f*u); text.setColor(t==selectedTrack?Color.rgb(20,20,19):Color.rgb(241,238,229));
            c.drawText("T"+(t+1)+" "+shortName(partNames[trackParts[t]],9),label.left+7f*u,label.centerY()+4f*u,text);
            RectF lane=new RectF(left+labelW,y,right,y+rowH-6f*u); drawPanel(c,lane,false,u);
            int[] data=NativeEngine.sequencerNotes(t);
            if(data!=null) for(int i=0;i+3<data.length;i+=4){
                if(data[i]>=loopTicks()) continue;
                float x0=lane.left+data[i]/(float)loopTicks()*lane.width();
                float x1=lane.left+Math.min(loopTicks(),data[i]+Math.max(1,data[i+1]))/(float)loopTicks()*lane.width();
                paint.setColor(Color.argb(155,238,229,207));
                c.drawRoundRect(new RectF(x0+1f*u,lane.top+9f*u,Math.max(x0+4f*u,x1-1f*u),lane.bottom-9f*u),3f*u,3f*u,paint);
            }
            for(int b=1;b<loopBars;b++){float x=lane.left+lane.width()*b/loopBars;paint.setColor(Color.argb(65,241,238,229));c.drawRect(x,lane.top,x+1f,lane.bottom,paint);}
        }
        drawButton(c,new RectF(12f*u,656f*u,126f*u,704f*u),"BARS "+loopBars,false,u);
        drawButton(c,new RectF(134f*u,656f*u,258f*u,704f*u),euclid?"EUCLID ON":"EUCLID",euclid,u);
        drawButton(c,new RectF(266f*u,656f*u,390f*u,704f*u),noteName(euclidRoot),euclid,u);
        drawButton(c,new RectF(398f*u,656f*u,522f*u,704f*u),SCALE_NAMES[euclidScale],euclid,u);
        drawButton(c,new RectF(530f*u,656f*u,708f*u,704f*u),"P"+euclidPulses+"/"+euclidSteps,euclid,u);
    }

    private void drawSequenceMixer(Canvas c,float u) {
        float top=72f*u,rowH=72f*u;
        for(int t=0;t<TRACKS;t++){
            float y=top+t*rowH;
            RectF row=new RectF(12f*u,y,708f*u,y+rowH-6f*u); drawPanel(c,row,t==selectedTrack,u);
            text.setTextSize(10.5f*u); text.setColor(t==selectedTrack?Color.rgb(20,20,19):Color.rgb(241,238,229));
            c.drawText("T"+(t+1)+" "+shortName(partNames[trackParts[t]],9),22f*u,y+25f*u,text);
            text.setTextSize(9f*u); c.drawText("VOL/PAN",22f*u,y+48f*u,text);
            text.setTextSize(10f*u); c.drawText("REV "+sendRev[t],250f*u,y+25f*u,text); c.drawText("DLY "+sendDelay[t],430f*u,y+25f*u,text);
            drawBar(c,250f*u,y+39f*u,140f*u,sendRev[t]/127f,u);
            drawBar(c,430f*u,y+39f*u,140f*u,sendDelay[t]/127f,u);
        }
        text.setColor(Color.argb(160,241,238,229));text.setTextSize(10f*u);
        c.drawText("OP-1 STYLE  •  TAP REV / DLY",12f*u,700f*u,text);
    }

    private void drawBar(Canvas c,float x,float y,float w,float v,float u){
        paint.setColor(Color.argb(55,241,238,229));c.drawRect(x,y,x+w,y+8f*u,paint);
        paint.setColor(Color.rgb(238,229,207));c.drawRect(x,y,x+w*Math.max(0f,Math.min(1f,v)),y+8f*u,paint);
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
            touchDownX = e.getX();
            touchDownY = e.getY();
            touchMoved = false;
            resizeMode = false;
            resizeNoteIndex = -1;
            downTarget = targetAt(touchDownX,touchDownY);
            pressedNoteIndex = -1;

            if (downTarget >= 0) return true;

            RectF roll = rollRect(u());
            if (roll.contains(touchDownX,touchDownY)) {
                if (eraserMode) {
                    eraserGestureActive = true;
                    pendingEraseIndices.clear();
                    eraseLastX = touchDownX;
                    eraseLastY = touchDownY;
                    collectEraseAt(touchDownX,touchDownY);
                    invalidate();
                    return true;
                }
                pressedNoteIndex = findNoteAt(touchDownX,touchDownY);
                if (pressedNoteIndex >= 0 && !recording) {
                    postDelayed(noteLongPress, 460L);
                }
                return true;
            }
            return false;
        }

        if (action == MotionEvent.ACTION_MOVE) {
            if (eraserGestureActive) {
                eraseAlongSegment(eraseLastX,eraseLastY,e.getX(),e.getY());
                eraseLastX = e.getX();
                eraseLastY = e.getY();
                invalidate();
                return true;
            }
            if (resizeMode) {
                updateResizeFromX(e.getX());
                return true;
            }
            float dx = e.getX() - touchDownX;
            float dy = e.getY() - touchDownY;
            if (dx*dx + dy*dy > (12f*u())*(12f*u())) {
                touchMoved = true;
                removeCallbacks(noteLongPress);
            }
            return true;
        }

        if (action == MotionEvent.ACTION_CANCEL) {
            removeCallbacks(noteLongPress);
            downTarget = -1;
            pressedNoteIndex = -1;
            resizeMode = false;
            resizeNoteIndex = -1;
            eraserGestureActive = false;
            pendingEraseIndices.clear();
            invalidate();
            return true;
        }

        if (action != MotionEvent.ACTION_UP) return true;
        removeCallbacks(noteLongPress);

        if (eraserGestureActive) {
            eraseAlongSegment(eraseLastX,eraseLastY,e.getX(),e.getY());
            commitEraseGesture();
            eraserGestureActive = false;
            pressedNoteIndex = -1;
            downTarget = -1;
            performClick();
            return true;
        }

        if (resizeMode) {
            int base=resizeNoteIndex*4;
            int chordStart=(base+1<noteData.length)?noteData[base]:-1;
            int chordDuration=(base+1<noteData.length)?noteData[base+1]:-1;
            for(int i=0;i+3<noteData.length;i+=4){
                if(noteData[i]==chordStart && noteData[i+1]==chordDuration)
                    NativeEngine.sequencerSetNoteDuration(selectedTrack,i/4,resizePreviewDuration);
            }
            resizeMode = false;
            resizeNoteIndex = -1;
            pressedNoteIndex = -1;
            refreshState(true);
            invalidate();
            performClick();
            return true;
        }

        int target = targetAt(e.getX(),e.getY());
        if (target >= 0 && target == downTarget) {
            activateTarget(target);
            downTarget = -1;
            pressedNoteIndex = -1;
            performClick();
            return true;
        }

        RectF roll = rollRect(u());
        if (downTarget < 0 && !touchMoved && roll.contains(e.getX(),e.getY())) {
            if (pressedNoteIndex >= 0) {
                removeExistingNote(pressedNoteIndex);
            } else {
                toggleGridAt(e.getX(),e.getY());
            }
            pressedNoteIndex = -1;
            performClick();
            return true;
        }

        downTarget = -1;
        pressedNoteIndex = -1;
        return true;
    }

    @Override public boolean performClick() {
        super.performClick();
        return true;
    }

    private void beginResize(int noteIndex) {
        int base = noteIndex * 4;
        if (base < 0 || base + 3 >= noteData.length) return;
        resizeMode = true;
        resizeNoteIndex = noteIndex;
        resizeStartTick = noteData[base];
        resizeOriginalDuration = Math.max(1,noteData[base+1]);
        resizePreviewDuration = resizeOriginalDuration;
        resizeNote = noteData[base+2];
        performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS);
        invalidate();
    }

    private void updateResizeFromX(float x) {
        if (!resizeMode) return;
        int originalSteps = Math.max(1,
                Math.round(resizeOriginalDuration / (float)STEP_TICKS));
        int deltaSteps = Math.round((x - touchDownX) / (30f*u()));
        int maxSteps = loopTicks() / STEP_TICKS - 1;
        int steps = clamp(originalSteps + deltaSteps,1,maxSteps);
        resizePreviewDuration = steps * STEP_TICKS;
        invalidate();
    }

    private void drawResizeOverlay(Canvas c, float u) {
        RectF roll = rollRect(u);
        float top = touchDownY > roll.centerY()
                ? roll.top + 18f*u
                : roll.bottom - 130f*u;
        RectF box = new RectF(52f*u,top,668f*u,top+112f*u);

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(220,5,5,5));
        c.drawRoundRect(box,9f*u,9f*u,paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(1f,1.5f*u));
        paint.setColor(Color.argb(185,241,238,229));
        c.drawRoundRect(box,9f*u,9f*u,paint);
        paint.setStyle(Paint.Style.FILL);

        int durationSteps = Math.max(1,
                Math.round(resizePreviewDuration / (float)STEP_TICKS));
        int windowStart = Math.max(0,durationSteps-16);
        String title = noteName(resizeNote) + "  LENGTH " + durationSteps + "/16";
        text.setColor(Color.rgb(241,238,229));
        text.setTextSize(13f*u);
        c.drawText(title,box.left+12f*u,box.top+24f*u,text);
        text.setTextSize(9.5f*u);
        text.setColor(Color.argb(155,241,238,229));
        c.drawText(windowStart > 0 ? "← EARLIER     DRAG END" : "START     DRAG END",
                box.left+12f*u,box.top+42f*u,text);

        float left = box.left+12f*u;
        float right = box.right-12f*u;
        float gridTop = box.top+54f*u;
        float gridBottom = box.bottom-14f*u;
        float cell = (right-left)/16f;

        for (int i=0;i<16;i++) {
            int stepNumber = windowStart + i + 1;
            if (stepNumber <= durationSteps) {
                paint.setColor(Color.argb(105,238,229,207));
                c.drawRect(left+i*cell+1f*u,gridTop+1f*u,
                        left+(i+1)*cell-1f*u,gridBottom-1f*u,paint);
            }
            paint.setColor(Color.argb(i%4==3 ? 125 : 58,241,238,229));
            c.drawRect(left+(i+1)*cell,gridTop,
                    left+(i+1)*cell+Math.max(1f,u),gridBottom,paint);
        }
        paint.setColor(Color.argb(90,241,238,229));
        c.drawRect(left,gridTop,right,gridTop+Math.max(1f,u),paint);
        c.drawRect(left,gridBottom-Math.max(1f,u),right,gridBottom,paint);

        float endX = left + (durationSteps-windowStart)*cell;
        endX = Math.max(left,Math.min(right,endX));
        paint.setColor(Color.rgb(241,238,229));
        c.drawRect(endX-2f*u,gridTop-4f*u,endX+2f*u,gridBottom+4f*u,paint);
    }

    private RectF noteRectForIndex(int noteIndex, float u) {
        int base = noteIndex*4;
        if (base < 0 || base+3 >= noteData.length) return null;

        RectF roll = rollRect(u);
        float keyW = 58f*u;
        float gridLeft = roll.left+keyW;
        float gridW = roll.width()-keyW;
        float rowH = roll.height()/VISIBLE_NOTES;
        int highNote = lowNote+VISIBLE_NOTES-1;

        int start = noteData[base];
        int dur = Math.max(1,noteData[base+1]);
        int note = noteData[base+2];
        if (note < lowNote || note > highNote) return null;

        int pageStartTick = page*2*TICKS_PER_BAR;
        int pageEndTick = pageStartTick+2*TICKS_PER_BAR;
        int visualStart = start;
        int visualEnd = start+dur;
        if (visualEnd > loopTicks()) {
            if (pageStartTick == 0) {
                visualStart = 0;
                visualEnd -= loopTicks();
            } else if (start >= pageStartTick && start < pageEndTick) {
                visualEnd = pageEndTick;
            }
        }
        if (visualEnd <= pageStartTick || visualStart >= pageEndTick) return null;

        float x0 = gridLeft +
                (Math.max(visualStart,pageStartTick)-pageStartTick) /
                (float)(2*TICKS_PER_BAR)*gridW;
        float x1 = gridLeft +
                (Math.min(visualEnd,pageEndTick)-pageStartTick) /
                (float)(2*TICKS_PER_BAR)*gridW;
        int row = highNote-note;
        float y0 = roll.top+row*rowH+1.5f*u;
        float y1 = y0+rowH-3f*u;
        x1 = Math.max(x0+12f*u,x1);
        return new RectF(x0-3f*u,y0-2f*u,x1+3f*u,y1+2f*u);
    }

    private int findNoteAt(float x, float y) {
        float u = u();
        for (int i=noteData.length/4-1; i>=0; i--) {
            RectF r = noteRectForIndex(i,u);
            if (r != null && r.contains(x,y)) return i;
        }
        return -1;
    }

    private void removeExistingNote(int noteIndex) {
        NativeEngine.sequencerDeleteNote(selectedTrack,noteIndex);
        postDelayed(() -> {
            refreshState(true);
            invalidate();
        },30L);
    }

    private void collectEraseAt(float x, float y) {
        if (!eraserMode) return;
        float uu = u();
        for (int i=0; i<noteData.length/4; i++) {
            RectF r = noteRectForIndex(i,uu);
            if (r != null && r.contains(x,y)) pendingEraseIndices.add(i);
        }
    }

    private void eraseAlongSegment(float x0, float y0, float x1, float y1) {
        if (!eraserMode) return;
        float dx = x1-x0;
        float dy = y1-y0;
        float distance = (float)Math.sqrt(dx*dx+dy*dy);
        int samples = Math.max(1,(int)Math.ceil(distance / Math.max(4f,8f*u())));
        for (int i=0;i<=samples;i++) {
            float t = i/(float)samples;
            collectEraseAt(x0+dx*t,y0+dy*t);
        }
    }

    private void commitEraseGesture() {
        if (pendingEraseIndices.isEmpty()) {
            invalidate();
            return;
        }

        java.util.ArrayList<Integer> indices =
                new java.util.ArrayList<>(pendingEraseIndices);
        java.util.Collections.sort(indices,java.util.Collections.reverseOrder());

        // Delete highest indexes first. Each native delete compacts the fixed
        // note array, so descending order preserves the identity of all
        // remaining indexes selected during this gesture.
        for (int index : indices) {
            NativeEngine.sequencerDeleteNote(selectedTrack,index);
        }

        pendingEraseIndices.clear();
        postDelayed(() -> {
            refreshState(true);
            invalidate();
        },35L);
    }

    private int[] chordIntervals() {
        switch (chordType) {
            case 1: return new int[]{0,3,7};
            case 2: return new int[]{0,4,7,11};
            case 3: return new int[]{0,4,7,10};
            case 4: return new int[]{0,3,7,10};
            case 5: return new int[]{0,2,7};
            case 6: return new int[]{0,5,7};
            default: return new int[]{0,4,7};
        }
    }

    private boolean noteExistsAtStep(int step, int note) {
        int tick = step*STEP_TICKS;
        for (int i=0; i+3<noteData.length; i+=4) {
            if (noteData[i] == tick && noteData[i+2] == note) return true;
        }
        return false;
    }

    private void auditionNotes(int[] notes, int count) {
        if (playing || count <= 0) return;
        final int part = trackParts[selectedTrack];
        final int[] preview = java.util.Arrays.copyOf(notes,count);
        for (int note : preview) {
            NativeEngine.noteOnPart(part,note,96);
        }
        postDelayed(() -> {
            for (int note : preview) {
                NativeEngine.noteOffPart(part,note,0);
            }
        },180L);
    }

    private void activateTarget(int target) {
        if (target == 100) { viewMode = 0; return; }\n        if (target == 101) { viewMode = 1; return; }\n        if (target == 102) { viewMode = 2; return; }
        if (target == 110) {
            loopBars = loopBars==1?2:loopBars==2?4:loopBars==4?8:1;
            page=Math.min(page,Math.max(0,(loopBars-1)/2));
            NativeEngine.sequencerSetLoopBars(loopBars); return;
        }
        if (target == 111) { euclid=!euclid; if(euclid) generateEuclid(); return; }
        if (target == 112) { euclidRoot = euclidRoot>=71?48:euclidRoot+1; if(euclid) generateEuclid(); return; }
        if (target == 113) { euclidScale=(euclidScale+1)%SCALE_NAMES.length; if(euclid) generateEuclid(); return; }
        if (target == 114) { euclidPulses=euclidPulses>=euclidSteps?1:euclidPulses+1; if(euclid) generateEuclid(); return; }
        if (target >= 120 && target < 128) { NativeEngine.sequencerSetSelectedTrack(target-120); return; }
        if (target >= 140 && target < 156) {
            int k=target-140, tr=k/2; boolean rev=(k%2)==0;
            if(rev) sendRev[tr]=(sendRev[tr]+16)%128; else sendDelay[tr]=(sendDelay[tr]+16)%128;
            NativeEngine.sequencerSetTrackSend(tr,sendRev[tr],sendDelay[tr]); return;
        }\n        if (target == 1) {
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
        } else if (target == 33) {
            chordMode = !chordMode;
        } else if (target == 34) {
            chordType = (chordType + 1) % CHORD_NAMES.length;
            chordMode = true;
        } else if (target == 40) {
            page = Math.max(0,page-1);
        } else if (target == 41) {
            page = Math.min(Math.max(0,(loopBars-1)/2),page+1);
        } else if (target == 42) {
            lowNote = Math.max(0,lowNote-12);
        } else if (target == 43) {
            lowNote = Math.min(104,lowNote+12);
        } else if (target == 44) {
            eraserMode = !eraserMode;
            pendingEraseIndices.clear();
            removeCallbacks(noteLongPress);
            resizeMode = false;
            resizeNoteIndex = -1;
            pressedNoteIndex = -1;
        } else if (target == 90) {
            setVisibility(GONE);
            if (listener != null) listener.onSequencerClose();
        }
        refreshState(true);
        invalidate();
    }

    private void toggleGridAt(float x, float y) {
        refreshState(true);

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
        int root = lowNote + (VISIBLE_NOTES-1-row);
        int globalStep = page*STEPS_PER_PAGE+col;

        if (!chordMode) {
            boolean exists = noteExistsAtStep(globalStep,root);
            NativeEngine.sequencerSetGridNote(
                    selectedTrack,globalStep,root,exists ? 0 : 100);
            if (!exists) auditionNotes(new int[]{root},1);
        } else {
            int[] intervals = chordIntervals();
            int[] notes = new int[intervals.length];
            int count = 0;
            boolean allPresent = true;
            for (int interval : intervals) {
                int note = root + interval;
                if (note > 127) continue;
                notes[count++] = note;
                if (!noteExistsAtStep(globalStep,note)) allPresent = false;
            }
            if (count == 0) return;

            int value = allPresent ? 0 : 100;
            for (int i=0;i<count;i++) {
                NativeEngine.sequencerSetGridNote(
                        selectedTrack,globalStep,notes[i],value);
            }
            if (!allPresent) auditionNotes(notes,count);
        }

        postDelayed(() -> {
            refreshState(true);
            invalidate();
        },30L);
    }

    private int targetAt(float x, float y) {
        float u = u();
        if (closeRect(u).contains(x,y)) return 90;\n        if (rollTabRect(u).contains(x,y)) return 100;
        if (arrTabRect(u).contains(x,y)) return 101;
        if (mixTabRect(u).contains(x,y)) return 102;
        if (viewMode==1) {
            if (new RectF(12f*u,656f*u,126f*u,704f*u).contains(x,y)) return 110;
            if (new RectF(134f*u,656f*u,258f*u,704f*u).contains(x,y)) return 111;
            if (new RectF(266f*u,656f*u,390f*u,704f*u).contains(x,y)) return 112;
            if (new RectF(398f*u,656f*u,522f*u,704f*u).contains(x,y)) return 113;
            if (new RectF(530f*u,656f*u,708f*u,704f*u).contains(x,y)) return 114;
            float top=72f*u,rowH=72f*u;
            for(int t=0;t<TRACKS;t++) if(new RectF(12f*u,top+t*rowH,708f*u,top+(t+1)*rowH-6f*u).contains(x,y)) return 120+t;
        }
        if (viewMode==2) {
            float top=72f*u,rowH=72f*u;
            for(int t=0;t<TRACKS;t++){
                float yy=top+t*rowH;
                if(new RectF(240f*u,yy,410f*u,yy+rowH-6f*u).contains(x,y)) return 140+t*2;
                if(new RectF(420f*u,yy,590f*u,yy+rowH-6f*u).contains(x,y)) return 141+t*2;
                if(new RectF(12f*u,yy,230f*u,yy+rowH-6f*u).contains(x,y)) return 120+t;
            }
        }
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
        if (chordRect(u).contains(x,y)) return 33;
        if (chordTypeRect(u).contains(x,y)) return 34;
        if (pageMinusRect(u).contains(x,y)) return 40;
        if (pagePlusRect(u).contains(x,y)) return 41;
        if (octMinusRect(u).contains(x,y)) return 42;
        if (octPlusRect(u).contains(x,y)) return 43;
        if (eraserRect(u).contains(x,y)) return 44;
        return -1;
    }

    private int loopTicks(){ return TICKS_PER_BAR * loopBars; }
    private RectF rollTabRect(float u){ return new RectF(300f*u,10f*u,368f*u,45f*u); }
    private RectF arrTabRect(float u){ return new RectF(374f*u,10f*u,454f*u,45f*u); }
    private RectF mixTabRect(float u){ return new RectF(460f*u,10f*u,522f*u,45f*u); }
    private RectF closeRect(float u){ return new RectF(628f*u,10f*u,708f*u,45f*u); }
    private RectF playRect(float u){ return new RectF(12f*u,65f*u,88f*u,111f*u); }
    private RectF stopRect(float u){ return new RectF(94f*u,65f*u,170f*u,111f*u); }
    private RectF recRect(float u){ return new RectF(176f*u,65f*u,252f*u,111f*u); }
    private RectF clickRect(float u){ return new RectF(258f*u,65f*u,368f*u,111f*u); }
    private RectF bpmMinusRect(float u){ return new RectF(374f*u,65f*u,422f*u,111f*u); }
    private RectF bpmRect(float u){ return new RectF(428f*u,65f*u,570f*u,111f*u); }
    private RectF bpmPlusRect(float u){ return new RectF(576f*u,65f*u,624f*u,111f*u); }
    private RectF partMinusRect(float u){ return new RectF(116f*u,194f*u,156f*u,226f*u); }
    private RectF partNameRect(float u){ return new RectF(162f*u,194f*u,386f*u,226f*u); }
    private RectF partPlusRect(float u){ return new RectF(392f*u,194f*u,432f*u,226f*u); }
    private RectF chordRect(float u){ return new RectF(440f*u,194f*u,520f*u,226f*u); }
    private RectF chordTypeRect(float u){ return new RectF(526f*u,194f*u,606f*u,226f*u); }
    private RectF clearRect(float u){ return new RectF(614f*u,194f*u,696f*u,226f*u); }
    private RectF rollRect(float u){ return new RectF(12f*u,242f*u,708f*u,630f*u); }
    private RectF pageMinusRect(float u){ return new RectF(12f*u,642f*u,82f*u,704f*u); }
    private RectF pageRect(float u){ return new RectF(88f*u,642f*u,178f*u,704f*u); }
    private RectF pagePlusRect(float u){ return new RectF(184f*u,642f*u,254f*u,704f*u); }
    private RectF eraserRect(float u){ return new RectF(264f*u,642f*u,360f*u,704f*u); }
    private RectF octMinusRect(float u){ return new RectF(370f*u,642f*u,440f*u,704f*u); }
    private RectF octRect(float u){ return new RectF(446f*u,642f*u,626f*u,704f*u); }
    private RectF octPlusRect(float u){ return new RectF(632f*u,642f*u,702f*u,704f*u); }

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
