package com.example.epsampler;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Sixteen-part mixer using the same visual/control language as the other
 * graphical CONFIG editors.
 *
 * Audio routing and mixer semantics are unchanged: this view still exposes only
 * Volume / Pan / Mute through the existing Listener contract.
 */
final class MixerView extends FrameLayout {
    interface Listener {
        void onMixerClose();
        void onMixerChanged(int part, int volume, int pan, boolean muted);
    }

    private static final int PARTS = 16;
    private static final int FG_R = 244;
    private static final int FG_G = 237;
    private static final int FG_B = 224;

    private static final String[] NAMES = {
            "VIOLIN", "FLUTE", "SAX", "FELT", "ACCORD",
            "XYLO", "BASS", "DRUMS",
            "SAMPLE 1", "SAMPLE 2", "SAMPLE 3", "SAMPLE 4",
            "SAMPLE 5", "SAMPLE 6", "SAMPLE 7", "SAMPLE 8"
    };

    private final TextView[] nameLabels = new TextView[PARTS];
    private final GraphicParameterControl[] volumeControls =
            new GraphicParameterControl[PARTS];
    private final GraphicParameterControl[] panControls =
            new GraphicParameterControl[PARTS];
    private final TextView[] muteControls = new TextView[PARTS];

    private final int[] volumes = {
            112,112,112,112,112,112,112,112,
            127,127,127,127,127,127,127,127
    };
    private final int[] pans = {
            64,64,64,64,64,64,64,64,
            64,64,64,64,64,64,64,64
    };
    private final boolean[] mutes = new boolean[PARTS];
    private final String[] partNames = NAMES.clone();

    private Listener listener;
    private boolean updating = false;

    MixerView(Context context) {
        super(context);
        setVisibility(GONE);
        setClickable(true);
        setFocusable(true);
        setBackgroundColor(Color.rgb(7, 7, 7));

        LinearLayout page = new LinearLayout(context);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(14), dp(12), dp(14), dp(14));
        addView(page, new FrameLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        // Header follows SoundDesignView: title on the left, compact panel CLOSE.
        LinearLayout header = new LinearLayout(context);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        page.addView(header, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, dp(48)));

        TextView title = text("MIXER", 21f);
        header.addView(title, new LinearLayout.LayoutParams(
                0, LayoutParams.MATCH_PARENT, 1f));

        TextView close = text("CLOSE", 11.5f);
        close.setGravity(Gravity.CENTER);
        close.setBackground(panelDrawable(false));
        close.setClickable(true);
        close.setFocusable(true);
        close.setContentDescription("Close mixer");
        close.setOnClickListener(v -> {
            if (listener != null) listener.onMixerClose();
        });
        LinearLayout.LayoutParams closeLp =
                new LinearLayout.LayoutParams(dp(88), dp(36));
        closeLp.setMargins(dp(8), 0, 0, 0);
        header.addView(close, closeLp);

