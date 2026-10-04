package com.example.epsampler;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.Settings;
import android.view.View;
import android.view.KeyEvent;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;


public class MainActivity extends Activity implements MidiController.Listener, PianoView.ActionListener {
    private static final int PICK_BANK = 1001;
    private static final String PREFS = "ep_sampler";
    private static final String KEY_BANK_URI = "bank_uri";
    private static final String KEY_BOOST = "boost_step";
    private static final String KEY_SPACE = "space_mode";
    private static final String KEY_TAPE = "tape_on";
    private static final String KEY_DREAMY = "dreamy_on";
    private static final String KEY_BOOST_DB = "boost_db";
    private static final String KEY_SPACE_MIX = "space_mix";
    private static final String KEY_SPACE_DECAY = "space_decay";
    private static final String KEY_TAPE_WOW = "tape_wow";
    private static final String KEY_TAPE_FLUTTER = "tape_flutter";
    private static final String KEY_TAPE_DRIVE = "tape_drive";
    private static final String KEY_DREAM_X = "dream_x";
    private static final String KEY_DREAM_Y = "dream_y";
    private static final String KEY_DREAM_MIX = "dream_mix";
    private static final String KEY_MIDI_CHANNEL = "midi_channel";
    private static final String KEY_MANUAL_SUSTAIN = "manual_sustain";
    private static final String KEY_AUDIO_BUFFER_BURSTS = "audio_buffer_bursts";
    private static final String[] AUDIO_BUFFER_LABELS = {
            "AUTO", "0.5 BURST", "0.75 BURST", "1 BURST", "1.5 BURSTS", "2 BURSTS",
            "3 BURSTS", "4 BURSTS", "5 BURSTS", "6 BURSTS", "7 BURSTS", "8 BURSTS"
    };
    private static final float[] AUDIO_BUFFER_VALUES = {
            0f, 0.5f, 0.75f, 1f, 1.5f, 2f, 3f, 4f, 5f, 6f, 7f, 8f
    };
    private PianoView pianoView;
    private MidiController midiController;
    private volatile boolean dreamy = true;
    private int boosterStep = 0;
    private int spaceMode = 0;
    private boolean tape = false;
    private int boostDb = 0;
    private int spaceMix = 50, spaceDecay = 50;
    private int tapeWow = 50, tapeFlutter = 50, tapeDrive = 50;
    private int dreamX = 28, dreamY = 28, dreamMix = 34;
    private int midiChannel = 0;
    private boolean manualSustain = false;
    private float audioBufferBursts = 0f;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemUI();
        pianoView = new PianoView(this);
        pianoView.setActionListener(this);
        setContentView(pianoView);

