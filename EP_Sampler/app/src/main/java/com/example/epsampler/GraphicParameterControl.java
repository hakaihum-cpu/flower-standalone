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
 * Encoder-like parameter control without a knob or slider track.
 * Drag vertically to change the value; the value is represented by a live graphic.
 */
final class GraphicParameterControl extends View {
    interface Listener {
        void onValueChanged(int value);
    }

    static final int STYLE_WAVE = 0;
    static final int STYLE_ORBIT = 1;
    static final int STYLE_FIELD = 2;
    static final int STYLE_PULSE = 3;

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path path = new Path();
    private final RectF card = new RectF();

    private String label = "PARAM";
    private int max = 100;
    private int value = 50;
    private int style = STYLE_WAVE;
    private Listener listener;

    private float downY;
    private int downValue;
    private boolean dragging;

    GraphicParameterControl(Context context) {
        super(context);
        setClickable(true);
        setFocusable(true);
        text.setTypeface(android.graphics.Typeface.create(
                "sans", android.graphics.Typeface.NORMAL));
        setBackgroundColor(Color.TRANSPARENT);
    }

    void configure(String label, int max, int value, int style, Listener listener) {
        this.label = label == null ? "PARAM" : label;
        this.max = Math.max(1, max);
        this.value = clamp(value);
        this.style = Math.floorMod(style, 4);
        this.listener = listener;
        updateDescription();
        invalidate();
    }

    void setLabel(String label) {
        this.label = label == null ? "PARAM" : label;
        updateDescription();
        invalidate();
    }

    void setValue(int value) {
        this.value = clamp(value);
        updateDescription();
        invalidate();
    }

    int getValue() {
        return value;
    }

    private int clamp(int v) {
        return Math.max(0, Math.min(max, v));
    }

    private float norm() {
        return value / (float) max;
    }

    private float u() {
        return Math.max(0.75f, getResources().getDisplayMetrics().density);
    }

