package com.example.epsampler;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

final class DrumEditorView extends View {
    interface Listener {
        void onDrumParameterChanged(int parameter, int value);
        void onDrumFxChanged(int boostDb, int distortion);
    }

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Listener listener;
    private int activeControl = -1;

    private final int[] values = new int[]{
            64, 58, 46, 38,
            64, 43, 74, 56,
            64, 53, 74, 58
    };
    private int boostDb = 6;
    private int distortion = 0;

    private static final String[] NAMES = {"KICK  C4", "HI-HAT  C#4", "SNARE  D4"};
    private static final String[][] LABELS = {
            {"TUNE","DECAY","BEND","CLICK"},
            {"TUNE","DECAY","COLOR","NOISE"},
            {"TUNE","DECAY","SNAPPY","IMPACT"}
    };

    DrumEditorView(Context context) {
        super(context);
        setBackgroundColor(Color.TRANSPARENT);
        setVisibility(GONE);
        text.setTypeface(android.graphics.Typeface.create("sans", android.graphics.Typeface.NORMAL));
    }

    void setListener(Listener listener) { this.listener = listener; }

    void setValues(int[] source) {
        if (source == null) return;
        for (int i=0; i<values.length && i<source.length; i++) {
            values[i] = clamp7(source[i]);
        }
        invalidate();
    }

    void setDrumFx(int boostDb, int distortion) {
        this.boostDb = Math.max(0, Math.min(18, boostDb));
        this.distortion = clamp7(distortion);
        invalidate();
    }

    void setDrumsVisible(boolean visible) {
        activeControl = -1;
        setVisibility(visible ? VISIBLE : GONE);
        if (visible) invalidate();
    }

    private RectF panelRect() {
        float u = unit();
        return new RectF(8f*u, 126f*u, getWidth()-8f*u, 446f*u);
    }

    private RectF closeRect() {
        float u = unit();
        RectF panel = panelRect();
        return new RectF(panel.right - 74f*u, panel.top + 6f*u,
                panel.right - 10f*u, panel.top + 30f*u);
    }

    private float unit() {
        return Math.max(0.75f, Math.min(getWidth(), getHeight()) / 720f);
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (getVisibility() != VISIBLE) return;

        float u = unit();
        RectF panel = panelRect();

        paint.setColor(Color.argb(150, 0, 0, 0));
        canvas.drawRoundRect(panel, 10f*u, 10f*u, paint);

        float nameW = 92f*u;
        float left = panel.left + 10f*u;
        float controlsLeft = left + nameW;
        float usableW = panel.right - controlsLeft - 8f*u;
        float colW = usableW / 4f;
        float rowH = 72f*u;

        text.setTextSize(11.5f*u);
        text.setColor(Color.argb(225, 244, 237, 224));
        canvas.drawText("DRUM EDITOR", left, panel.top + 23f*u, text);
        RectF close = closeRect();
        paint.setColor(Color.argb(72, 238, 229, 207));
        canvas.drawRoundRect(close, 5f*u, 5f*u, paint);
        String closeText = "CLOSE";
        float closeTextW = text.measureText(closeText);
        canvas.drawText(closeText, close.centerX() - closeTextW/2f, panel.top + 23f*u, text);

        for (int row=0; row<3; row++) {
            float y0 = panel.top + 34f*u + row*rowH;

            text.setTextSize(13f*u);
            text.setColor(Color.argb(245, 244, 237, 224));
            canvas.drawText(NAMES[row], left, y0 + 28f*u, text);

            for (int col=0; col<4; col++) {
                int p = row*4 + col;
                float x0 = controlsLeft + col*colW + 4f*u;
                float x1 = controlsLeft + (col+1)*colW - 5f*u;
                float barY = y0 + 34f*u;
                float barH = 11f*u;

                text.setTextSize(11f*u);
                text.setColor(Color.argb(230, 244, 237, 224));
                canvas.drawText(LABELS[row][col], x0, y0 + 15f*u, text);

                paint.setColor(Color.argb(60, 238, 229, 207));
                canvas.drawRoundRect(new RectF(x0, barY, x1, barY+barH), 4f*u, 4f*u, paint);

                float norm = values[p] / 127f;
                paint.setColor(Color.argb(190, 242, 232, 207));
                canvas.drawRoundRect(new RectF(x0, barY, x0+(x1-x0)*norm, barY+barH),
                        4f*u, 4f*u, paint);

                String value;
                if (col == 0) {
                    float st = (values[p] / 127f - 0.5f) * 24f;
                    value = String.format(java.util.Locale.US, "%+.1f", st);
                } else {
                    value = Integer.toString(values[p]);
                }
                float tw = text.measureText(value);
                canvas.drawText(value, x1-tw, y0 + 15f*u, text);
            }
        }

        float fxTop = panel.top + 252f*u;
        text.setTextSize(12.5f*u);
        text.setColor(Color.argb(245, 244, 237, 224));
        canvas.drawText("DRUM BUS", left, fxTop + 23f*u, text);

        float fxLeft = controlsLeft;
        float gap = 12f*u;
        float fxW = (panel.right - fxLeft - 8f*u - gap) / 2f;
        drawFxControl(canvas, fxLeft, fxTop, fxW, "BOOSTER", boostDb / 18f,
                boostDb + "dB", u);
        drawFxControl(canvas, fxLeft + fxW + gap, fxTop, fxW, "DISTORTION",
                distortion / 127f, Integer.toString(distortion), u);
    }