        boosterStep = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_BOOST, 0);
        spaceMode = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_SPACE, 0);
        tape = getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_TAPE, false);
        dreamy = getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_DREAMY, true);
        boostDb = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_BOOST_DB, boosterStep * 2);
        spaceMix = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_SPACE_MIX, 50);
        spaceDecay = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_SPACE_DECAY, 50);
        tapeWow = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_TAPE_WOW, 50);
        tapeFlutter = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_TAPE_FLUTTER, 50);
        tapeDrive = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_TAPE_DRIVE, 50);
        dreamX = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_X, 28);
        dreamY = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_Y, 28);
        dreamMix = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_MIX, 34);
        midiChannel = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_MIDI_CHANNEL, 0);
        manualSustain = getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_MANUAL_SUSTAIN, false);
        audioBufferBursts = getSharedPreferences(PREFS, MODE_PRIVATE).getFloat(KEY_AUDIO_BUFFER_BURSTS, 0f);

        pianoView.setBoosterStep(boosterStep);
        pianoView.setBoostDb(boostDb);
        pianoView.setSpaceMode(spaceMode);
        pianoView.setTape(tape);
        pianoView.setDreamy(dreamy);

        NativeEngine.start();
        NativeEngine.setAudioBufferBursts(audioBufferBursts);
        NativeEngine.setBoosterStep(boosterStep);
        NativeEngine.setBoostDb(boostDb);
        NativeEngine.setSpaceMode(spaceMode);
        NativeEngine.setSpaceParameters(spaceMix, spaceDecay);
        NativeEngine.setTape(tape);
        NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
        NativeEngine.setDreamy(dreamy);
        NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
        if (manualSustain) {
            NativeEngine.controlChange(64, 127);
            pianoView.controlChange(64, 127);
        }
        loadExistingBank();

        midiController = new MidiController(this, this);
        midiController.setChannel(midiChannel);
        midiController.start();
    }

    private void hideSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        if (android.os.Build.VERSION.SDK_INT >= 30 && getWindow().getInsetsController() != null) {
            getWindow().getInsetsController().hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
            getWindow().getInsetsController().setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        }
    }

    private void loadExistingBank() {
        String saved = getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_BANK_URI, null);
        if (saved == null || saved.isEmpty()) {
            pianoView.setBankStatus("BANK —");
            return;
        }
        loadBankUri(Uri.parse(saved));
    }

    private void loadBankUri(Uri uri) {
        pianoView.setBankStatus("BANK LOADING…");
        new Thread(() -> {
            boolean ok = false;
            try (ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r")) {
                if (pfd != null) ok = NativeEngine.loadBankFd(pfd.getFd());
            } catch (Exception ignored) {
            }
            final boolean result = ok;
            runOnUiThread(() -> pianoView.setBankStatus(result ? NativeEngine.bankStatus() : "BANK ERROR"));
        }, "BankOpen").start();
    }

    @Override public void onToggleDreamy() {
        dreamy = !dreamy;
        NativeEngine.setDreamy(dreamy);
        pianoView.setDreamy(dreamy);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(KEY_DREAMY, dreamy).apply();
    }

    @Override public void onCycleBooster() {
        boosterStep = (boosterStep + 1) % 4;
        boostDb = boosterStep * 2;
        NativeEngine.setBoosterStep(boosterStep);
        NativeEngine.setBoostDb(boostDb);
        pianoView.setBoosterStep(boosterStep);
        pianoView.setBoostDb(boostDb);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt(KEY_BOOST, boosterStep).putInt(KEY_BOOST_DB, boostDb).apply();
    }

    @Override public void onCycleSpace() {
        spaceMode = (spaceMode + 1) % 4;
        NativeEngine.setSpaceMode(spaceMode);
        pianoView.setSpaceMode(spaceMode);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_SPACE, spaceMode).apply();
    }

    @Override public void onToggleTape() {
        tape = !tape;
        NativeEngine.setTape(tape);
        pianoView.setTape(tape);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(KEY_TAPE, tape).apply();
    }

    @Override public void onEditEffect(int effect) {
        if (effect == PianoView.EFFECT_BOOST) showBoostDialog();
        else if (effect == PianoView.EFFECT_SPACE) showSpaceDialog();
        else if (effect == PianoView.EFFECT_TAPE) showTapeDialog();
        else if (effect == PianoView.EFFECT_DREAMY) showDreamyDialog();
    }

    @Override public void onOpenConfig() {
        showConfigDialog();
    }

    private LinearLayout dialogRoot() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int p = Math.round(18f * getResources().getDisplayMetrics().density);
        root.setPadding(p, p / 2, p, p / 2);
        return root;
    }

    private void addSlider(LinearLayout root, String name, int max, int value, java.util.function.IntConsumer onChange) {
        TextView label = new TextView(this);
        label.setText(name + "  " + value);
        label.setTextSize(16f);
        root.addView(label);
        SeekBar bar = new SeekBar(this);
        bar.setMax(max);
        bar.setProgress(Math.max(0, Math.min(max, value)));
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                label.setText(name + "  " + progress);
                if (fromUser) onChange.accept(progress);
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override public void onStopTrackingTouch(SeekBar seekBar) { }
        });
        root.addView(bar);
    }

    private void showBoostDialog() {
        LinearLayout root = dialogRoot();
        addSlider(root, "BOOST dB", 6, boostDb, v -> {
            boostDb = v;
            NativeEngine.setBoostDb(v);
            pianoView.setBoostDb(v);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_BOOST_DB, v).apply();
        });
        new AlertDialog.Builder(this).setTitle("BOOST").setView(root)
                .setPositiveButton("CLOSE", null).show();
    }

    private void showSpaceDialog() {
        LinearLayout root = dialogRoot();
        addSlider(root, "MIX %", 100, spaceMix, v -> {
            spaceMix = v;
            NativeEngine.setSpaceParameters(spaceMix, spaceDecay);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_SPACE_MIX, v).apply();
        });
        addSlider(root, "DECAY %", 100, spaceDecay, v -> {
            spaceDecay = v;
            NativeEngine.setSpaceParameters(spaceMix, spaceDecay);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_SPACE_DECAY, v).apply();
        });
        new AlertDialog.Builder(this).setTitle("SPACE").setView(root)
                .setPositiveButton("CLOSE", null).show();
    }

    private void showTapeDialog() {
        LinearLayout root = dialogRoot();
        addSlider(root, "WOW %", 100, tapeWow, v -> {
            tapeWow = v;
            NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_TAPE_WOW, v).apply();
        });
        addSlider(root, "FLUTTER %", 100, tapeFlutter, v -> {
            tapeFlutter = v;
            NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_TAPE_FLUTTER, v).apply();
        });
        addSlider(root, "DRIVE %", 100, tapeDrive, v -> {
            tapeDrive = v;
            NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_TAPE_DRIVE, v).apply();
        });
        new AlertDialog.Builder(this).setTitle("TAPE").setView(root)
                .setPositiveButton("CLOSE", null).show();
    }

    private void showDreamyDialog() {
        LinearLayout root = dialogRoot();
        addSlider(root, "X %", 100, dreamX, v -> {
            dreamX = v;
            NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_DREAM_X, v).apply();
        });
        addSlider(root, "Y %", 100, dreamY, v -> {
            dreamY = v;
            NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_DREAM_Y, v).apply();
        });
        addSlider(root, "MIX %", 100, dreamMix, v -> {
            dreamMix = v;
            NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_DREAM_MIX, v).apply();
        });
        new AlertDialog.Builder(this).setTitle("DREAMY").setView(root)
                .setPositiveButton("CLOSE", null).show();
    }

    private int audioBufferSelection(float value) {
        int best = 0;
        float bestDiff = Float.MAX_VALUE;
        for (int i=0;i<AUDIO_BUFFER_VALUES.length;i++) {
            float d = Math.abs(AUDIO_BUFFER_VALUES[i] - value);
            if (d < bestDiff) { bestDiff = d; best = i; }
        }
        return best;
    }

    private String audioBufferStats(int lastResult) {
        int fpb = NativeEngine.audioFramesPerBurst();
        int frames = NativeEngine.audioBufferSizeFrames();
        int capacity = NativeEngine.audioBufferCapacityFrames();
        int xruns = NativeEngine.audioXRunCount();
        String actual = fpb > 0
                ? String.format(java.util.Locale.US, "%.2f", frames / (float) fpb)
                : "N/A";
        String result = lastResult < 0 ? "\nRequest result: " + lastResult : "";
        return "Frames per burst: " + fpb +
                "\nActual buffer: " + frames + " frames" +
                "\nActual bursts: " + actual +
                "\nCapacity: " + capacity + " frames" +
                "\nXRuns: " + Math.max(0, xruns) + result;
    }

    private void showConfigDialog() {
        LinearLayout root = dialogRoot();

        TextView audioLabel = new TextView(this);
        audioLabel.setText("AUDIO BUFFER");
        audioLabel.setTextSize(16f);
        root.addView(audioLabel);

        Spinner audioSpinner = new Spinner(this);
        audioSpinner.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item, AUDIO_BUFFER_LABELS));
        final float originalAudioBuffer = audioBufferBursts;
        final int[] lastAudioResult = { 0 };
        audioSpinner.setSelection(audioBufferSelection(audioBufferBursts));
        root.addView(audioSpinner);

        TextView audioStats = new TextView(this);
        audioStats.setTextSize(13.5f);
        audioStats.setPadding(0, 0, 0, 14);
        audioStats.setText(audioBufferStats(0));
        root.addView(audioStats);

        audioSpinner.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> parent, View view,
                                                 int position, long id) {
                float requested = AUDIO_BUFFER_VALUES[Math.max(0,
                        Math.min(AUDIO_BUFFER_VALUES.length - 1, position))];
                lastAudioResult[0] = NativeEngine.setAudioBufferBursts(requested);
                audioStats.setText(audioBufferStats(lastAudioResult[0]));
            }
            @Override public void onNothingSelected(android.widget.AdapterView<?> parent) { }
        });

        TextView chLabel = new TextView(this);
        chLabel.setText("MIDI CHANNEL");
        chLabel.setTextSize(16f);
        root.addView(chLabel);

        Spinner spinner = new Spinner(this);
        String[] channels = new String[17];
        channels[0] = "OMNI";
        for (int i=1;i<=16;i++) channels[i] = "CH " + i;
        spinner.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, channels));
        spinner.setSelection(Math.max(0, Math.min(16, midiChannel)));
        root.addView(spinner);

        CheckBox sustain = new CheckBox(this);
        sustain.setText("Manual SUSTAIN");
        sustain.setChecked(manualSustain);
        root.addView(sustain);

        TextView cc = new TextView(this);
        cc.setText("\nMIDI CC\n" +
                "7 Volume / 11 Expression / 64 Sustain\n" +
                "20 Boost / 21 Space Mode / 22 Space Mix / 23 Space Decay\n" +
                "24 Tape On-Off / 25 Wow / 26 Flutter / 27 Drive\n" +
                "28 Dreamy On-Off / 103 Dreamy X / 104 Dreamy Y / 105 Dreamy Mix");
        cc.setTextSize(14f);
        root.addView(cc);

        android.widget.ScrollView scroll = new android.widget.ScrollView(this);
        scroll.addView(root);

        final AlertDialog[] holder = new AlertDialog[1];
        AlertDialog dialog = new AlertDialog.Builder(this).setTitle("CONFIG").setView(scroll)
                .setPositiveButton("APPLY", (d, which) -> {
                    int ai = Math.max(0, Math.min(AUDIO_BUFFER_VALUES.length - 1,
                            audioSpinner.getSelectedItemPosition()));
                    audioBufferBursts = AUDIO_BUFFER_VALUES[ai];
                    NativeEngine.setAudioBufferBursts(audioBufferBursts);

                    midiChannel = spinner.getSelectedItemPosition();
                    if (midiController != null) midiController.setChannel(midiChannel);
                    manualSustain = sustain.isChecked();
                    int sus = manualSustain ? 127 : 0;
                    NativeEngine.controlChange(64, sus);
                    pianoView.controlChange(64, sus);
                    getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                            .putFloat(KEY_AUDIO_BUFFER_BURSTS, audioBufferBursts)
                            .putInt(KEY_MIDI_CHANNEL, midiChannel)
                            .putBoolean(KEY_MANUAL_SUSTAIN, manualSustain).apply();
                })
                .setNegativeButton("CANCEL", (d, which) -> {
                    NativeEngine.setAudioBufferBursts(originalAudioBuffer);
                }).create();
        holder[0] = dialog;

        final Runnable refreshStats = new Runnable() {
            @Override public void run() {
                AlertDialog current = holder[0];
                if (current == null || !current.isShowing()) return;
                audioStats.setText(audioBufferStats(lastAudioResult[0]));
                audioStats.postDelayed(this, 500L);
            }
        };
        dialog.setOnShowListener(d -> audioStats.post(refreshStats));
        dialog.setOnCancelListener(d -> NativeEngine.setAudioBufferBursts(originalAudioBuffer));
        dialog.show();
    }

    @Override public void onRecorderRecord() {
        NativeEngine.recorderToggleRecording();
        pianoView.invalidate();
    }

    @Override public void onRecorderClear() {
        NativeEngine.recorderClear();
        pianoView.invalidate();
    }

    @Override public void onRecorderRandom() {
        NativeEngine.recorderToggleRandom();
        pianoView.invalidate();
    }

    @Override public void onRecorderClock() {
        NativeEngine.recorderToggleClock();
        pianoView.invalidate();
    }

    @Override public void onRecorderBpm(int bpm) {
        NativeEngine.recorderSetBpm(bpm);
        pianoView.invalidate();
    }

    @Override public void onRecorderTile(int slot) {
        if (NativeEngine.recorderIsRecording()
                && slot != NativeEngine.recorderRecordingSlot()
                && NativeEngine.recorderValidSamples(slot) == 0) {
            NativeEngine.recorderRecordSlot(slot);
        } else {
            NativeEngine.recorderPlaySlot(slot);
        }
        pianoView.invalidate();
    }

    @Override public void onChooseBank() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/octet-stream", "application/x-binary", "*/*"});
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, PICK_BANK);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_BANK || resultCode != RESULT_OK || data == null) return;
        Uri uri = data.getData();
        if (uri == null) return;
        try {
            final int flags = data.getFlags() &
                    (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            getContentResolver().takePersistableUriPermission(uri, flags & Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException ignored) {
        }
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString(KEY_BANK_URI, uri.toString()).apply();
        loadBankUri(uri);
    }

    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_SELECT) {
            if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0)
                pianoView.toggleRecorderDrawer();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override protected void onDestroy() {
        if (midiController != null) midiController.stop();
        NativeEngine.stop();
        super.onDestroy();
    }

    @Override public void onNoteOn(int note, int velocity) { runOnUiThread(() -> pianoView.noteOn(note, velocity)); }
    @Override public void onNoteOff(int note, int velocity) { runOnUiThread(() -> pianoView.noteOff(note)); }
    @Override public void onPolyPressure(int note, int value) { runOnUiThread(() -> pianoView.polyPressure(note, value)); }
    @Override public void onChannelPressure(int value) { runOnUiThread(() -> pianoView.setChannelPressure(value)); }
    @Override public void onControlChange(int cc, int value) {
        runOnUiThread(() -> {
            pianoView.controlChange(cc, value);
            if (cc == 20) { boostDb = Math.round(value * 6f / 127f); pianoView.setBoostDb(boostDb); }
            else if (cc == 21) { spaceMode = Math.round(value * 3f / 127f); pianoView.setSpaceMode(spaceMode); }
            else if (cc == 22) spaceMix = Math.round(value * 100f / 127f);
            else if (cc == 23) spaceDecay = Math.round(value * 100f / 127f);
            else if (cc == 24) { tape = value >= 64; pianoView.setTape(tape); }
            else if (cc == 25) tapeWow = Math.round(value * 100f / 127f);
            else if (cc == 26) tapeFlutter = Math.round(value * 100f / 127f);
            else if (cc == 27) tapeDrive = Math.round(value * 100f / 127f);
            else if (cc == 28) { dreamy = value >= 64; pianoView.setDreamy(dreamy); }
            else if (cc == 64) manualSustain = value >= 64;
            else if (cc == 103) dreamX = Math.round(value * 100f / 127f);
            else if (cc == 104) dreamY = Math.round(value * 100f / 127f);
            else if (cc == 105) dreamMix = Math.round(value * 100f / 127f);
        });
    }
    @Override public void onPitchBend(int value14) { runOnUiThread(() -> pianoView.setPitchBend(value14)); }
    @Override public void onConnectionCountChanged(int count) { runOnUiThread(() -> pianoView.setMidiConnections(count)); }
}