        TextView help = text(
                "VOLUME / PAN / MUTE   ·   DRAG GRAPHICS ↑↓   ·   SWIPE SIDEWAYS",
                10.5f);
        help.setTextColor(Color.argb(160, FG_R, FG_G, FG_B));
        LinearLayout.LayoutParams helpLp = new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, dp(34));
        helpLp.setMargins(0, 0, 0, dp(4));
        page.addView(help, helpLp);

        HorizontalScrollView scroll = new HorizontalScrollView(context);
        scroll.setFillViewport(false);
        scroll.setHorizontalScrollBarEnabled(false);
        scroll.setOverScrollMode(View.OVER_SCROLL_IF_CONTENT_SCROLLS);
        page.addView(scroll, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, 0, 1f));

        LinearLayout row = new LinearLayout(context);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.TOP);
        row.setPadding(0, 0, dp(16), 0);
        scroll.addView(row, new FrameLayout.LayoutParams(
                LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));

        for (int part = 0; part < PARTS; part++) {
            final int p = part;

            LinearLayout strip = new LinearLayout(context);
            strip.setOrientation(LinearLayout.VERTICAL);
            strip.setGravity(Gravity.CENTER_HORIZONTAL);
            strip.setPadding(dp(8), dp(8), dp(8), dp(8));
            strip.setBackground(panelDrawable(false));

            LinearLayout.LayoutParams stripLp = new LinearLayout.LayoutParams(
                    dp(172), LayoutParams.MATCH_PARENT);
            stripLp.setMargins(0, 0, dp(8), 0);
            row.addView(strip, stripLp);

            TextView name = text(NAMES[part], 14f);
            name.setGravity(Gravity.CENTER);
            name.setSingleLine(true);
            nameLabels[part] = name;
            strip.addView(name, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(36)));

            TextView partNumber = text(twoDigit(part + 1), 9.5f);
            partNumber.setGravity(Gravity.CENTER);
            partNumber.setTextColor(Color.argb(105, FG_R, FG_G, FG_B));
            strip.addView(partNumber, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(20)));

            GraphicParameterControl volume = new GraphicParameterControl(context);
            volume.configure(
                    "VOLUME",
                    127,
                    volumes[part],
                    GraphicParameterControl.STYLE_FIELD,
                    value -> {
                        volumes[p] = clamp7(value);
                        if (!updating) notifyChange(p);
                    });
            volume.setContentDescription(NAMES[part] + " volume");
            volumeControls[part] = volume;
            LinearLayout.LayoutParams volumeLp = new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(116));
            volumeLp.setMargins(0, dp(3), 0, dp(5));
            strip.addView(volume, volumeLp);

            GraphicParameterControl pan = new GraphicParameterControl(context);
            pan.configure(
                    panText(pans[part]),
                    127,
                    pans[part],
                    GraphicParameterControl.STYLE_FIELD,
                    value -> {
                        pans[p] = clamp7(value);
                        panControls[p].setLabel(panText(pans[p]));
                        if (!updating) notifyChange(p);
                    });
            pan.setContentDescription(NAMES[part] + " pan");
            panControls[part] = pan;
            LinearLayout.LayoutParams panLp = new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(116));
            panLp.setMargins(0, 0, 0, dp(8));
            strip.addView(pan, panLp);

            TextView mute = text("MUTE", 12f);
            mute.setGravity(Gravity.CENTER);
            mute.setClickable(true);
            mute.setFocusable(true);
            mute.setContentDescription(NAMES[part] + " mute");
            mute.setOnClickListener(v -> {
                mutes[p] = !mutes[p];
                updateMuteControl(p);
                if (!updating) notifyChange(p);
            });
            muteControls[part] = mute;
            strip.addView(mute, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(42)));
            updateMuteControl(part);
        }
    }

    void setListener(Listener listener) {
        this.listener = listener;
    }

    void setPartNames(String[] names) {
        if (names == null) return;
        for (int i = 0; i < PARTS && i < names.length; i++) {
            String value = names[i] == null || names[i].trim().isEmpty()
                    ? NAMES[i] : names[i].trim();
            partNames[i] = value;

            if (nameLabels[i] != null) nameLabels[i].setText(value);
            if (volumeControls[i] != null)
                volumeControls[i].setContentDescription(value + " volume");
            if (panControls[i] != null)
                panControls[i].setContentDescription(value + " pan");
            if (muteControls[i] != null)
                muteControls[i].setContentDescription(value + " mute");
        }
    }

    void setMixerState(int[] sourceVolumes, int[] sourcePans, boolean[] sourceMutes) {
        updating = true;
        for (int i = 0; i < PARTS; i++) {
            if (sourceVolumes != null && i < sourceVolumes.length)
                volumes[i] = clamp7(sourceVolumes[i]);
            if (sourcePans != null && i < sourcePans.length)
                pans[i] = clamp7(sourcePans[i]);
            if (sourceMutes != null && i < sourceMutes.length)
                mutes[i] = sourceMutes[i];

            if (volumeControls[i] != null)
                volumeControls[i].setValue(volumes[i]);
            if (panControls[i] != null) {
                panControls[i].setValue(pans[i]);
                panControls[i].setLabel(panText(pans[i]));
            }
            updateMuteControl(i);
        }
        updating = false;
    }

    void setPartVolume(int part, int volume) {
        if (part < 0 || part >= PARTS) return;
        updating = true;
        volumes[part] = clamp7(volume);
        if (volumeControls[part] != null)
            volumeControls[part].setValue(volumes[part]);
        updating = false;
    }

    private void notifyChange(int part) {
        if (listener != null)
            listener.onMixerChanged(part, volumes[part], pans[part], mutes[part]);
    }

    private void updateMuteControl(int part) {
        TextView view = muteControls[part];
        if (view == null) return;

        view.setText(mutes[part] ? "MUTED" : "MUTE");
        view.setBackground(panelDrawable(mutes[part]));
        view.setTextColor(mutes[part]
                ? Color.rgb(20, 20, 19)
                : Color.rgb(FG_R, FG_G, FG_B));
    }

    private String panText(int value) {
        if (value >= 62 && value <= 66) return "PAN C";
        if (value < 64) {
            int amount = Math.round((64 - value) * 100f / 64f);
            return "PAN L" + amount;
        }
        int amount = Math.round((value - 64) * 100f / 63f);
        return "PAN R" + amount;
    }

    private static String twoDigit(int value) {
        return value < 10 ? "0" + value : Integer.toString(value);
    }

    private TextView text(String value, float sizeSp) {
        TextView t = new TextView(getContext());
        t.setText(value);
        t.setTextSize(sizeSp);
        t.setTextColor(Color.rgb(FG_R, FG_G, FG_B));
        t.setGravity(Gravity.CENTER_VERTICAL);
        t.setTypeface(android.graphics.Typeface.create(
                "sans", android.graphics.Typeface.NORMAL));
        return t;
    }

    // Same panel palette and active inversion used by the graphical CONFIG UI.
    private GradientDrawable panelDrawable(boolean active) {
        GradientDrawable d = new GradientDrawable();
        d.setCornerRadius(dp(7));
        d.setColor(active
                ? Color.rgb(238, 229, 207)
                : Color.argb(48, FG_R, FG_G, FG_B));
        d.setStroke(Math.max(1, dp(1)),
                Color.argb(active ? 185 : 92, FG_R, FG_G, FG_B));
        return d;
    }

    private int dp(float value) {
        return Math.max(1, Math.round(
                value * getResources().getDisplayMetrics().density));
    }

    private static int clamp7(int value) {
        return Math.max(0, Math.min(127, value));
    }
}