    private void drawFxControl(Canvas canvas, float x, float y, float w,
                               String label, float norm, String value, float u) {
        text.setTextSize(11.5f*u);
        text.setColor(Color.argb(230, 244, 237, 224));
        canvas.drawText(label, x, y + 15f*u, text);
        float tw = text.measureText(value);
        canvas.drawText(value, x+w-tw, y + 15f*u, text);

        float barY = y + 34f*u;
        paint.setColor(Color.argb(60, 238, 229, 207));
        canvas.drawRoundRect(new RectF(x, barY, x+w, barY+12f*u), 4f*u, 4f*u, paint);
        paint.setColor(Color.argb(205, 242, 232, 207));
        canvas.drawRoundRect(new RectF(x, barY, x+w*Math.max(0f, Math.min(1f, norm)),
                barY+12f*u), 4f*u, 4f*u, paint);
    }

    private int controlAt(float x, float y) {
        RectF panel = panelRect();
        if (!panel.contains(x,y)) return -1;

        float u = unit();
        float left = panel.left + 10f*u;
        float controlsLeft = left + 92f*u;

        if (closeRect().contains(x,y)) return 14;

        float rowStart = panel.top + 34f*u;
        float rowAreaBottom = rowStart + 3f*72f*u;
        if (y >= rowStart && y < rowAreaBottom) {
            if (x < controlsLeft) return -1;
            float usableW = panel.right - controlsLeft - 8f*u;
            float colW = usableW / 4f;
            int row = Math.max(0, Math.min(2,
                    (int)((y - rowStart) / (72f*u))));
            int col = Math.max(0, Math.min(3,
                    (int)((x - controlsLeft) / colW)));
            return row*4 + col;
        }

        float fxTop = panel.top + 252f*u;
        float gap = 12f*u;
        float fxW = (panel.right - controlsLeft - 8f*u - gap) / 2f;
        if (y < fxTop || y > fxTop + 58f*u) return -1;
        if (x >= controlsLeft && x <= controlsLeft + fxW) return 12;
        if (x >= controlsLeft + fxW + gap && x <= panel.right - 8f*u) return 13;
        return -1;
    }

    private int valueAtX(int control, float x) {
        RectF panel = panelRect();
        float u = unit();
        float controlsLeft = panel.left + 10f*u + 92f*u;

        if (control < 12) {
            float usableW = panel.right - controlsLeft - 8f*u;
            float colW = usableW / 4f;
            int col = control % 4;
            float x0 = controlsLeft + col*colW + 4f*u;
            float x1 = controlsLeft + (col+1)*colW - 5f*u;
            float norm = (x - x0) / Math.max(1f, x1-x0);
            return clamp7(Math.round(Math.max(0f, Math.min(1f, norm)) * 127f));
        }

        float gap = 12f*u;
        float fxW = (panel.right - controlsLeft - 8f*u - gap) / 2f;
        float x0 = control == 12 ? controlsLeft : controlsLeft + fxW + gap;
        float x1 = x0 + fxW;
        float norm = Math.max(0f, Math.min(1f, (x - x0) / Math.max(1f, x1-x0)));
        return control == 12 ? Math.round(norm * 18f) : Math.round(norm * 127f);
    }

    private void edit(int control, float x) {
        if (control < 0 || control > 14) return;
        if (control == 14) {
            activeControl = -1;
            setVisibility(GONE);
            return;
        }
        int value = valueAtX(control, x);

        if (control < 12) {
            if (values[control] == value) return;
            values[control] = value;
            if (listener != null) listener.onDrumParameterChanged(control, value);
        } else if (control == 12) {
            if (boostDb == value) return;
            boostDb = value;
            if (listener != null) listener.onDrumFxChanged(boostDb, distortion);
        } else {
            if (distortion == value) return;
            distortion = value;
            if (listener != null) listener.onDrumFxChanged(boostDb, distortion);
        }
        invalidate();
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        if (getVisibility() != VISIBLE) return false;

        int action = event.getActionMasked();
        int index = event.getActionIndex();

        if (action == MotionEvent.ACTION_DOWN) {
            float x = event.getX(index);
            float y = event.getY(index);
            int control = controlAt(x, y);
            if (control < 0) {
                // The editor panel itself blocks the performance XY layer
                // even between controls.
                return panelRect().contains(x, y);
            }
            activeControl = control;
            edit(control, event.getX(index));
            return true;
        }

        if (action == MotionEvent.ACTION_MOVE && activeControl >= 0) {
            edit(activeControl, event.getX(0));
            return true;
        }

        if ((action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL)
                && activeControl >= 0) {
            if (action == MotionEvent.ACTION_UP) edit(activeControl, event.getX(index));
            activeControl = -1;
            return true;
        }

        return activeControl >= 0;
    }

    private static int clamp7(int value) {
        return Math.max(0, Math.min(127, value));
    }
}
