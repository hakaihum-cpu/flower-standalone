package com.example.epsampler;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

/**
 * Full-screen graphical editor for physical-model parameters.
 *
 * The audio parameter meanings are unchanged. This view only replaces the
 * standard Android slider stack with direct graphical manipulation.
 */
final class SoundDesignView extends View {
    interface Listener {
        void onSoundDesignClose();
        void onSoundDesignModelParameterChanged(int index, int value);
        void onSoundDesignAdsrChanged(int attackMs, int decayMs, int sustainPct, int releaseMs);
        void onSoundDesignFxChanged(int boostDb, int distortion, int reverbMix, int reverbDecay);
    }

    private static final int PAGE_MODEL = 0;
    private static final int PAGE_ENV = 1;
    private static final int PAGE_MOD = 2;
    private static final int PAGE_FX = 3;

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path path = new Path();
    private final RectF tmp = new RectF();

    private Listener listener;
    private int page = PAGE_MODEL;
    private int instrumentMode = 0;
    private String instrumentName = "VIOLIN";
    private String description = "";
    private String[] parameterNames = new String[]{"CONTROL 1","CONTROL 2","CONTROL 3","VIBRATO"};
    private final int[] modelValues = new int[]{74,74,42,14};

    private int attackMs = 20;
    private int decayMs = 120;
    private int sustainPct = 90;
    private int releaseMs = 300;

    private int boostDb = 0;
    private int distortion = 0;
    private int reverbMix = 28;
    private int reverbDecay = 58;
    private boolean hasReverb = false;

    // 0 none, 1 model tile, 2 envelope handle, 3 mod depth, 4 fx tile
    private int activeKind = 0;
    private int activeIndex = -1;
    private float downY = 0f;
    private int downValue = 0;

    SoundDesignView(Context context) {
        super(context);
        setVisibility(GONE);
        setClickable(true);
        setFocusable(true);
        setBackgroundColor(Color.TRANSPARENT);
        text.setTypeface(android.graphics.Typeface.create(
                "sans", android.graphics.Typeface.NORMAL));
    }

    void setListener(Listener listener) {
        this.listener = listener;
    }

    void setEditorState(int instrumentMode,
                        String instrumentName,
                        String description,
                        String[] names,
                        int[] values,
                        int attackMs,
                        int decayMs,
                        int sustainPct,
                        int releaseMs,
                        int boostDb,
                        int distortion,
                        int reverbMix,
                        int reverbDecay,
                        boolean hasReverb) {
        this.instrumentMode = Math.max(0, instrumentMode);
        this.instrumentName = instrumentName == null ? "MODEL" : instrumentName;
        this.description = description == null ? "" : description;
        if (names != null && names.length >= 4) {
            this.parameterNames = new String[]{names[0],names[1],names[2],names[3]};
        }
        if (values != null) {
            for (int i=0;i<Math.min(4, values.length);i++) {
                modelValues[i] = clamp(values[i], 0, 127);
            }
        }
        this.attackMs = clamp(attackMs, 0, 2000);
        this.decayMs = clamp(decayMs, 0, 2000);
        this.sustainPct = clamp(sustainPct, 0, 100);
        this.releaseMs = clamp(releaseMs, 0, 3000);
        this.boostDb = clamp(boostDb, 0, 18);
        this.distortion = clamp(distortion, 0, 127);
        this.reverbMix = clamp(reverbMix, 0, 100);
        this.reverbDecay = clamp(reverbDecay, 0, 100);
        this.hasReverb = hasReverb;
        page = PAGE_MODEL;
        activeKind = 0;
        activeIndex = -1;
        invalidate();
    }

    void setModelValue(int index, int value) {
        if (index < 0 || index >= 4) return;
        modelValues[index] = clamp(value, 0, 127);
        invalidate();
    }

