package com.example.epsampler;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.Settings;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;


public class MainActivity extends Activity implements MidiController.Listener, PianoView.ActionListener {
    private static final int PICK_BANK = 1001;
    private static final String PREFS = "ep_sampler";
    private static final String KEY_BANK_URI = "bank_uri";
    private static final String KEY_BOOST = "boost_step";
    private static final String KEY_SPACE = "space_mode";
    private static final String KEY_TAPE = "tape_on";
    private static final String KEY_DREAMY = "dreamy_on";
    private PianoView pianoView;
    private MidiController midiController;
    private volatile boolean dreamy = true;
    private int boosterStep = 0;
    private int spaceMode = 0;
    private boolean tape = false;

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

        pianoView.setBoosterStep(boosterStep);
        pianoView.setSpaceMode(spaceMode);
        pianoView.setTape(tape);
        pianoView.setDreamy(dreamy);

        NativeEngine.start();
        NativeEngine.setBoosterStep(boosterStep);
        NativeEngine.setSpaceMode(spaceMode);
        NativeEngine.setTape(tape);
        NativeEngine.setDreamy(dreamy);
        loadExistingBank();

        midiController = new MidiController(this, this);
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
        NativeEngine.setBoosterStep(boosterStep);
        pianoView.setBoosterStep(boosterStep);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_BOOST, boosterStep).apply();
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

    @Override protected void onDestroy() {
        if (midiController != null) midiController.stop();
        NativeEngine.stop();
        super.onDestroy();
    }

    @Override public void onNoteOn(int note, int velocity) { runOnUiThread(() -> pianoView.noteOn(note, velocity)); }
    @Override public void onNoteOff(int note, int velocity) { runOnUiThread(() -> pianoView.noteOff(note)); }
    @Override public void onPolyPressure(int note, int value) { runOnUiThread(() -> pianoView.polyPressure(note, value)); }
    @Override public void onChannelPressure(int value) { runOnUiThread(() -> pianoView.setChannelPressure(value)); }
    @Override public void onControlChange(int cc, int value) { runOnUiThread(() -> pianoView.controlChange(cc, value)); }
    @Override public void onPitchBend(int value14) { runOnUiThread(() -> pianoView.setPitchBend(value14)); }
    @Override public void onConnectionCountChanged(int count) { runOnUiThread(() -> pianoView.setMidiConnections(count)); }
}
