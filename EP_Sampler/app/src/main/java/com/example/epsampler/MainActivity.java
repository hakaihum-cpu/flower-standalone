package com.example.epsampler;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class MainActivity extends Activity implements MidiController.Listener, PianoView.ActionListener {
    private static final int PICK_BANK = 1001;
    private PianoView pianoView;
    private MidiController midiController;
    private volatile boolean dreamy = true;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemUI();
        pianoView = new PianoView(this);
        pianoView.setActionListener(this);
        setContentView(pianoView);

        NativeEngine.start();
        NativeEngine.setDreamy(true);
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

    private File bankFile() { return new File(getFilesDir(), "epbank.bin"); }

    private void loadExistingBank() {
        File f = bankFile();
        if (f.isFile() && NativeEngine.loadBank(f.getAbsolutePath())) {
            pianoView.setBankStatus("BANK READY");
        } else {
            pianoView.setBankStatus("BANK —");
        }
    }

    @Override public void onToggleDreamy() {
        dreamy = !dreamy;
        NativeEngine.setDreamy(dreamy);
        pianoView.setDreamy(dreamy);
    }

    @Override public void onChooseBank() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/octet-stream", "application/x-binary", "*/*"});
        startActivityForResult(intent, PICK_BANK);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_BANK || resultCode != RESULT_OK || data == null) return;
        Uri uri = data.getData();
        if (uri == null) return;
        pianoView.setBankStatus("BANK COPYING…");
        new Thread(() -> {
            boolean ok = false;
            File tmp = new File(getFilesDir(), "epbank.tmp");
            try (InputStream in = getContentResolver().openInputStream(uri);
                 FileOutputStream out = new FileOutputStream(tmp)) {
                if (in == null) throw new IllegalStateException("No input stream");
                byte[] buffer = new byte[1024 * 1024];
                int n;
                while ((n = in.read(buffer)) > 0) out.write(buffer, 0, n);
                out.getFD().sync();
                File dst = bankFile();
                if (dst.exists()) dst.delete();
                ok = tmp.renameTo(dst) && NativeEngine.loadBank(dst.getAbsolutePath());
            } catch (Exception ignored) {
                tmp.delete();
            }
            final boolean result = ok;
            runOnUiThread(() -> pianoView.setBankStatus(result ? "BANK READY" : "BANK ERROR"));
        }, "BankImport").start();
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
