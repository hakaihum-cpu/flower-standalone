package com.example.epsampler;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

/**
 * Compact mixer overview cell. It shows state only; detailed edits are made in
 * MixerView's shared graphical editor below the 4x4 grid.
 */
final class MixerChannelTile extends View {
    interface Listener {
        void onSelect();
    }

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF card = new RectF();

    private String name = "PART";
    private int volume = 127;
    private int pan = 64;
    private boolean muted = false;
    private boolean selected = false;
    private Listener listener;

    MixerChannelTile(Context context) {
        super(context);
        setClickable(true);
        setFocusable(true);
        setBackgroundColor(Color.TRANSPARENT);
        text.setTypeface(android.graphics.Typeface.create(
                "sans", android.graphics.Typeface.NORMAL));
        updateDescription();
    }

    void setListener(Listener listener) {
        this.listener = listener;
    }

    void setState(String name, int volume, int pan, boolean muted, boolean selected) {
        this.name = name == null || name.trim().isEmpty() ? "PART" : name.trim();
        this.volume = clamp7(volume);
        this.pan = clamp7(pan);
        this.muted = muted;
        this.selected = selected;
        updateDescription();
        invalidate();
    }

    void setSelected(boolean selected) {
        this.selected = selected;
        invalidate();
    }

    private void updateDescription() {
        setContentDescription(name + ", volume " + volume + ", " +
                panText(pan) + (muted ? ", muted" : ""));
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        float w = getWidth();
        float h = getHeight();
        float u = Math.max(0.75f, Math.min(w / 168f, h / 112f));
        float pad = 4f * u;

        card.set(pad, pad, w - pad, h - pad);

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(selected
                ? Color.argb(82, 244, 237, 224)
                : Color.argb(34, 244, 237, 224));
        canvas.drawRoundRect(card, 9f*u, 9f*u, paint);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth((selected ? 2.0f : 1.0f) * u);
        paint.setColor(Color.argb(selected ? 205 : 76, 244, 237, 224));
        canvas.drawRoundRect(card, 9f*u, 9f*u, paint);

        if (muted) {
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(Color.argb(205, 244, 237, 224));
            canvas.drawRoundRect(
                    new RectF(card.left, card.top, card.right, card.top + 7f*u),
                    7f*u, 7f*u, paint);
        }

        text.setColor(Color.rgb(244,237,224));
        text.setTextSize(11.5f*u);
        text.setTextAlign(Paint.Align.LEFT);
        String display = name.length() > 12 ? name.substring(0, 12) : name;
        canvas.drawText(display, card.left + 8f*u, card.top + 23f*u, text);

        text.setTextAlign(Paint.Align.RIGHT);
        text.setTextSize(18.5f*u);
        canvas.drawText(Integer.toString(volume),
                card.right - 8f*u, card.top + 25f*u, text);

        text.setTextAlign(Paint.Align.LEFT);
        text.setTextSize(8.5f*u);
        text.setColor(Color.argb(125,244,237,224));
        canvas.drawText("VOL", card.right - 34f*u, card.top + 39f*u, text);

        // Blackbox-like compact pan position line, using the existing monochrome palette.
        float lineLeft = card.left + 10f*u;
        float lineRight = card.right - 10f*u;
        float lineY = card.bottom - 17f*u;
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(1.2f*u);
        paint.setColor(Color.argb(75,244,237,224));
        canvas.drawLine(lineLeft, lineY, lineRight, lineY, paint);
        float center = (lineLeft + lineRight) * 0.5f;
        canvas.drawLine(center, lineY - 4f*u, center, lineY + 4f*u, paint);

        float px = lineLeft + (lineRight - lineLeft) * (pan / 127f);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(225,244,237,224));
        canvas.drawCircle(px, lineY, 3.6f*u, paint);

        text.setTextAlign(Paint.Align.LEFT);
        text.setTextSize(8.5f*u);
        text.setColor(Color.argb(130,244,237,224));
        canvas.drawText(panText(pan), card.left + 8f*u, card.bottom - 31f*u, text);

        if (muted) {
            text.setTextAlign(Paint.Align.RIGHT);
            text.setTextSize(8.5f*u);
            text.setColor(Color.rgb(244,237,224));
            canvas.drawText("MUTED", card.right - 8f*u, card.bottom - 31f*u, text);
        }
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_UP) {
            performClick();
            if (listener != null) listener.onSelect();
            return true;
        }
        return true;
    }

    @Override public boolean performClick() {
        super.performClick();
        return true;
    }

    private static int clamp7(int value) {
        return Math.max(0, Math.min(127, value));
    }

    private static String panText(int value) {
        if (value >= 62 && value <= 66) return "PAN C";
        if (value < 64) {
            int amount = Math.round((64 - value) * 100f / 64f);
            return "L" + amount;
        }
        int amount = Math.round((value - 64) * 100f / 63f);
        return "R" + amount;
    }
}
