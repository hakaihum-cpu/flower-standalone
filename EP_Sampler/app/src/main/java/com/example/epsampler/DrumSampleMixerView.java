package com.example.epsampler;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/** Six-channel submixer for the bundled DRUMS one-shot WAV files. */
final class DrumSampleMixerView extends FrameLayout {
    interface Listener {
        void onDrumSampleMixerClose();
        void onDrumSampleMixerChanged(int slot, int volume, int pan, boolean muted);
    }

    static final int CHANNELS = 6;
    static final String[] FILE_NAMES = {
            "drum_closehat.wav",
            "drum_tom.wav",
            "drum_crash.wav",
            "drum_kick.wav",
            "drum_stick.wav",
            "drum_snaire.wav"
    };

    private static final int FG_R = 244, FG_G = 237, FG_B = 224;

    private final TextView[] channelTiles = new TextView[CHANNELS];
    private final int[] volumes = {127,127,127,127,127,127};
    private final int[] pans = {64,64,64,64,64,64};
    private final boolean[] mutes = new boolean[CHANNELS];

    private Listener listener;
    private int selectedSlot = 0;
    private boolean updating = false;
    private TextView selectedName;
    private GraphicParameterControl volumeControl;
    private GraphicParameterControl panControl;
    private TextView muteControl;

    DrumSampleMixerView(Context context) {
        super(context);
        setVisibility(GONE);
        setClickable(true);
        setFocusable(true);
        setBackgroundColor(Color.rgb(7,7,7));

        LinearLayout page = new LinearLayout(context);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(12), dp(10), dp(12), dp(12));
        addView(page, new FrameLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        LinearLayout header = new LinearLayout(context);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        page.addView(header, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, dp(48)));

        TextView title = text("DRUM SAMPLE MIXER", 19f);
        header.addView(title, new LinearLayout.LayoutParams(
                0, LayoutParams.MATCH_PARENT, 1f));

        TextView close = text("CLOSE", 11.5f);
        close.setGravity(Gravity.CENTER);
        close.setBackground(panel(false));
        close.setClickable(true);
        close.setOnClickListener(v -> {
            if (listener != null) listener.onDrumSampleMixerClose();
        });
        header.addView(close, new LinearLayout.LayoutParams(dp(88), dp(36)));