    private void updateDescription() {
        setContentDescription(label + " " + value + " of " + max);
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float d = u();
        float w = getWidth();
        float h = getHeight();
        float pad = 8f * d;

        card.set(pad * 0.25f, pad * 0.2f, w - pad * 0.25f, h - pad * 0.2f);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(dragging ? 88 : 48, 244, 237, 224));
        canvas.drawRoundRect(card, 12f*d, 12f*d, paint);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(1f, 1.1f*d));
        paint.setColor(Color.argb(dragging ? 185 : 92, 244, 237, 224));
        canvas.drawRoundRect(card, 12f*d, 12f*d, paint);

        text.setColor(Color.argb(238, 244, 237, 224));
        text.setTextSize(13.5f*d);
        canvas.drawText(label, card.left + 12f*d, card.top + 22f*d, text);

        String v = Integer.toString(value);
        text.setTextSize(18f*d);
        float vw = text.measureText(v);
        canvas.drawText(v, card.right - 12f*d - vw, card.top + 23f*d, text);

        RectF g = new RectF(card.left + 12f*d, card.top + 34f*d,
                card.right - 12f*d, card.bottom - 9f*d);
        drawGraphic(canvas, g, norm(), d);

        if (dragging) {
            text.setTextSize(9.5f*d);
            text.setColor(Color.argb(150, 244, 237, 224));
            canvas.drawText("↑↓", card.right - 24f*d, card.bottom - 9f*d, text);
        }

        if (style == STYLE_WAVE || style == STYLE_PULSE) {
            postInvalidateOnAnimation();
        }
    }

    private void drawGraphic(Canvas canvas, RectF r, float n, float d) {
        final int fg = Color.argb(215, 244, 237, 224);
        final int dim = Color.argb(52, 244, 237, 224);
        paint.setStrokeWidth(Math.max(1.2f, 1.6f*d));
        paint.setStyle(Paint.Style.STROKE);

        if (style == STYLE_ORBIT) {
            float cx = r.centerX(), cy = r.centerY();
            float maxR = Math.min(r.width(), r.height()) * 0.42f;
            paint.setColor(dim);
            canvas.drawCircle(cx, cy, maxR, paint);
            canvas.drawCircle(cx, cy, maxR * 0.58f, paint);

            float rr = maxR * (0.20f + 0.80f*n);
            float phase = (SystemClock.uptimeMillis() % 6000L) / 6000f
                    * (float)(Math.PI * 2.0);
            float px = cx + (float)Math.cos(phase) * rr;
            float py = cy + (float)Math.sin(phase) * rr;
            paint.setColor(fg);
            canvas.drawCircle(cx, cy, rr, paint);
            paint.setStyle(Paint.Style.FILL);
            canvas.drawCircle(px, py, 3.5f*d, paint);
            postInvalidateOnAnimation();
            return;
        }

        if (style == STYLE_FIELD) {
            paint.setColor(dim);
            for (int i=0;i<5;i++) {
                float y = r.top + r.height() * i / 4f;
                canvas.drawLine(r.left, y, r.right, y, paint);
            }
            float x = r.left + r.width() * n;
            paint.setColor(fg);
            paint.setStrokeWidth(2.2f*d);
            canvas.drawLine(x, r.top, x, r.bottom, paint);
            paint.setStyle(Paint.Style.FILL);
            float cy = r.centerY();
            canvas.drawCircle(x, cy, 5.0f*d, paint);
            paint.setColor(Color.argb(48, 244, 237, 224));
            canvas.drawCircle(x, cy, (8f + 16f*n)*d, paint);
            return;
        }

        if (style == STYLE_PULSE) {
            float t = (SystemClock.uptimeMillis() % 1600L) / 1600f;
            float pulse = 0.5f + 0.5f*(float)Math.sin(t * Math.PI * 2.0);
            float cx = r.centerX(), cy = r.centerY();
            float radius = Math.min(r.width(), r.height()) * (0.12f + 0.30f*n);
            paint.setColor(dim);
            for (int i=1;i<=3;i++) {
                canvas.drawCircle(cx, cy, radius * i / 3f, paint);
            }
            paint.setColor(fg);
            paint.setStrokeWidth((1.2f + 2.0f*pulse*n)*d);
            canvas.drawCircle(cx, cy, radius * (0.70f + 0.30f*pulse), paint);
            paint.setStyle(Paint.Style.FILL);
            canvas.drawCircle(cx, cy, 3.5f*d + 2.5f*d*n, paint);
            return;
        }

        // STYLE_WAVE
        float phase = (SystemClock.uptimeMillis() % 2600L) / 2600f
                * (float)(Math.PI * 2.0);
        paint.setColor(dim);
        canvas.drawLine(r.left, r.centerY(), r.right, r.centerY(), paint);
        path.reset();
        int points = 48;
        for (int i=0;i<=points;i++) {
            float q = i / (float)points;
            float x = r.left + q*r.width();
            float amp = r.height() * (0.06f + 0.38f*n);
            float y = r.centerY() + (float)Math.sin(q*Math.PI*4.0 + phase)*amp;
            if (i==0) path.moveTo(x,y); else path.lineTo(x,y);
        }
        paint.setColor(fg);
        paint.setStrokeWidth(1.8f*d);
        canvas.drawPath(path, paint);
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                dragging = true;
                downY = event.getY();
                downValue = value;
                getParent().requestDisallowInterceptTouchEvent(true);
                invalidate();
                return true;
            case MotionEvent.ACTION_MOVE: {
                float dy = downY - event.getY();
                float range = Math.max(72f, getHeight() * 0.82f);
                int next = clamp(Math.round(downValue + dy / range * max * 1.35f));
                if (next != value) {
                    value = next;
                    updateDescription();
                    if (listener != null) listener.onValueChanged(value);
                    invalidate();
                }
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                dragging = false;
                getParent().requestDisallowInterceptTouchEvent(false);
                performClick();
                invalidate();
                return true;
            default:
                return true;
        }
    }

    @Override public boolean performClick() {
        super.performClick();
        return true;
    }
}
