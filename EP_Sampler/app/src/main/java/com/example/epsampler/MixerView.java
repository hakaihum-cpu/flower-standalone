package com.example.epsampler;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

final class MixerView extends FrameLayout {
    interface Listener {
        void onMixerClose();
        void onMixerChanged(int part, int volume, int pan, boolean muted);
    }

    private static final int PARTS = 16;
    private static final String[] NAMES = {
            "VIOLIN", "FLUTE", "SAX", "FELT", "ACCORD",
            "XYLO", "BASS", "DRUMS",
            "SAMPLE 1", "SAMPLE 2", "SAMPLE 3", "SAMPLE 4",
            "SAMPLE 5", "SAMPLE 6", "SAMPLE 7", "SAMPLE 8"
    };

    private final TextView[] nameLabels = new TextView[PARTS];
    private final SeekBar[] volumeBars = new SeekBar[PARTS];
    private final SeekBar[] panBars = new SeekBar[PARTS];
    private final TextView[] volumeLabels = new TextView[PARTS];
    private final TextView[] panLabels = new TextView[PARTS];
    private final Button[] muteButtons = new Button[PARTS];
    private final int[] volumes = {
            112,112,112,112,112,112,112,112,
            127,127,127,127,127,127,127,127
    };
    private final int[] pans = {
            64,64,64,64,64,64,64,64,
            64,64,64,64,64,64,64,64
    };
    private final boolean[] mutes = new boolean[PARTS];

    private Listener listener;
    private boolean updating = false;

    MixerView(Context context) {
        super(context);
        setVisibility(GONE);
        setClickable(true);
        setFocusable(true);
        setBackgroundColor(Color.rgb(8, 8, 8));

        LinearLayout page = new LinearLayout(context);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(12), dp(10), dp(12), dp(12));
        addView(page, new FrameLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        LinearLayout header = new LinearLayout(context);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        page.addView(header, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, dp(54)));