    private static int clamp(int v, int lo, int hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    private float unit() {
        return Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
    }

    private int fg() {
        return Color.rgb(244, 237, 224);
    }

    private RectF closeRect() {
        float u = unit();
        return new RectF(getWidth()-102f*u, 14f*u, getWidth()-14f*u, 50f*u);
    }

    private RectF tabRect(int index) {
        float u = unit();
        float left = 16f*u;
        float gap = 7f*u;
        float total = getWidth() - 32f*u;
        float w = (total - gap*3f) / 4f;
        float x = left + index*(w+gap);
        return new RectF(x, 68f*u, x+w, 104f*u);
    }

    private RectF contentRect() {
        float u = unit();
        return new RectF(14f*u, 124f*u, getWidth()-14f*u, getHeight()-14f*u);
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float u = unit();
        canvas.drawColor(Color.rgb(7, 7, 7));

        // Very subtle depth field; no gradients or Android widget chrome.
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(1f, u));
        paint.setColor(Color.argb(24, 244, 237, 224));
        for (int i=0;i<8;i++) {
            float y = (116f + i*78f)*u;
            canvas.drawLine(0,y,getWidth(),y,paint);
        }

        text.setColor(fg());
        text.setTextSize(21f*u);
        canvas.drawText(instrumentName, 16f*u, 34f*u, text);

        text.setTextSize(10.5f*u);
        text.setColor(Color.argb(160, 244, 237, 224));
        String desc = description.replace("\n", " · ");
        if (desc.length() > 82) desc = desc.substring(0, 79) + "…";
        canvas.drawText(desc, 16f*u, 53f*u, text);

        drawClose(canvas, u);
        drawTabs(canvas, u);

        switch (page) {
            case PAGE_MODEL: drawModelPage(canvas, u); break;
            case PAGE_ENV: drawEnvelopePage(canvas, u); break;
            case PAGE_MOD: drawModPage(canvas, u); break;
            case PAGE_FX: drawFxPage(canvas, u); break;
        }
    }