        TextView help = text("6 ONE-SHOT FILES  ·  DRUMS BUS MASTER REMAINS IN MAIN MIXER", 9.5f);
        help.setTextColor(Color.argb(145, FG_R, FG_G, FG_B));
        page.addView(help, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, dp(28)));

        LinearLayout grid = new LinearLayout(context);
        grid.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams gridLp = new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, 0, 1.9f);
        gridLp.setMargins(0, 0, 0, dp(8));
        page.addView(grid, gridLp);

        for (int row=0; row<3; row++) {
            LinearLayout rowView = new LinearLayout(context);
            rowView.setOrientation(LinearLayout.HORIZONTAL);
            grid.addView(rowView, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, 0, 1f));

            for (int col=0; col<2; col++) {
                final int slot = row*2 + col;
                TextView tile = text("", 11.5f);
                tile.setGravity(Gravity.CENTER);
                tile.setClickable(true);
                tile.setFocusable(true);
                tile.setPadding(dp(6), dp(4), dp(6), dp(4));
                tile.setOnClickListener(v -> selectSlot(slot));
                channelTiles[slot] = tile;

                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                        0, LayoutParams.MATCH_PARENT, 1f);
                lp.setMargins(dp(2), dp(2), dp(2), dp(2));
                rowView.addView(tile, lp);
            }
        }

        LinearLayout editor = new LinearLayout(context);
        editor.setOrientation(LinearLayout.VERTICAL);
        editor.setPadding(dp(9), dp(7), dp(9), dp(8));
        editor.setBackground(panel(false));
        page.addView(editor, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, 0, 1f));

        selectedName = text(FILE_NAMES[0], 13.5f);
        selectedName.setGravity(Gravity.CENTER_VERTICAL);
        editor.addView(selectedName, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, dp(30)));

        LinearLayout controls = new LinearLayout(context);
        controls.setOrientation(LinearLayout.HORIZONTAL);
        editor.addView(controls, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, 0, 1f));

        volumeControl = new GraphicParameterControl(context);
        volumeControl.configure("VOLUME", 127, 127, GraphicParameterControl.STYLE_FIELD,
                value -> {
                    if (updating) return;
                    volumes[selectedSlot] = clamp7(value);
                    refreshTile(selectedSlot);
                    notifyChange(selectedSlot);
                });
        LinearLayout.LayoutParams volLp = new LinearLayout.LayoutParams(
                0, LayoutParams.MATCH_PARENT, 1f);
        volLp.setMargins(0, 0, dp(5), 0);
        controls.addView(volumeControl, volLp);

        panControl = new GraphicParameterControl(context);
        panControl.configure("PAN C", 127, 64, GraphicParameterControl.STYLE_FIELD,
                value -> {
                    if (updating) return;
                    pans[selectedSlot] = clamp7(value);
                    panControl.setLabel(panText(pans[selectedSlot]));
                    refreshTile(selectedSlot);
                    notifyChange(selectedSlot);
                });
        LinearLayout.LayoutParams panLp = new LinearLayout.LayoutParams(
                0, LayoutParams.MATCH_PARENT, 1f);
        panLp.setMargins(0, 0, dp(5), 0);
        controls.addView(panControl, panLp);

        muteControl = text("MUTE", 11.5f);
        muteControl.setGravity(Gravity.CENTER);
        muteControl.setClickable(true);
        muteControl.setOnClickListener(v -> {
            if (updating) return;
            mutes[selectedSlot] = !mutes[selectedSlot];
            refreshEditor();
            refreshTile(selectedSlot);
            notifyChange(selectedSlot);
        });
        controls.addView(muteControl, new LinearLayout.LayoutParams(
                dp(88), LayoutParams.MATCH_PARENT));

        for (int i=0; i<CHANNELS; i++) refreshTile(i);
        selectSlot(0);
    }

    void setListener(Listener listener) { this.listener = listener; }

    void setMixerState(int[] sourceVolumes, int[] sourcePans, boolean[] sourceMutes) {
        updating = true;
        for (int i=0; i<CHANNELS; i++) {
            if (sourceVolumes != null && i < sourceVolumes.length)
                volumes[i] = clamp7(sourceVolumes[i]);
            if (sourcePans != null && i < sourcePans.length)
                pans[i] = clamp7(sourcePans[i]);
            if (sourceMutes != null && i < sourceMutes.length)
                mutes[i] = sourceMutes[i];
            refreshTile(i);
        }
        refreshEditor();
        updating = false;
    }

    private void selectSlot(int slot) {
        selectedSlot = Math.max(0, Math.min(CHANNELS - 1, slot));
        for (int i=0; i<CHANNELS; i++) refreshTile(i);
        refreshEditor();
    }

    private void refreshTile(int slot) {
        TextView tile = channelTiles[slot];
        if (tile == null) return;
        String state = FILE_NAMES[slot] +
                "\nVOL " + volumes[slot] + "   " + panText(pans[slot]) +
                (mutes[slot] ? "   MUTED" : "");
        tile.setText(state);
        tile.setBackground(panel(slot == selectedSlot));
        tile.setTextColor(slot == selectedSlot
                ? Color.rgb(20,20,19)
                : Color.rgb(FG_R,FG_G,FG_B));
    }

    private void refreshEditor() {
        if (selectedName == null) return;
        updating = true;
        selectedName.setText(FILE_NAMES[selectedSlot]);
        volumeControl.setValue(volumes[selectedSlot]);
        panControl.setValue(pans[selectedSlot]);
        panControl.setLabel(panText(pans[selectedSlot]));
        boolean muted = mutes[selectedSlot];
        muteControl.setText(muted ? "MUTED" : "MUTE");
        muteControl.setBackground(panel(muted));
        muteControl.setTextColor(muted
                ? Color.rgb(20,20,19)
                : Color.rgb(FG_R,FG_G,FG_B));
        updating = false;
    }

    private void notifyChange(int slot) {
        if (listener != null) {
            listener.onDrumSampleMixerChanged(
                    slot, volumes[slot], pans[slot], mutes[slot]);
        }
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
        t.setTextColor(Color.rgb(FG_R,FG_G,FG_B));
        t.setTypeface(android.graphics.Typeface.create(
                "sans", android.graphics.Typeface.NORMAL));
        return t;
    }

    private GradientDrawable panel(boolean active) {
        GradientDrawable d = new GradientDrawable();
        d.setCornerRadius(dp(7));
        d.setColor(active
                ? Color.rgb(238,229,207)
                : Color.argb(48,FG_R,FG_G,FG_B));
        d.setStroke(Math.max(1,dp(1)),
                Color.argb(active ? 185 : 92,FG_R,FG_G,FG_B));
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