        TextView title = text("MIXER", 22f);
        header.addView(title, new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f));

        Button close = new Button(context);
        close.setText("CLOSE");
        close.setTextSize(13f);
        close.setTextColor(Color.rgb(244, 237, 224));
        close.setBackground(panelDrawable(false));
        close.setMinWidth(dp(96));
        close.setMinHeight(dp(44));
        close.setOnClickListener(v -> {
            if (listener != null) listener.onMixerClose();
        });
        header.addView(close, new LinearLayout.LayoutParams(dp(104), dp(44)));

        TextView help = text("VOLUME / PAN / MUTE    •    swipe sideways", 12f);
        help.setTextColor(Color.argb(185, 244, 237, 224));
        LinearLayout.LayoutParams helpLp = new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        helpLp.setMargins(0, 0, 0, dp(8));
        page.addView(help, helpLp);

        HorizontalScrollView scroll = new HorizontalScrollView(context);
        scroll.setFillViewport(false);
        scroll.setHorizontalScrollBarEnabled(true);
        scroll.setOverScrollMode(View.OVER_SCROLL_IF_CONTENT_SCROLLS);
        page.addView(scroll, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, 0, 1f));

        LinearLayout row = new LinearLayout(context);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setPadding(0, 0, dp(16), 0);
        scroll.addView(row, new FrameLayout.LayoutParams(
                LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));

        for (int part = 0; part < PARTS; part++) {
            final int p = part;
            LinearLayout strip = new LinearLayout(context);
            strip.setOrientation(LinearLayout.VERTICAL);
            strip.setGravity(Gravity.CENTER_HORIZONTAL);
            strip.setPadding(dp(12), dp(12), dp(12), dp(12));
            strip.setBackground(panelDrawable(false));

            LinearLayout.LayoutParams stripLp = new LinearLayout.LayoutParams(
                    dp(174), LayoutParams.MATCH_PARENT);
            stripLp.setMargins(0, 0, dp(9), 0);
            row.addView(strip, stripLp);

            TextView name = text(NAMES[part], 16f);
            name.setGravity(Gravity.CENTER);
            nameLabels[part] = name;
            strip.addView(name, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(42)));

            volumeLabels[part] = text("VOL " + volumes[part], 13f);
            volumeLabels[part].setGravity(Gravity.CENTER);
            strip.addView(volumeLabels[part], new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(34)));

            SeekBar volume = new SeekBar(context);
            volume.setMax(127);
            volume.setProgress(volumes[part]);
            volume.setContentDescription(NAMES[part] + " volume");
            volume.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    volumes[p] = clamp7(progress);
                    volumeLabels[p].setText("VOL " + volumes[p]);
                    if (!updating && fromUser) notifyChange(p);
                }
                @Override public void onStartTrackingTouch(SeekBar seekBar) { }
                @Override public void onStopTrackingTouch(SeekBar seekBar) { }
            });
            volumeBars[part] = volume;
            strip.addView(volume, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(54)));

            panLabels[part] = text(panText(pans[part]), 13f);
            panLabels[part].setGravity(Gravity.CENTER);
            LinearLayout.LayoutParams panLabelLp = new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(34));
            panLabelLp.setMargins(0, dp(16), 0, 0);
            strip.addView(panLabels[part], panLabelLp);

            SeekBar pan = new SeekBar(context);
            pan.setMax(127);
            pan.setProgress(pans[part]);
            pan.setContentDescription(NAMES[part] + " pan");
            pan.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    pans[p] = clamp7(progress);
                    panLabels[p].setText(panText(pans[p]));
                    if (!updating && fromUser) notifyChange(p);
                }
                @Override public void onStartTrackingTouch(SeekBar seekBar) { }
                @Override public void onStopTrackingTouch(SeekBar seekBar) { }
            });
            panBars[part] = pan;
            strip.addView(pan, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(54)));

            Button mute = new Button(context);
            mute.setText("MUTE");
            mute.setTextSize(13f);
            mute.setTextColor(Color.rgb(244, 237, 224));
            mute.setMinHeight(dp(48));
            mute.setContentDescription(NAMES[part] + " mute");
            mute.setOnClickListener(v -> {
                mutes[p] = !mutes[p];
                updateMuteButton(p);
                if (!updating) notifyChange(p);
            });
            muteButtons[part] = mute;
            LinearLayout.LayoutParams muteLp = new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, dp(52));
            muteLp.setMargins(0, dp(22), 0, 0);
            strip.addView(mute, muteLp);
            updateMuteButton(part);
        }
    }

    void setListener(Listener listener) {
        this.listener = listener;
    }

    void setPartNames(String[] names) {
        if (names == null) return;
        for (int i=0; i<PARTS && i<names.length; i++) {
            String value = names[i] == null || names[i].trim().isEmpty()
                    ? NAMES[i] : names[i].trim();
            if (nameLabels[i] != null) nameLabels[i].setText(value);
            if (volumeBars[i] != null) volumeBars[i].setContentDescription(value + " volume");
            if (panBars[i] != null) panBars[i].setContentDescription(value + " pan");
            if (muteButtons[i] != null) muteButtons[i].setContentDescription(value + " mute");
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

            volumeBars[i].setProgress(volumes[i]);
            panBars[i].setProgress(pans[i]);
            volumeLabels[i].setText("VOL " + volumes[i]);
            panLabels[i].setText(panText(pans[i]));
            updateMuteButton(i);
        }
        updating = false;
    }

    void setPartVolume(int part, int volume) {
        if (part < 0 || part >= PARTS) return;
        updating = true;
        volumes[part] = clamp7(volume);
        volumeBars[part].setProgress(volumes[part]);
        volumeLabels[part].setText("VOL " + volumes[part]);
        updating = false;
    }

    private void notifyChange(int part) {
        if (listener != null)
            listener.onMixerChanged(part, volumes[part], pans[part], mutes[part]);
    }

    private void updateMuteButton(int part) {
        Button b = muteButtons[part];
        if (b == null) return;
        b.setBackground(panelDrawable(mutes[part]));
        b.setText(mutes[part] ? "MUTED" : "MUTE");
        b.setTextColor(mutes[part] ? Color.rgb(24, 24, 22) : Color.rgb(244, 237, 224));
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

    private TextView text(String value, float sizeSp) {
        TextView t = new TextView(getContext());
        t.setText(value);
        t.setTextSize(sizeSp);
        t.setTextColor(Color.rgb(244, 237, 224));
        t.setGravity(Gravity.CENTER_VERTICAL);
        return t;
    }

    private GradientDrawable panelDrawable(boolean active) {
        GradientDrawable d = new GradientDrawable();
        d.setCornerRadius(dp(7));
        d.setColor(active
                ? Color.rgb(238, 229, 207)
                : Color.argb(52, 244, 237, 224));
        d.setStroke(Math.max(1, dp(1)), Color.argb(110, 244, 237, 224));
        return d;
    }

    private int dp(float value) {
        return Math.max(1, Math.round(value * getResources().getDisplayMetrics().density));
    }

    private static int clamp7(int value) {
        return Math.max(0, Math.min(127, value));
    }
}
