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
    }

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Listener listener;
    private int activeParameter = -1;

    private final int[] values = new int[]{
            64, 58, 46, 38,
            64, 43, 74, 56,
            64, 53, 74, 58
    };

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

    void setDrumsVisible(boolean visible) {
        activeParameter = -1;
        setVisibility(visible ? VISIBLE : GONE);
        if (visible) invalidate();
    }

    private RectF panelRect() {
        float u = unit();
        return new RectF(8f*u, 126f*u, getWidth()-8f*u, 352f*u);
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
        float rowH = (panel.height() - 18f*u) / 3f;

        for (int row=0; row<3; row++) {
            float y0 = panel.top + 8f*u + row*rowH;

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
    }

    private int parameterAt(float x, float y) {
        RectF panel = panelRect();
        if (!panel.contains(x,y)) return -1;

        float u = unit();
        float controlsLeft = panel.left + 10f*u + 92f*u;
        if (x < controlsLeft) return -1;

        float usableW = panel.right - controlsLeft - 8f*u;
        float colW = usableW / 4f;
        float rowH = (panel.height() - 18f*u) / 3f;

        int row = Math.max(0, Math.min(2,
                (int)((y - (panel.top + 8f*u)) / rowH)));
        int col = Math.max(0, Math.min(3,
                (int)((x - controlsLeft) / colW)));
        return row*4 + col;
    }

    private int valueAtX(int parameter, float x) {
        RectF panel = panelRect();
        float u = unit();
        float controlsLeft = panel.left + 10f*u + 92f*u;
        float usableW = panel.right - controlsLeft - 8f*u;
        float colW = usableW / 4f;
        int col = parameter % 4;

        float x0 = controlsLeft + col*colW + 4f*u;
        float x1 = controlsLeft + (col+1)*colW - 5f*u;
        float norm = (x - x0) / Math.max(1f, x1-x0);
        return clamp7(Math.round(Math.max(0f, Math.min(1f, norm)) * 127f));
    }

    private void edit(int parameter, float x) {
        if (parameter < 0 || parameter >= values.length) return;
        int value = valueAtX(parameter, x);
        if (values[parameter] == value) return;
        values[parameter] = value;
        if (listener != null) listener.onDrumParameterChanged(parameter, value);
        invalidate();
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        if (getVisibility() != VISIBLE) return false;

        int action = event.getActionMasked();
        int index = event.getActionIndex();

        if (action == MotionEvent.ACTION_DOWN) {
            int parameter = parameterAt(event.getX(index), event.getY(index));
            if (parameter < 0) return false;
            activeParameter = parameter;
            edit(parameter, event.getX(index));
            return true;
        }

        if (action == MotionEvent.ACTION_MOVE && activeParameter >= 0) {
            edit(activeParameter, event.getX(0));
            return true;
        }

        if ((action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL)
                && activeParameter >= 0) {
            if (action == MotionEvent.ACTION_UP) edit(activeParameter, event.getX(index));
            activeParameter = -1;
            return true;
        }

        return activeParameter >= 0;
    }

    private static int clamp7(int value) {
        return Math.max(0, Math.min(127, value));
    }
}
