package com.example.epsampler;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

final class PerformanceXYView extends View {
    interface Listener {
        void onPerformanceXY(boolean active, int part, int x, int y);
    }

    private final PianoView pianoView;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Listener listener;
    private int instrumentMode = 0;
    private int activePointerId = -1;
    private boolean active = false;
    private float touchX = 0f;
    private float touchY = 0f;
    private int valueX = 64;
    private int valueY = 64;

    PerformanceXYView(Context context, PianoView pianoView) {
        super(context);
        this.pianoView = pianoView;
        setBackgroundColor(Color.TRANSPARENT);
        text.setTypeface(android.graphics.Typeface.create("sans", android.graphics.Typeface.NORMAL));
    }

    void setListener(Listener listener) {
        this.listener = listener;
    }

    void setInstrumentMode(int mode) {
        int next = Math.max(0, Math.min(7, mode));
        if (active && next != instrumentMode) releaseEffect();
        instrumentMode = next;
        invalidate();
    }

    private float unit() {
        return Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
    }

    private void updateValues(float x, float y) {
        float u = unit();
        float left = 8f*u;
        float right = getWidth() - 8f*u;
        float top = 116f*u;
        float bottom = getHeight() - 8f*u;

        float nx = (x - left) / Math.max(1f, right - left);
        float ny = 1f - ((y - top) / Math.max(1f, bottom - top));
        nx = Math.max(0f, Math.min(1f, nx));
        ny = Math.max(0f, Math.min(1f, ny));

        valueX = Math.round(nx * 127f);
        valueY = Math.round(ny * 127f);
        touchX = x;
        touchY = y;

        if (listener != null) listener.onPerformanceXY(true, instrumentMode, valueX, valueY);
        invalidate();
    }

    private void releaseEffect() {
        if (!active) return;
        active = false;
        activePointerId = -1;
        if (listener != null) listener.onPerformanceXY(false, instrumentMode, valueX, valueY);
        invalidate();
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!active) return;

        float u = unit();
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(2f*u);
        paint.setColor(Color.argb(205, 246, 236, 212));
        canvas.drawCircle(touchX, touchY, 20f*u, paint);
        canvas.drawLine(touchX-28f*u, touchY, touchX+28f*u, touchY, paint);
        canvas.drawLine(touchX, touchY-28f*u, touchX, touchY+28f*u, paint);
        paint.setStyle(Paint.Style.FILL);

        String label = instrumentMode == 7 ? "STUTTER" : "DELAY";
        text.setTextSize(13f*u);
        text.setColor(Color.argb(235, 246, 236, 212));
        String value = label + "  X" + valueX + " Y" + valueY;
        float tx = Math.max(8f*u, Math.min(getWidth() - text.measureText(value) - 8f*u,
                touchX + 26f*u));
        float ty = Math.max(128f*u, touchY - 24f*u);
        canvas.drawText(value, tx, ty, text);
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        final int index = event.getActionIndex();

        if (action == MotionEvent.ACTION_DOWN) {
            float x = event.getX(index);
            float y = event.getY(index);
            if (pianoView != null && pianoView.blocksPerformanceXY(x, y)) return false;

            active = true;
            activePointerId = event.getPointerId(index);
            updateValues(x, y);
            return true;
        }

        if (!active) return false;

        if (action == MotionEvent.ACTION_MOVE) {
            int pointerIndex = event.findPointerIndex(activePointerId);
            if (pointerIndex >= 0) {
                float x = event.getX(pointerIndex);
                float y = event.getY(pointerIndex);
                if (pianoView == null || !pianoView.blocksPerformanceXY(x, y)) {
                    updateValues(x, y);
                }
            }
            return true;
        }

        if (action == MotionEvent.ACTION_UP ||
                action == MotionEvent.ACTION_POINTER_UP ||
                action == MotionEvent.ACTION_CANCEL) {
            if (action == MotionEvent.ACTION_CANCEL ||
                    event.getPointerId(index) == activePointerId) {
                releaseEffect();
            }
            return true;
        }

        return true;
    }
}