    private void drawClose(Canvas c, float u) {
        RectF r = closeRect();
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(60, 244, 237, 224));
        c.drawRoundRect(r, 7f*u, 7f*u, paint);
        text.setTextSize(11.5f*u);
        text.setColor(fg());
        String s = "CLOSE";
        float tw = text.measureText(s);
        c.drawText(s, r.centerX()-tw/2f, r.centerY()+4f*u, text);
    }

    private void drawTabs(Canvas c, float u) {
        String[] tabs = {"MODEL","ENV","MOD","FX"};
        for (int i=0;i<4;i++) {
            RectF r = tabRect(i);
            boolean selected = i == page;
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(selected
                    ? Color.argb(210, 244, 237, 224)
                    : Color.argb(42, 244, 237, 224));
            c.drawRoundRect(r, 7f*u, 7f*u, paint);
            text.setTextSize(12f*u);
            text.setColor(selected ? Color.rgb(20,20,19) : fg());
            float tw = text.measureText(tabs[i]);
            c.drawText(tabs[i], r.centerX()-tw/2f, r.centerY()+4f*u, text);
        }
    }

    private RectF modelTile(int index) {
        RectF cr = contentRect();
        float u = unit();
        float gap = 10f*u;
        float w = (cr.width() - gap) / 2f;
        float h = (cr.height() - gap) / 2f;
        int col = index & 1;
        int row = index >> 1;
        return new RectF(cr.left + col*(w+gap),
                cr.top + row*(h+gap),
                cr.left + col*(w+gap) + w,
                cr.top + row*(h+gap) + h);
    }

    private void drawModelPage(Canvas c, float u) {
        for (int i=0;i<4;i++) {
            drawModelTile(c, i, modelTile(i), u);
        }
    }

    private void drawModelTile(Canvas c, int index, RectF r, float u) {
        boolean active = activeKind == 1 && activeIndex == index;
        float n = modelValues[index] / 127f;

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(active ? 90 : 44, 244, 237, 224));
        c.drawRoundRect(r, 13f*u, 13f*u, paint);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(active ? 2.0f*u : 1.0f*u);
        paint.setColor(Color.argb(active ? 190 : 82, 244, 237, 224));
        c.drawRoundRect(r, 13f*u, 13f*u, paint);

        text.setColor(fg());
        text.setTextSize(12.5f*u);
        c.drawText(parameterNames[index], r.left+12f*u, r.top+22f*u, text);

        String value = Integer.toString(modelValues[index]);
        text.setTextSize(18f*u);
        float vw = text.measureText(value);
        c.drawText(value, r.right-12f*u-vw, r.top+23f*u, text);

        RectF g = new RectF(r.left+18f*u, r.top+44f*u,
                r.right-18f*u, r.bottom-22f*u);
        drawModelGraphic(c,index,g,n,u);

        text.setTextSize(9f*u);
        text.setColor(Color.argb(active ? 180 : 105, 244, 237, 224));
        c.drawText(active ? "DRAG ↑↓" : "TOUCH + DRAG",
                r.left+12f*u, r.bottom-9f*u, text);
    }

    private void drawModelGraphic(Canvas c, int index, RectF r, float n, float u) {
        final int bright = Color.argb(220,244,237,224);
        final int dim = Color.argb(50,244,237,224);
        float cx = r.centerX(), cy = r.centerY();

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(1.5f*u);

        if (index == 0) {
            float rr = Math.min(r.width(),r.height())*0.42f;
            paint.setColor(dim);
            for(int i=1;i<=4;i++) c.drawCircle(cx,cy,rr*i/4f,paint);
            paint.setColor(bright);
            paint.setStrokeWidth(2.2f*u);
            c.drawCircle(cx,cy,rr*(0.18f+0.82f*n),paint);
            float angle = (float)(-Math.PI*0.75 + n*Math.PI*1.5);
            paint.setStyle(Paint.Style.FILL);
            c.drawCircle(cx+(float)Math.cos(angle)*rr*0.78f,
                    cy+(float)Math.sin(angle)*rr*0.78f,4f*u,paint);
            return;
        }

        if (index == 1) {
            float phase = (SystemClock.uptimeMillis()%2800L)/2800f*(float)(Math.PI*2);
            paint.setColor(dim);
            c.drawLine(r.left,cy,r.right,cy,paint);
            path.reset();
            for(int p=0;p<=64;p++){
                float q=p/64f;
                float x=r.left+q*r.width();
                float amp=r.height()*(0.05f+0.37f*n);
                float y=cy+(float)Math.sin(q*Math.PI*5.0+phase)*amp;
                if(p==0)path.moveTo(x,y);else path.lineTo(x,y);
            }
            paint.setColor(bright);
            paint.setStrokeWidth(1.8f*u);
            c.drawPath(path,paint);
            postInvalidateOnAnimation();
            return;
        }

        if (index == 2) {
            paint.setColor(dim);
            for(int i=0;i<7;i++){
                float x=r.left+r.width()*i/6f;
                c.drawLine(x,r.top,x,r.bottom,paint);
            }
            float x=r.left+r.width()*n;
            paint.setColor(bright);
            paint.setStrokeWidth(2.4f*u);
            c.drawLine(x,r.top,x,r.bottom,paint);
            paint.setStyle(Paint.Style.FILL);
            c.drawCircle(x,cy,6f*u,paint);
            paint.setColor(Color.argb(45,244,237,224));
            c.drawCircle(x,cy,(12f+20f*n)*u,paint);
            return;
        }

        // Modulation parameter: live waveform, rate is fixed in DSP.
        float phase=(SystemClock.uptimeMillis()%1000L)/1000f*(float)(Math.PI*2);
        paint.setColor(dim);
        c.drawLine(r.left,cy,r.right,cy,paint);
        path.reset();
        for(int p=0;p<=64;p++){
            float q=p/64f;
            float x=r.left+q*r.width();
            float amp=r.height()*0.42f*n;
            float y=cy+(float)Math.sin(q*Math.PI*4.0+phase)*amp;
            if(p==0)path.moveTo(x,y);else path.lineTo(x,y);
        }
        paint.setColor(bright);
        paint.setStrokeWidth(2.0f*u);
        c.drawPath(path,paint);
        postInvalidateOnAnimation();
    }

    private RectF envPlot() {
        RectF cr = contentRect();
        float u = unit();
        return new RectF(cr.left+28f*u, cr.top+40f*u,
                cr.right-28f*u, cr.bottom-86f*u);
    }

    private float attackX(RectF r) {
        return r.left + r.width()*(0.04f + 0.22f*(attackMs/2000f));
    }

    private float decayX(RectF r) {
        float ax=attackX(r);
        return ax + r.width()*(0.06f + 0.20f*(decayMs/2000f));
    }

    private float releaseStartX(RectF r) {
        return r.left + r.width()*0.72f;
    }

    private float releaseX(RectF r) {
        return releaseStartX(r) + r.width()*(0.05f+0.20f*(releaseMs/3000f));
    }

    private float sustainY(RectF r) {
        return r.bottom - r.height()*(sustainPct/100f);
    }

    private void drawEnvelopePage(Canvas c, float u) {
        RectF r = envPlot();

        text.setTextSize(12f*u);
        text.setColor(Color.argb(170,244,237,224));
        c.drawText("DRAG THE ENVELOPE POINTS", r.left, r.top-14f*u, text);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(1f*u);
        paint.setColor(Color.argb(42,244,237,224));
        for(int i=0;i<=4;i++){
            float y=r.top+r.height()*i/4f;
            c.drawLine(r.left,y,r.right,y,paint);
        }
        for(int i=0;i<=8;i++){
            float x=r.left+r.width()*i/8f;
            c.drawLine(x,r.top,x,r.bottom,paint);
        }

        float ax=attackX(r);
        float dx=decayX(r);
        float sy=sustainY(r);
        float rs=releaseStartX(r);
        float rx=releaseX(r);

        // Fill under the envelope.
        path.reset();
        path.moveTo(r.left,r.bottom);
        path.lineTo(ax,r.top);
        path.lineTo(dx,sy);
        path.lineTo(rs,sy);
        path.lineTo(rx,r.bottom);
        path.close();
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(34,244,237,224));
        c.drawPath(path,paint);

        path.reset();
        path.moveTo(r.left,r.bottom);
        path.lineTo(ax,r.top);
        path.lineTo(dx,sy);
        path.lineTo(rs,sy);
        path.lineTo(rx,r.bottom);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(2.6f*u);
        paint.setColor(Color.argb(225,244,237,224));
        c.drawPath(path,paint);

        drawEnvHandle(c,0,ax,r.top,u);
        drawEnvHandle(c,1,dx,sy,u);
        drawEnvHandle(c,2,rs,sy,u);
        drawEnvHandle(c,3,rx,r.bottom,u);

        RectF cr=contentRect();
        float y=cr.bottom-44f*u;
        String[] labels={
                "A  "+attackMs+" ms",
                "D  "+decayMs+" ms",
                "S  "+sustainPct+"%",
                "R  "+releaseMs+" ms"
        };
        float col=cr.width()/4f;
        for(int i=0;i<4;i++){
            text.setTextSize(12f*u);
            text.setColor(fg());
            float tw=text.measureText(labels[i]);
            c.drawText(labels[i],cr.left+i*col+(col-tw)/2f,y,text);
        }
    }

    private void drawEnvHandle(Canvas c,int index,float x,float y,float u){
        boolean active=activeKind==2&&activeIndex==index;
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(active?Color.rgb(244,237,224):Color.argb(205,244,237,224));
        c.drawCircle(x,y,active?8f*u:6f*u,paint);
        if(active){
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(1.4f*u);
            paint.setColor(Color.argb(100,244,237,224));
            c.drawCircle(x,y,16f*u,paint);
        }
    }

    private void drawModPage(Canvas c,float u){
        RectF cr=contentRect();
        RectF r=new RectF(cr.left+24f*u,cr.top+68f*u,
                cr.right-24f*u,cr.bottom-100f*u);
        float n=modelValues[3]/127f;
        float cy=r.centerY();

        text.setColor(Color.argb(175,244,237,224));
        text.setTextSize(12f*u);
        c.drawText(parameterNames[3]+" · 5.2 Hz FIXED",r.left,r.top-24f*u,text);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(1f*u);
        paint.setColor(Color.argb(45,244,237,224));
        c.drawLine(r.left,cy,r.right,cy,paint);
        for(int i=0;i<=8;i++){
            float x=r.left+r.width()*i/8f;
            c.drawLine(x,r.top,x,r.bottom,paint);
        }

        float phase=(SystemClock.uptimeMillis()%1000L)/1000f*(float)(Math.PI*2);
        path.reset();
        for(int i=0;i<=120;i++){
            float q=i/120f;
            float x=r.left+q*r.width();
            float amp=r.height()*0.43f*n;
            float y=cy+(float)Math.sin(q*Math.PI*8.0+phase)*amp;
            if(i==0)path.moveTo(x,y);else path.lineTo(x,y);
        }
        paint.setColor(Color.argb(230,244,237,224));
        paint.setStrokeWidth(2.4f*u);
        c.drawPath(path,paint);

        // A moving point gives direct feedback that this is modulation, not a static EQ curve.
        float q=(SystemClock.uptimeMillis()%1400L)/1400f;
        float px=r.left+q*r.width();
        float py=cy+(float)Math.sin(q*Math.PI*8.0+phase)*r.height()*0.43f*n;
        paint.setStyle(Paint.Style.FILL);
        c.drawCircle(px,py,6f*u,paint);

        text.setColor(fg());
        text.setTextSize(28f*u);
        String val=Integer.toString(modelValues[3]);
        float tw=text.measureText(val);
        c.drawText(val,cr.centerX()-tw/2f,cr.bottom-50f*u,text);

        text.setTextSize(10.5f*u);
        text.setColor(Color.argb(150,244,237,224));
        String hint=activeKind==3?"DRAG ↑↓ TO SET DEPTH":"TOUCH WAVE + DRAG ↑↓";
        tw=text.measureText(hint);
        c.drawText(hint,cr.centerX()-tw/2f,cr.bottom-22f*u,text);
        postInvalidateOnAnimation();
    }

    private RectF fxTile(int index,int count){
        RectF cr=contentRect();
        float u=unit();
        float gap=10f*u;
        if(count<=2){
            float h=(cr.height()-gap)/2f;
            return new RectF(cr.left,cr.top+index*(h+gap),cr.right,cr.top+index*(h+gap)+h);
        }
        float w=(cr.width()-gap)/2f;
        float h=(cr.height()-gap)/2f;
        int col=index&1,row=index>>1;
        return new RectF(cr.left+col*(w+gap),cr.top+row*(h+gap),
                cr.left+col*(w+gap)+w,cr.top+row*(h+gap)+h);
    }

    private void drawFxPage(Canvas c,float u){
        int count=hasReverb?4:2;
        String[] names=hasReverb
                ?new String[]{"BOOST","DISTORTION","REVERB MIX","REVERB DECAY"}
                :new String[]{"BOOST","DISTORTION"};
        int[] values=hasReverb
                ?new int[]{boostDb,distortion,reverbMix,reverbDecay}
                :new int[]{boostDb,distortion};
        int[] max=hasReverb
                ?new int[]{18,127,100,100}
                :new int[]{18,127};

        for(int i=0;i<count;i++){
            RectF r=fxTile(i,count);
            boolean active=activeKind==4&&activeIndex==i;
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(Color.argb(active?88:44,244,237,224));
            c.drawRoundRect(r,13f*u,13f*u,paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(active?2f*u:1f*u);
            paint.setColor(Color.argb(active?190:80,244,237,224));
            c.drawRoundRect(r,13f*u,13f*u,paint);

            text.setTextSize(12.5f*u);
            text.setColor(fg());
            c.drawText(names[i],r.left+12f*u,r.top+22f*u,text);
            String v=i==0?values[i]+" dB":Integer.toString(values[i]);
            text.setTextSize(18f*u);
            float tw=text.measureText(v);
            c.drawText(v,r.right-12f*u-tw,r.top+23f*u,text);

            RectF g=new RectF(r.left+18f*u,r.top+44f*u,r.right-18f*u,r.bottom-18f*u);
            drawFxGraphic(c,i,g,values[i]/(float)max[i],u);
        }
    }

    private void drawFxGraphic(Canvas c,int index,RectF r,float n,float u){
        int bright=Color.argb(220,244,237,224);
        int dim=Color.argb(48,244,237,224);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(1.5f*u);
        float cx=r.centerX(),cy=r.centerY();

        if(index==0){
            paint.setColor(dim);
            for(int i=0;i<6;i++){
                float rr=Math.min(r.width(),r.height())*(0.10f+0.065f*i);
                c.drawArc(new RectF(cx-rr,cy-rr,cx+rr,cy+rr),210,120,false,paint);
            }
            float rr=Math.min(r.width(),r.height())*(0.12f+0.28f*n);
            paint.setColor(bright);
            paint.setStrokeWidth(2.5f*u);
            c.drawArc(new RectF(cx-rr,cy-rr,cx+rr,cy+rr),205,130,false,paint);
            return;
        }

        if(index==1){
            paint.setColor(dim);
            c.drawLine(r.left,cy,r.right,cy,paint);
            path.reset();
            for(int p=0;p<=64;p++){
                float q=p/64f;
                float x=r.left+q*r.width();
                float raw=(float)Math.sin(q*Math.PI*4.0);
                float shaped=(float)Math.tanh(raw*(1.0+6.0*n));
                float y=cy-shaped*r.height()*0.38f;
                if(p==0)path.moveTo(x,y);else path.lineTo(x,y);
            }
            paint.setColor(bright);
            paint.setStrokeWidth(2.0f*u);
            c.drawPath(path,paint);
            return;
        }

        if(index==2){
            float maxR=Math.min(r.width(),r.height())*0.42f;
            paint.setColor(dim);
            for(int i=1;i<=4;i++)c.drawCircle(cx,cy,maxR*i/4f,paint);
            paint.setColor(bright);
            paint.setStrokeWidth(2.2f*u);
            c.drawCircle(cx,cy,maxR*(0.18f+0.82f*n),paint);
            return;
        }

        // Reverb decay trail.
        paint.setColor(dim);
        for(int i=0;i<10;i++){
            float x=r.left+r.width()*i/9f;
            c.drawLine(x,r.top,x,r.bottom,paint);
        }
        path.reset();
        for(int p=0;p<=80;p++){
            float q=p/80f;
            float x=r.left+q*r.width();
            float decay=(float)Math.exp(-q*(1.2+7.0*(1.0-n)));
            float y=cy+(float)Math.sin(q*Math.PI*12.0)*decay*r.height()*0.42f;
            if(p==0)path.moveTo(x,y);else path.lineTo(x,y);
        }
        paint.setColor(bright);
        paint.setStrokeWidth(1.8f*u);
        c.drawPath(path,paint);
    }

    private int nearestEnvHandle(float x,float y){
        RectF r=envPlot();
        float[] xs={attackX(r),decayX(r),releaseStartX(r),releaseX(r)};
        float sy=sustainY(r);
        float[] ys={r.top,sy,sy,r.bottom};
        int best=0;
        float bestD=Float.MAX_VALUE;
        for(int i=0;i<4;i++){
            float dx=x-xs[i],dy=y-ys[i];
            float d=dx*dx+dy*dy;
            if(d<bestD){bestD=d;best=i;}
        }
        return best;
    }

    private void updateEnvelope(int index,float x,float y){
        RectF r=envPlot();
        if(index==0){
            float q=(x-r.left-r.width()*0.04f)/(r.width()*0.22f);
            attackMs=clamp(Math.round(q*2000f),0,2000);
        }else if(index==1){
            float ax=attackX(r);
            float q=(x-ax-r.width()*0.06f)/(r.width()*0.20f);
            decayMs=clamp(Math.round(q*2000f),0,2000);
        }else if(index==2){
            float q=1f-(y-r.top)/Math.max(1f,r.height());
            sustainPct=clamp(Math.round(q*100f),0,100);
        }else if(index==3){
            float rs=releaseStartX(r);
            float q=(x-rs-r.width()*0.05f)/(r.width()*0.20f);
            releaseMs=clamp(Math.round(q*3000f),0,3000);
        }
        if(listener!=null)listener.onSoundDesignAdsrChanged(
                attackMs,decayMs,sustainPct,releaseMs);
        invalidate();
    }

    private int modelTileAt(float x,float y){
        for(int i=0;i<4;i++)if(modelTile(i).contains(x,y))return i;
        return -1;
    }

    private int fxTileAt(float x,float y){
        int count=hasReverb?4:2;
        for(int i=0;i<count;i++)if(fxTile(i,count).contains(x,y))return i;
        return -1;
    }

    private int fxValue(int index){
        if(index==0)return boostDb;
        if(index==1)return distortion;
        if(index==2)return reverbMix;
        return reverbDecay;
    }

    private int fxMax(int index){
        if(index==0)return 18;
        if(index==1)return 127;
        return 100;
    }

    private void setFxValue(int index,int value){
        if(index==0)boostDb=clamp(value,0,18);
        else if(index==1)distortion=clamp(value,0,127);
        else if(index==2)reverbMix=clamp(value,0,100);
        else if(index==3)reverbDecay=clamp(value,0,100);
        if(listener!=null)listener.onSoundDesignFxChanged(
                boostDb,distortion,reverbMix,reverbDecay);
        invalidate();
    }

    private void updateRelative(float y){
        float travel=Math.max(80f*unit(),getHeight()*0.36f);
        if(activeKind==1){
            int next=clamp(Math.round(downValue+(downY-y)/travel*127f),0,127);
            if(next!=modelValues[activeIndex]){
                modelValues[activeIndex]=next;
                if(listener!=null)listener.onSoundDesignModelParameterChanged(activeIndex,next);
                invalidate();
            }
        }else if(activeKind==3){
            int next=clamp(Math.round(downValue+(downY-y)/travel*127f),0,127);
            if(next!=modelValues[3]){
                modelValues[3]=next;
                if(listener!=null)listener.onSoundDesignModelParameterChanged(3,next);
                invalidate();
            }
        }else if(activeKind==4){
            int max=fxMax(activeIndex);
            int next=clamp(Math.round(downValue+(downY-y)/travel*max),0,max);
            if(next!=fxValue(activeIndex))setFxValue(activeIndex,next);
        }
    }

    @Override public boolean onTouchEvent(MotionEvent event){
        int action=event.getActionMasked();
        float x=event.getX(event.getActionIndex());
        float y=event.getY(event.getActionIndex());

        if(action==MotionEvent.ACTION_DOWN){
            if(closeRect().contains(x,y)){
                if(listener!=null)listener.onSoundDesignClose();
                return true;
            }
            for(int i=0;i<4;i++){
                if(tabRect(i).contains(x,y)){
                    page=i;
                    activeKind=0;
                    activeIndex=-1;
                    invalidate();
                    return true;
                }
            }

            if(page==PAGE_MODEL){
                int target=modelTileAt(x,y);
                if(target>=0){
                    activeKind=1;activeIndex=target;
                    downY=y;downValue=modelValues[target];
                    invalidate();
                    return true;
                }
            }else if(page==PAGE_ENV){
                if(envPlot().contains(x,y)){
                    activeKind=2;
                    activeIndex=nearestEnvHandle(x,y);
                    updateEnvelope(activeIndex,x,y);
                    return true;
                }
            }else if(page==PAGE_MOD){
                if(contentRect().contains(x,y)){
                    activeKind=3;activeIndex=3;
                    downY=y;downValue=modelValues[3];
                    invalidate();
                    return true;
                }
            }else if(page==PAGE_FX){
                int target=fxTileAt(x,y);
                if(target>=0){
                    activeKind=4;activeIndex=target;
                    downY=y;downValue=fxValue(target);
                    invalidate();
                    return true;
                }
            }
            return true;
        }

        if(action==MotionEvent.ACTION_MOVE){
            if(activeKind==2){
                updateEnvelope(activeIndex,event.getX(0),event.getY(0));
            }else if(activeKind!=0){
                updateRelative(event.getY(0));
            }
            return true;
        }

        if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_CANCEL){
            activeKind=0;
            activeIndex=-1;
            invalidate();
            performClick();
            return true;
        }
        return true;
    }

    @Override public boolean performClick(){
        super.performClick();
        return true;
    }
}
