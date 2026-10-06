package com.example.epsampler;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Sixteen-part 4x4 mixer overview with one shared detail editor.
 *
 * The visual language is intentionally the same as SoundDesignView and the
 * other graphical CONFIG pages: black field, warm white graphics, translucent
 * cards and direct graphical controls.
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

    private final MixerChannelTile[] tiles = new MixerChannelTile[PARTS];
    private final String[] partNames = NAMES.clone();

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
    private int selectedPart = 0;

    private TextView selectedName;
    private TextView selectedNumber;
    private GraphicParameterControl volumeEditor;
    private GraphicParameterControl panEditor;
    private TextView muteEditor;

    private final Runnable meterTick = new Runnable() {
        @Override public void run() {
            if (getVisibility() != View.VISIBLE) return;
            for (int part=0; part<PARTS; part++) {
                if (tiles[part] != null) {
                    tiles[part].setMeter(NativeEngine.partMeter(part));
                }
            }
            postDelayed(this, 50L);
        }
    };

    MixerView(Context context) {
        super(context);
        setVisibility(GONE);
        setClickable(true);
        setFocusable(true);
        setBackgroundColor(Color.rgb(7, 7, 7));

        LinearLayout page = new LinearLayout(context);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(12), dp(10), dp(12), dp(12));
        addView(page, new FrameLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        LinearLayout header = new LinearLayout(context);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        page.addView(header, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, dp(46)));

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
        header.addView(close, new LinearLayout.LayoutParams(dp(88), dp(36)));

        TextView help = text(
                "16 PARTS  ·  TAP A CELL TO EDIT  ·  REAL POST-FADER VU",
                9.5f);
        help.setTextColor(Color.argb(145, FG_R, FG_G, FG_B));
        page.addView(help, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, dp(26)));

        // 4x4 overview: all sixteen parts are visible at once.
        LinearLayout grid = new LinearLayout(context);
        grid.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams gridLp = new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, 0, 2.15f);
        gridLp.setMargins(0, 0, 0, dp(7));
        page.addView(grid, gridLp);

        for (int row=0; row<4; row++) {
            LinearLayout rowLayout = new LinearLayout(context);
            rowLayout.setOrientation(LinearLayout.HORIZONTAL);
            grid.addView(rowLayout, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, 0, 1f));

            for (int col=0; col<4; col++) {
                final int part = row*4 + col;
                MixerChannelTile tile = new MixerChannelTile(context);
                tile.setListener(() -> selectPart(part));
                tiles[part] = tile;

                LinearLayout.LayoutParams tileLp = new LinearLayout.LayoutParams(
                        0, LayoutParams.MATCH_PARENT, 1f);
                tileLp.setMargins(dp(2), dp(2), dp(2), dp(2));
                rowLayout.addView(tile, tileLp);
            }
        }

        // Selected-part detail editor. This is the OP-1-like focus layer:
        // one part becomes large and direct after choosing it in the overview.
        LinearLayout editor = new LinearLayout(context);
        editor.setOrientation(LinearLayout.VERTICAL);
        editor.setPadding(dp(9), dp(7), dp(9), dp(8));
        editor.setBackground(panelDrawable(false));
        page.addView(editor, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, 0, 1.0f));

        LinearLayout editorHeader = new LinearLayout(context);
        editorHeader.setOrientation(LinearLayout.HORIZONTAL);
        editorHeader.setGravity(Gravity.CENTER_VERTICAL);
        editor.addView(editorHeader, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, dp(30)));

        selectedName = text(partNames[0], 14f);
        selectedName.setSingleLine(true);
        editorHeader.addView(selectedName, new LinearLayout.LayoutParams(
                0, LayoutParams.MATCH_PARENT, 1f));

        selectedNumber = text("PART 01", 9.5f);
        selectedNumber.setGravity(Gravity.CENTER_VERTICAL | Gravity.RIGHT);
        selectedNumber.setTextColor(Color.argb(135, FG_R, FG_G, FG_B));
        editorHeader.addView(selectedNumber, new LinearLayout.LayoutParams(
                dp(74), LayoutParams.MATCH_PARENT));

        LinearLayout controls = new LinearLayout(context);
        controls.setOrientation(LinearLayout.HORIZONTAL);
        controls.setGravity(Gravity.CENTER_VERTICAL);
        editor.addView(controls, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, 0, 1f));

        volumeEditor = new GraphicParameterControl(context);
        volumeEditor.configure(
                "VOLUME",
                127,
                volumes[0],
                GraphicParameterControl.STYLE_FIELD,
                value -> {
                    if (updating) return;
                    volumes[selectedPart] = clamp7(value);
                    refreshTile(selectedPart);
                    notifyChange(selectedPart);
                });
        LinearLayout.LayoutParams volLp = new LinearLayout.LayoutParams(
                0, LayoutParams.MATCH_PARENT, 1f);
        volLp.setMargins(0, 0, dp(5), 0);
        controls.addView(volumeEditor, volLp);

        panEditor = new GraphicParameterControl(context);
        panEditor.configure(
                panText(pans[0]),
                127,
                pans[0],
                GraphicParameterControl.STYLE_FIELD,
                value -> {
                    if (updating) return;
                    pans[selectedPart] = clamp7(value);
                    panEditor.setLabel(panText(pans[selectedPart]));
                    refreshTile(selectedPart);
                    notifyChange(selectedPart);
                });
        LinearLayout.LayoutParams panLp = new LinearLayout.LayoutParams(
                0, LayoutParams.MATCH_PARENT, 1f);
        panLp.setMargins(0, 0, dp(5), 0);
        controls.addView(panEditor, panLp);

        muteEditor = text("MUTE", 11.5f);
        muteEditor.setGravity(Gravity.CENTER);
        muteEditor.setClickable(true);
        muteEditor.setFocusable(true);
        muteEditor.setOnClickListener(v -> {
            if (updating) return;
            mutes[selectedPart] = !mutes[selectedPart];
            updateMuteEditor();
            refreshTile(selectedPart);
            notifyChange(selectedPart);
        });
        controls.addView(muteEditor, new LinearLayout.LayoutParams(
                dp(88), LayoutParams.MATCH_PARENT));

        for (int part=0; part<PARTS; part++) refreshTile(part);
        selectPart(0);
    }

    void setListener(Listener listener) {
        this.listener = listener;
    }

    void setPartNames(String[] names) {
        if (names == null) return;
        for (int i=0; i<PARTS && i<names.length; i++) {
            String value = names[i] == null || names[i].trim().isEmpty()
                    ? NAMES[i] : names[i].trim();
            partNames[i] = value;
            refreshTile(i);
        }
        refreshEditor();
    }

    void setMixerState(int[] sourceVolumes, int[] sourcePans, boolean[] sourceMutes) {
        updating = true;
        for (int i=0; i<PARTS; i++) {
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

    void setPartVolume(int part, int volume) {
        if (part < 0 || part >= PARTS) return;
        updating = true;
        volumes[part] = clamp7(volume);
        refreshTile(part);
        if (part == selectedPart) refreshEditor();
        updating = false;
    }

    private void selectPart(int part) {
        part = Math.max(0, Math.min(PARTS - 1, part));
        int old = selectedPart;
        selectedPart = part;
        if (tiles[old] != null) tiles[old].setSelected(false);
        if (tiles[selectedPart] != null) tiles[selectedPart].setSelected(true);
        refreshEditor();
    }

    private void refreshTile(int part) {
        if (part < 0 || part >= PARTS || tiles[part] == null) return;
        tiles[part].setState(
                partNames[part],
                volumes[part],
                pans[part],
                mutes[part],
                part == selectedPart);
    }

    private void refreshEditor() {
        if (selectedName == null) return;
        updating = true;

        selectedName.setText(partNames[selectedPart]);
        selectedNumber.setText("PART " + twoDigit(selectedPart + 1));

        volumeEditor.setValue(volumes[selectedPart]);
        volumeEditor.setContentDescription(
                partNames[selectedPart] + " volume");

        panEditor.setValue(pans[selectedPart]);
        panEditor.setLabel(panText(pans[selectedPart]));
        panEditor.setContentDescription(
                partNames[selectedPart] + " pan");

        muteEditor.setContentDescription(
                partNames[selectedPart] + " mute");
        updateMuteEditor();

        updating = false;
    }

    private void updateMuteEditor() {
        if (muteEditor == null) return;
        boolean active = mutes[selectedPart];
        muteEditor.setText(active ? "MUTED" : "MUTE");
        muteEditor.setBackground(panelDrawable(active));
        muteEditor.setTextColor(active
                ? Color.rgb(20,20,19)
                : Color.rgb(FG_R,FG_G,FG_B));
    }

    private void notifyChange(int part) {
        if (listener != null) {
            listener.onMixerChanged(
                    part, volumes[part], pans[part], mutes[part]);
        }
    }

    @Override protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        if (changedView != this) return;
        removeCallbacks(meterTick);
        if (visibility == View.VISIBLE) post(meterTick);
    }

    @Override protected void onDetachedFromWindow() {
        removeCallbacks(meterTick);
        super.onDetachedFromWindow();
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
